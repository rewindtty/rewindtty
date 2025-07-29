#include "replayer.h"
#include "utils.h"
#include "cJSON.h"
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
    char *content = read_file(filename);
    if (content == NULL)
    {
        fprintf(stderr, "Error reading file: %s", filename);
        return;
    }

    cJSON *session = cJSON_Parse(content);

    if (session == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        goto replay_end;
    }

    cJSON *step = NULL;
    time_t last_ts = 0;
    time_t current_ts = 0;

    if (session == NULL)
    {
        fprintf(stderr, "Session is empty");
        exit(0);
    }

    // start with a small delay 1s

    sleep(1);

    cJSON_ArrayForEach(step, session)
    {
        cJSON *timestamp = cJSON_GetObjectItemCaseSensitive(step, "timestamp");
        cJSON *command = cJSON_GetObjectItemCaseSensitive(step, "command");
        cJSON *output = cJSON_GetObjectItemCaseSensitive(step, "output");
        cJSON *std_err = cJSON_GetObjectItemCaseSensitive(step, "stderr");

        if (
            command != NULL &&
            timestamp != NULL &&
            output != NULL &&
            std_err != NULL)
        {

            current_ts = timestamp->valueint;

            if (last_ts != 0 && current_ts != 0)
            {
                int delay = current_ts - last_ts;
                if (delay > 0 && delay < 10)
                {
                    sleep(delay); // simulates the time between commands
                }
            }

            last_ts = current_ts;

            printf(COLOR_BLUE "rewindtty> %s\n" COLOR_RESET, command->valuestring);

            if (output->valuestring)
            {
                printf(COLOR_GREEN "%s" COLOR_RESET, output->valuestring);
            }
            if (std_err->valuestring)
            {
                fprintf(stderr, COLOR_RED "%s" COLOR_RESET, std_err->valuestring);
            }
        }
    }

replay_end:
    cJSON_Delete(session);
}