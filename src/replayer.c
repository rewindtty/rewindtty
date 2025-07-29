#include "replayer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_RED "\x1b[31m"
#define COLOR_BLUE "\x1b[34m"

void start_replay(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("fopen");
        return;
    }

    char line[MAX_LINE];
    char command[MAX_LINE] = "";
    char output[MAX_LINE * 10] = "";
    char stderr_buf[MAX_LINE * 10] = "";
    time_t last_ts = 0;
    time_t current_ts = 0;

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, "{"))
        {
            command[0] = '\0';
            output[0] = '\0';
            stderr_buf[0] = '\0';
            current_ts = 0;
            continue;
        }

        if (strstr(line, "timestamp"))
        {
            current_ts = (time_t)atol(extract_json_field(line, "timestamp"));
        }

        if (strstr(line, "command"))
        {
            strncpy(command, extract_json_field(line, "command"), sizeof(command));
        }

        if (strstr(line, "output"))
        {
            strncpy(output, extract_json_field(line, "output"), sizeof(output));
        }

        if (strstr(line, "stderr"))
        {
            strncpy(stderr_buf, extract_json_field(line, "stderr"), sizeof(stderr_buf));
        }

        if (strstr(line, "}"))
        {
            if (last_ts != 0 && current_ts != 0)
            {
                int delay = current_ts - last_ts;
                if (delay > 0 && delay < 10)
                {
                    sleep(delay); // simulates the time between commands
                }
            }

            last_ts = current_ts;

            // Replay print
            printf(COLOR_BLUE "rewindtty> %s\n" COLOR_RESET, command);

            char *decoded_out = unescape_json_string(output);
            char *decoded_err = unescape_json_string(stderr_buf);

            if (decoded_out && *decoded_out)
            {
                printf(COLOR_GREEN "%s" COLOR_RESET, decoded_out);
            }
            if (decoded_err && *decoded_err)
            {
                fprintf(stderr, COLOR_RED "%s" COLOR_RESET, decoded_err);
            }

            free(decoded_out);
            free(decoded_err);
        }
    }

    fclose(fp);
}