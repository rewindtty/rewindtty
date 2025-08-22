#include "recorder.h"
#include "uploader.h"
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

static int first = 1;
static int child_running = 0;
static pid_t current_child_pid = 0;

// Structure to hold session data for final JSON creation

static SessionData *global_session_data = NULL;
static char *current_filename = NULL;

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

SessionData *create_session_data(int interactive_mode)
{
    SessionData *data = malloc(sizeof(SessionData));
    data->sessions = malloc(sizeof(TTYSession *) * 10);
    data->session_count = 0;
    data->session_capacity = 10;
    data->interactive_mode = interactive_mode;
    data->start_timestamp = get_timestamp();
    return data;
}

void add_session_to_data(SessionData *data, TTYSession *session)
{
    if (data->session_count >= data->session_capacity)
    {
        data->session_capacity *= 2;
        data->sessions = realloc(data->sessions, sizeof(TTYSession *) * data->session_capacity);
    }
    data->sessions[data->session_count++] = session;
}

void write_sessions_to_file(const char *filename, SessionData *data)
{
    write_sessions_to_file_with_upload(filename, data, 0, NULL);
}

void write_sessions_to_file_with_upload(const char *filename, SessionData *data, int upload_enabled, const char *upload_url)
{
    cJSON *root = cJSON_CreateObject();

    // Create metadata
    cJSON *metadata = cJSON_CreateObject();
    cJSON_AddStringToObject(metadata, "version", REWINDTTY_VERSION);
    cJSON_AddBoolToObject(metadata, "interactive_mode", data->interactive_mode);
    cJSON_AddNumberToObject(metadata, "timestamp", data->start_timestamp);
    cJSON_AddItemToObject(root, "metadata", metadata);

    // Create sessions array
    cJSON *sessions_array = cJSON_CreateArray();
    for (size_t i = 0; i < data->session_count; i++)
    {
        TTYSession *session = data->sessions[i];
        cJSON *session_obj = cJSON_CreateObject();

        cJSON_AddStringToObject(session_obj, "command", session->command);
        cJSON_AddNumberToObject(session_obj, "start_time", session->start_time);
        cJSON_AddNumberToObject(session_obj, "end_time", session->end_time);
        cJSON_AddNumberToObject(session_obj, "duration", session->end_time - session->start_time);

        cJSON *chunks_array = cJSON_CreateArray();
        for (size_t j = 0; j < session->chunk_count; j++)
        {
            cJSON *chunk = cJSON_CreateObject();
            double relative_time = session->chunks[j].timestamp - session->start_time;

            cJSON_AddNumberToObject(chunk, "time", relative_time);
            cJSON_AddNumberToObject(chunk, "size", (double)session->chunks[j].data_length);
            cJSON_AddStringToObject(chunk, "data", session->chunks[j].data);

            cJSON_AddItemToArray(chunks_array, chunk);
        }
        cJSON_AddItemToObject(session_obj, "chunks", chunks_array);

        cJSON_AddItemToArray(sessions_array, session_obj);
    }
    cJSON_AddItemToObject(root, "sessions", sessions_array);

    char *json_string = cJSON_Print(root);

    // Write to file
    FILE *file = fopen(filename, "w");
    if (file && json_string)
    {
        fprintf(file, "%s\n", json_string);
        fclose(file);
    }

    // Upload if enabled
    if (upload_enabled && upload_url && json_string)
    {
        if (upload_session_data(json_string, upload_url))
        {
            printf("Session uploaded successfully!\n");
        }
        else
        {
            printf("Failed to upload session data.\n");
        }
    }

    if (json_string)
    {
        free(json_string);
    }
    cJSON_Delete(root);
}

void free_session_data(SessionData *data)
{
    if (!data)
        return;

    for (size_t i = 0; i < data->session_count; i++)
    {
        free_tty_session(data->sessions[i]);
    }
    free(data->sessions);
    free(data);
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

void signal_handler(int signal)
{
    if (signal == SIGINT && child_running && current_child_pid > 0)
    {
        kill(current_child_pid, SIGINT);
        return;
    }

    printf("\n[!] Signal received (%d), saving session file...\n", signal);

    // Save session data if available
    if (global_session_data)
    {
        const char *filename = current_filename ? current_filename : "emergency_session.json";
        write_sessions_to_file(filename, global_session_data);
        free_session_data(global_session_data);
        global_session_data = NULL;
    }

    if (current_filename)
    {
        free(current_filename);
        current_filename = NULL;
    }

    _exit(1);
}

// Initialize input buffer
InputBuffer *create_input_buffer()
{
    InputBuffer *buf = malloc(sizeof(InputBuffer));
    buf->capacity = 1024;
    buf->buffer = malloc(buf->capacity);
    buf->size = 0;
    return buf;
}

void append_to_buffer(InputBuffer *buf, const char *data, size_t len)
{
    if (buf->size + len >= buf->capacity)
    {
        buf->capacity = (buf->size + len) * 2;
        buf->buffer = realloc(buf->buffer, buf->capacity);
    }
    memcpy(buf->buffer + buf->size, data, len);
    buf->size += len;
    buf->buffer[buf->size] = '\0';
}

void free_input_buffer(InputBuffer *buf)
{
    if (buf)
    {
        free(buf->buffer);
        free(buf);
    }
}
// Detect if data contains a shell prompt
int detect_shell_prompt(const char *data, size_t len)
{
    // Look for common prompt patterns: $, #, >, %
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] == '$' || data[i] == '#' || data[i] == '%')
        {
            // Check if it's followed by space (likely a prompt)
            if (i + 1 < len && data[i + 1] == ' ')
            {
                return 1;
            }
        }
        // Look for "> " pattern
        if (i + 1 < len && data[i] == '>' && data[i + 1] == ' ')
        {
            return 1;
        }
    }
    return 0;
}

