#include "recorder.h"
#include "cJSON.h"
#include "utils.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stddef.h>

static FILE *fp = NULL;
static int first = 1;
static int child_running = 0;
static pid_t current_child_pid = 0;

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

        Output out = exec_and_capture(
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