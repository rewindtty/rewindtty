#include "recorder.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stddef.h>
#include <pty.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>

#define BUF_SIZE 8192
#define MAX_CHUNKS_PER_COMMAND 1000

static FILE *fp = NULL;
static FILE *timing_fp = NULL;
static int first = 1;
static int child_running = 0;
static pid_t current_child_pid = 0;

typedef struct
{
    double timestamp;
    size_t data_length;
    char *data;
} TTYChunk;

typedef struct
{
    char command[1024];
    double start_time;
    double end_time;
    TTYChunk *chunks;
    size_t chunk_count;
    size_t chunk_capacity;
} TTYSession;

double get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

TTYSession *create_tty_session(const char *command)
{
    TTYSession *session = malloc(sizeof(TTYSession));
    strncpy(session->command, command, sizeof(session->command) - 1);
    session->command[sizeof(session->command) - 1] = '\0';
    session->start_time = get_timestamp();
    session->end_time = 0;
    session->chunks = malloc(sizeof(TTYChunk) * 100);
    session->chunk_count = 0;
    session->chunk_capacity = 100;
    return session;
}

void add_chunk_to_session(TTYSession *session, double timestamp, const char *data, size_t length)
{
    if (session->chunk_count >= session->chunk_capacity)
    {
        session->chunk_capacity *= 2;
        session->chunks = realloc(session->chunks, sizeof(TTYChunk) * session->chunk_capacity);
    }

    TTYChunk *chunk = &session->chunks[session->chunk_count];
    chunk->timestamp = timestamp;
    chunk->data_length = length;
    chunk->data = malloc(length + 1);
    memcpy(chunk->data, data, length);
    chunk->data[length] = '\0';

    session->chunk_count++;
}

void finish_tty_session(TTYSession *session)
{
    session->end_time = get_timestamp();
}

char *create_json_tty_session(TTYSession *session)
{
    cJSON *json_session = cJSON_CreateObject();
    cJSON *json_chunks = cJSON_CreateArray();

    cJSON_AddStringToObject(json_session, "command", session->command);
    cJSON_AddNumberToObject(json_session, "start_time", session->start_time);
    cJSON_AddNumberToObject(json_session, "end_time", session->end_time);
    cJSON_AddNumberToObject(json_session, "duration", session->end_time - session->start_time);

    for (size_t i = 0; i < session->chunk_count; i++)
    {
        cJSON *chunk = cJSON_CreateObject();
        double relative_time = session->chunks[i].timestamp - session->start_time;

        cJSON_AddNumberToObject(chunk, "time", relative_time);
        cJSON_AddNumberToObject(chunk, "size", (double)session->chunks[i].data_length);
        cJSON_AddStringToObject(chunk, "data", session->chunks[i].data);

        cJSON_AddItemToArray(json_chunks, chunk);
    }

    cJSON_AddItemToObject(json_session, "chunks", json_chunks);

    char *json_string = cJSON_Print(json_session);
    cJSON_Delete(json_session);

    return json_string;
}

void free_tty_session(TTYSession *session)
{
    if (!session)
        return;

    for (size_t i = 0; i < session->chunk_count; i++)
    {
        free(session->chunks[i].data);
    }
    free(session->chunks);
    free(session);
}

