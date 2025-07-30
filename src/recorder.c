#include "recorder.h"
#include "cJSON.h"
#include "http.h"
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
static OutputType output_type = OUTPUT_FILE;

void finalize_session(void)
{
    if (!fp)
    {
        return;
    }

    if (output_type == OUTPUT_PASTEBIN)
    {
        printf("Uploading file...");
        cJSON *json_body = cJSON_CreateObject();
        if (json_body == NULL)
        {
            fprintf(stderr, "Unable to create JSON body for uploading file.");
            goto exit_creation;
        }

        request_http_post("https://jsonhosting.com/api/json", json_body);
    }

exit_creation:
    fprintf(fp, "\n]\n");
    fclose(fp);
    fp = NULL;
}

void signal_handler(int signal)
{
    if (signal == SIGINT && child_running && current_child_pid > 0)
    {
        kill(current_child_pid, SIGINT);
        return;
    }

    printf("\n[!] Signal received (%d), I will close session file...\n", signal);
    finalize_session();
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

void start_recording(const char *filename_or_pastebin)
{

    output_type = detect_output_type(filename_or_pastebin);

    char *filename = NULL;

    if (output_type == OUTPUT_PASTEBIN)
    {
        const char tmp_session_file_path[] = "/tmp/session.json";
        filename = malloc(sizeof(tmp_session_file_path));
        strcpy(filename, tmp_session_file_path);
    }
    else
    {
        filename = strdup(filename_or_pastebin);
        printf("Output will be written on %s\n", filename);
    }

    fp = fopen(filename, "w");
    if (!fp)
    {
        perror("fopen");
    }

    char command[1024];

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

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

    finalize_session();
}