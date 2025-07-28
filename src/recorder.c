#include "recorder.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <stddef.h>

static FILE *fp = NULL;
static int first = 1;

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
    printf("\n[!] Signal received (%d), I will close session file...\n", signal);
    close_session_file();
    _exit(1); // exit and avoid unsecure calls in signal handler
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

        Output out = exec_and_capture(command);
        time_t now = time(NULL);

        if (!first)
            fprintf(fp, ",\n");
        first = 0;

        char *sanitized_stdout = escape_json_string(out.stdout_buf ? out.stdout_buf : "");
        char *sanitized_stderr = escape_json_string(out.stderr_buf ? out.stderr_buf : "");

        fprintf(fp, "  {\n"
                    "    \"timestamp\": %ld,\n"
                    "    \"command\": \"%s\",\n"
                    "    \"output\": \"%s\",\n"
                    "    \"stderr\": \"%s\"\n"
                    "  }",
                now, command, sanitized_stdout, sanitized_stderr);

        free(sanitized_stdout);
        free(sanitized_stderr);

        free_output(&out);
    }

    close_session_file();
}