TTYSession *exec_and_capture_pty_realtime(
    const char *command,
    const char *shell_path,
    int *child_running,
    pid_t *current_child_pid)
{
    int master_fd;
    pid_t pid;
    struct termios term_attrs, raw_attrs;
    TTYSession *session = create_tty_session(command);

    if (tcgetattr(STDIN_FILENO, &term_attrs) != 0)
    {
        perror("tcgetattr");
        free_tty_session(session);
        return NULL;
    }

    pid = forkpty(&master_fd, NULL, &term_attrs, NULL);
    if (pid == -1)
    {
        perror("forkpty");
        free_tty_session(session);
        return NULL;
    }

    *current_child_pid = pid;
    *child_running = 1;

    if (pid == 0)
    {
        // Child process
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);

        execl(shell_path, shell_path, "-c", command, (char *)NULL);
        perror("execl");
        exit(1);
    }
    else
    {
        // Parent process
        raw_attrs = term_attrs;
        cfmakeraw(&raw_attrs);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_attrs);

        char buffer[BUF_SIZE];
        ssize_t n;
        int status;
        fd_set read_fds;
        int stdin_fd = STDIN_FILENO;
        int max_fd = (master_fd > stdin_fd) ? master_fd : stdin_fd;

        // Set master_fd to non-blocking
        int flags = fcntl(master_fd, F_GETFL);
        fcntl(master_fd, F_SETFL, flags | O_NONBLOCK);

        while (*child_running)
        {
            FD_ZERO(&read_fds);
            FD_SET(master_fd, &read_fds);
            FD_SET(stdin_fd, &read_fds);

            struct timeval timeout = {0, 10000}; // 10ms timeout
            int select_result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

            if (select_result > 0)
            {
                if (FD_ISSET(master_fd, &read_fds))
                {
                    n = read(master_fd, buffer, BUF_SIZE - 1);
                    if (n > 0)
                    {
                        double timestamp = get_timestamp();

                        // Write to terminal for live view
                        write(STDOUT_FILENO, buffer, n);

                        // Record the chunk with precise timing
                        add_chunk_to_session(session, timestamp, buffer, n);
                    }
                    else if (n == 0)
                    {
                        break;
                    }
                    else if (errno != EAGAIN && errno != EWOULDBLOCK)
                    {
                        break;
                    }
                }

                if (FD_ISSET(stdin_fd, &read_fds))
                {
                    n = read(stdin_fd, buffer, BUF_SIZE - 1);
                    if (n > 0)
                    {
                        write(master_fd, buffer, n);
                    }
                    else if (n == 0)
                    {
                        break;
                    }
                }
            }

            // Check if child has terminated
            if (waitpid(pid, &status, WNOHANG) > 0)
            {
                *child_running = 0;
                break;
            }
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &term_attrs);

        if (*child_running)
        {
            waitpid(pid, &status, 0);
        }

        close(master_fd);
        *child_running = 0;
        *current_child_pid = 0;

        finish_tty_session(session);
        return session;
    }
}

void close_session_file(void)
{
    if (fp)
    {
        fprintf(fp, "\n]\n");
        fclose(fp);
        fp = NULL;
    }
}

void signal_handler(int signal)
{
    if (signal == SIGINT && child_running && current_child_pid > 0)
    {
        kill(current_child_pid, SIGINT);
        return;
    }

    printf("\n[!] Signal received (%d), closing session file...\n", signal);
    close_session_file();
    _exit(1);
}

void start_recording(const char *filename)
{
    char command[1024];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    fp = fopen(filename, "w");
    if (!fp)
    {
        perror("fopen");
        return;
    }

    fprintf(fp, "[\n");
    first = 1;

    printf("TTY Real-time Recorder started. Type 'exit' to quit.\n");

    while (1)
    {
        printf("rewindtty> ");
        fflush(stdout);

        if (!fgets(command, sizeof(command), stdin))
            break;

        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0)
            break;

        const char *shell_path = getenv("SHELL");
        if (!shell_path)
            shell_path = "/bin/sh";

        printf("Recording command: %s\n", command);
        printf("Press Ctrl+C to interrupt the command, 'exit' to quit recording.\n");

        TTYSession *session = exec_and_capture_pty_realtime(
            command,
            shell_path,
            &child_running,
            &current_child_pid);

        if (session)
        {
            if (!first)
                fprintf(fp, ",\n");
            first = 0;

            char *json_session = create_json_tty_session(session);
            fprintf(fp, "%s", json_session);
            fflush(fp);

            free(json_session);
            free_tty_session(session);
        }
    }

    close_session_file();
    printf("Recording session saved to: %s\n", filename);
}