// Clean command string by removing control characters
void clean_command_string(char *cmd)
{
    char *src = cmd;
    char *dst = cmd;

    while (*src)
    {
        if (*src >= 32 && *src < 127)
        { // Printable ASCII
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    // Trim trailing whitespace
    while (dst > cmd && (*(dst - 1) == ' ' || *(dst - 1) == '\t'))
    {
        *(--dst) = '\0';
    }
}

void start_interactive_recording(const char *filename)
{
    start_interactive_recording_with_upload(filename, 0, NULL);
}

void start_interactive_recording_with_upload(const char *filename, int upload_enabled, const char *upload_url)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    // Initialize session data
    global_session_data = create_session_data(1); // interactive mode
    current_filename = strdup(filename);

    const char *shell_path = getenv("SHELL");
    if (!shell_path)
        shell_path = "/bin/bash";

    printf("Interactive TTY recording started. Use your shell normally.\n");
    printf("Press Ctrl+D or type 'exit' to stop recording.\n");

    int master_fd;
    pid_t pid;
    struct termios term_attrs, raw_attrs;

    if (tcgetattr(STDIN_FILENO, &term_attrs) != 0)
    {
        perror("tcgetattr");
        return;
    }

    pid = forkpty(&master_fd, NULL, &term_attrs, NULL);
    if (pid == -1)
    {
        perror("forkpty");
        return;
    }

    current_child_pid = pid;
    child_running = 1;

    if (pid == 0)
    {
        // Child process: start interactive shell
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);

        execl(shell_path, shell_path, "-i", (char *)NULL);
        perror("execl");
        exit(1);
    }
    else
    {
        // Parent process: transparent recording
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

        // Command detection variables
        InputBuffer *input_buf = create_input_buffer();
        InputBuffer *output_buf = create_input_buffer();
        TTYSession *current_session = NULL;
        int in_command = 0;
        int waiting_for_prompt = 1;

        while (child_running)
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

                        // Track output for prompt detection
                        append_to_buffer(output_buf, buffer, n);

                        // Check for shell prompt in output
                        if (waiting_for_prompt && detect_shell_prompt(buffer, n))
                        {
                            waiting_for_prompt = 0;
                        }

                        // Add to current session if we have one
                        if (current_session)
                        {
                            add_chunk_to_session(current_session, timestamp, buffer, n);
                        }
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

                        // Track input for command detection
                        append_to_buffer(input_buf, buffer, n);

                        // Start new command session after seeing a prompt and getting input
                        if (!waiting_for_prompt && !in_command && n > 0)
                        {
                            // Finish previous session if exists
                            if (current_session)
                            {
                                finish_tty_session(current_session);
                                add_session_to_data(global_session_data, current_session);
                                current_session = NULL;
                            }

                            // Create command from accumulated input
                            char command_str[1024] = {0};
                            size_t copy_len = input_buf->size < sizeof(command_str) - 1 ? input_buf->size : sizeof(command_str) - 1;
                            memcpy(command_str, input_buf->buffer, copy_len);
                            command_str[copy_len] = '\0';
                            clean_command_string(command_str);

                            // Start new session
                            current_session = create_tty_session(command_str);
                            in_command = 1;
                        }

                        // Detect command end (Enter pressed)
                        if (in_command)
                        {
                            in_command = 0;
                            waiting_for_prompt = 1;

                            // Reset input buffer for next command
                            input_buf->size = 0;
                            if (input_buf->buffer)
                                input_buf->buffer[0] = '\0';

                            // Reset output buffer
                            output_buf->size = 0;
                            if (output_buf->buffer)
                                output_buf->buffer[0] = '\0';
                        }
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
                child_running = 0;
                break;
            }
        }

        // Cleanup
        tcsetattr(STDIN_FILENO, TCSANOW, &term_attrs);

        if (current_session)
        {
            finish_tty_session(current_session);
            add_session_to_data(global_session_data, current_session);
        }

        if (child_running)
        {
            waitpid(pid, &status, 0);
        }

        close(master_fd);
        child_running = 0;
        current_child_pid = 0;

        free_input_buffer(input_buf);
        free_input_buffer(output_buf);
    }

    // Write final JSON file with optional upload
    write_sessions_to_file_with_upload(filename, global_session_data, upload_enabled, upload_url);
    free_session_data(global_session_data);
    global_session_data = NULL;

    if (current_filename)
    {
        free(current_filename);
        current_filename = NULL;
    }

    printf("\nInteractive recording session saved to: %s\n", filename);
}

void start_recording(const char *filename)
{
    start_recording_with_upload(filename, 0, NULL);
}

void start_recording_with_upload(const char *filename, int upload_enabled, const char *upload_url)
{
    char command[1024];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

    // Initialize session data
    global_session_data = create_session_data(0); // non-interactive mode
    current_filename = strdup(filename);

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
            add_session_to_data(global_session_data, session);
        }
    }

    // Write final JSON file with optional upload
    write_sessions_to_file_with_upload(filename, global_session_data, upload_enabled, upload_url);
    free_session_data(global_session_data);
    global_session_data = NULL;

    if (current_filename)
    {
        free(current_filename);
        current_filename = NULL;
    }

    printf("Recording session saved to: %s\n", filename);
}