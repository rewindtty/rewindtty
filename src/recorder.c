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

#define BUF_SIZE 256

static FILE *fp = NULL;
static int first = 1;
static int child_running = 0;
static pid_t current_child_pid = 0;

int file_exists(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    int is_exist = 0;
    if (fp != NULL)
    {
        is_exist = 1;
        fclose(fp);
    }
    return is_exist;
}

void free_output(Output *out)
{
    if (!out)
        return;
    if (out->stdout_buf)
        free(out->stdout_buf);
    if (out->stderr_buf)
        free(out->stderr_buf);
    out->stdout_buf = NULL;
    out->stderr_buf = NULL;
    out->stdout_size = 0;
    out->stderr_size = 0;
}

Output exec_and_capture_pty(
    const char *command,
    const char *shell_path,
    int *child_running,
    pid_t *current_child_pid)
{
    int master_fd;
    pid_t pid;
    Output out = {NULL, 0, NULL, 0};
    struct termios term_attrs, raw_attrs;

    if (tcgetattr(STDIN_FILENO, &term_attrs) != 0)
    {
        perror("tcgetattr");
        return out;
    }

    pid = forkpty(&master_fd, NULL, &term_attrs, NULL);
    if (pid == -1)
    {
        perror("forkpty");
        return out;
    }

    *current_child_pid = pid;
    *child_running = 1;

    if (pid == 0)
    {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);

        execl(shell_path, shell_path, "-c", command, (char *)NULL);
        perror("execl");
        exit(1);
    }
    else
    {
        raw_attrs = term_attrs;
        cfmakeraw(&raw_attrs);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_attrs);

        char buffer[BUF_SIZE];
        ssize_t n;
        int status;
        fd_set read_fds;
        int stdin_fd = STDIN_FILENO;
        int max_fd = (master_fd > stdin_fd) ? master_fd : stdin_fd;

        while (*child_running)
        {
            FD_ZERO(&read_fds);
            FD_SET(master_fd, &read_fds);
            FD_SET(stdin_fd, &read_fds);

            struct timeval timeout = {0, 10000};
            int select_result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

            if (select_result > 0)
            {
                if (FD_ISSET(master_fd, &read_fds))
                {
                    n = read(master_fd, buffer, BUF_SIZE - 1);
                    if (n > 0)
                    {
                        write(STDOUT_FILENO, buffer, n);

                        out.stdout_buf = realloc(out.stdout_buf, out.stdout_size + n + 1);
                        if (!out.stdout_buf)
                        {
                            perror("realloc");
                            break;
                        }
                        memcpy(out.stdout_buf + out.stdout_size, buffer, n);
                        out.stdout_size += n;
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

        if (out.stdout_buf)
            out.stdout_buf[out.stdout_size] = '\0';

        return out;
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

    // Altrimenti gestisci normalmente
    printf("\n[!] Signal received (%d), I will close session file...\n", signal);
    close_session_file();
    _exit(1); // exit and avoid unsecure calls in signal handler
}

char *create_json_session_step(
    time_t timestamp,
    char command[1024],
    char *stdout_str,
    char *stderr_str)
{
    char *string = NULL;
    cJSON *sessionStep = cJSON_CreateObject();

    if (cJSON_AddNumberToObject(sessionStep, "timestamp", (long int)timestamp) == NULL)
    {
        goto exit_creation;
    }

    if (cJSON_AddStringToObject(sessionStep, "command", command) == NULL)
    {
        goto exit_creation;
    }

    if (cJSON_AddStringToObject(sessionStep, "output", stdout_str) == NULL)
    {
        goto exit_creation;
    }

    if (cJSON_AddStringToObject(sessionStep, "stderr", stderr_str) == NULL)
    {
        goto exit_creation;
    }

    string = cJSON_Print(sessionStep);
    if (string == NULL)
    {
        fprintf(stderr, "Failed to create json string.\n");
    }

exit_creation:
    cJSON_Delete(sessionStep);
    return string;
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
    }

    fprintf(fp, "[\n");
    first = 1;
    while (1)
    {
        printf("rewindtty> ");

        if (!fgets(command, sizeof(command), stdin))
            break;

        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "exit") == 0)
            break;

        const char *shell_path = getenv("SHELL");
        if (!shell_path)
            shell_path = "/bin/sh";

        if (!file_exists(shell_path))
        {
            fprintf(stdout, "Shell identified does not exists (%s)", shell_path);
            exit(1);
        }

        Output out = exec_and_capture_pty(
            command,
            shell_path,
            &child_running,
            &current_child_pid);

        time_t timestamp = time(NULL);

        if (!first)
            fprintf(fp, ",\n");
        first = 0;

        char *session_step = create_json_session_step(
            timestamp,
            command,
            out.stdout_buf ? out.stdout_buf : "",
            out.stderr_buf ? out.stderr_buf : "");

        fprintf(fp, session_step);

        free(session_step);
        free_output(&out);
    }

    close_session_file();
}