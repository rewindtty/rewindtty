#include "replayer.h"
#include "utils.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_RED "\x1b[31m"
#define COLOR_BLUE "\x1b[34m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_CYAN "\x1b[36m"

static volatile int is_replay_interrupted = 0;

void handle_sigint_during_replay(int sig)
{
    if (sig != SIGINT)
        return;
    is_replay_interrupted = 1;
    printf("\n" COLOR_YELLOW "[REPLAY INTERRUPTED]" COLOR_RESET "\n");
}

void sleep_for(double seconds)
{
    if (seconds <= 0)
        return;

    struct timespec req;
    req.tv_sec = (time_t)seconds;
    req.tv_nsec = (long)((seconds - req.tv_sec) * 1000000000);

    nanosleep(&req, NULL);
}

void setup_terminal_for_replay()
{
    // Save terminal settings
    struct termios term;
    tcgetattr(STDIN_FILENO, &term);

    // Enable canonical mode and echo for user input
    term.c_lflag |= ECHO;
    term.c_lflag |= ICANON;

    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Converts escaped literals (e.g., \u001b) into actual escape characters
char *decode_escaped_sequences(const char *input)
{
    if (!input)
        return NULL;

    size_t len = strlen(input);
    char *output = malloc(len * 2); // Extra space for safety
    size_t out_pos = 0;
    size_t i = 0;

    while (i < len)
    {
        // ESC unicode escape
        if (i + 5 < len && strncmp(input + i, "\\u001b", 6) == 0)
        {
            output[out_pos++] = '\033'; // ESC character
            i += 6;
        }
        // double escape
        else if (i + 6 < len && strncmp(input + i, "\\\\u001b", 7) == 0)
        {
            output[out_pos++] = '\033'; // ESC character
            i += 7;
        }
        // octal escape
        else if (i + 3 < len && strncmp(input + i, "\\033", 4) == 0)
        {
            output[out_pos++] = '\033'; // ESC character
            i += 4;
        }
        // double escape octal
        else if (i + 4 < len && strncmp(input + i, "\\\\033", 5) == 0)
        {
            output[out_pos++] = '\033'; // ESC character
            i += 5;
        }
        // hex escape
        else if (i + 3 < len && strncmp(input + i, "\\x1b", 4) == 0)
        {
            output[out_pos++] = '\033'; // ESC character
            i += 4;
        }
        // double hex escape
        else if (i + 4 < len && strncmp(input + i, "\\\\x1b", 5) == 0)
        {
            output[out_pos++] = '\033'; // ESC character
            i += 5;
        }
        // other common escape
        else if (i + 1 < len && input[i] == '\\')
        {
            switch (input[i + 1])
            {
            case 'n':
                output[out_pos++] = '\n';
                i += 2;
                break;
            case 'r':
                output[out_pos++] = '\r';
                i += 2;
                break;
            case 't':
                output[out_pos++] = '\t';
                i += 2;
                break;
            case 'b':
                output[out_pos++] = '\b';
                i += 2;
                break;
            case 'f':
                output[out_pos++] = '\f';
                i += 2;
                break;
            case 'v':
                output[out_pos++] = '\v';
                i += 2;
                break;
            case '\\':
                output[out_pos++] = '\\';
                i += 2;
                break;
            case '"':
                output[out_pos++] = '"';
                i += 2;
                break;
            case '/':
                output[out_pos++] = '/';
                i += 2;
                break;
            default:
                // backslash escape
                output[out_pos++] = input[i++];
                break;
            }
        }
        else
        {
            output[out_pos++] = input[i++];
        }
    }

    output[out_pos] = '\0';

    // Effective resize
    char *final_output = realloc(output, out_pos + 1);
    return final_output ? final_output : output;
}

void setup_terminal_for_ansi()
{
    // Ensure the terminal properly interprets ANSI escape sequences
    printf("\033[?25h");
    printf("\033[0m");
    fflush(stdout);

    struct termios term;
    tcgetattr(STDOUT_FILENO, &term);
    term.c_oflag |= OPOST; // Enable output processing
    tcsetattr(STDOUT_FILENO, TCSANOW, &term);
}

void replay_session_from_file(const char *filename, double speed_multiplier)
{
    signal(SIGINT, handle_sigint_during_replay);
    setup_terminal_for_replay();
    setup_terminal_for_ansi();

    char *content = read_file(filename);
    if (content == NULL)
    {
        fprintf(stderr, "Error reading file: %s\n", filename);
        return;
    }

    cJSON *json = cJSON_Parse(content);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "JSON Error: %s\n", error_ptr);
        }
        free(content);
        return;
    }

    cJSON *sessions = NULL;
    
    // Check if this is the new format with metadata
    if (cJSON_IsObject(json))
    {
        cJSON *metadata = cJSON_GetObjectItem(json, "metadata");
        if (metadata && cJSON_IsObject(metadata))
        {
            cJSON *interactive_mode = cJSON_GetObjectItem(metadata, "interactive_mode");
            if (interactive_mode && cJSON_IsBool(interactive_mode) && cJSON_IsTrue(interactive_mode))
            {
                printf(COLOR_YELLOW "Info: Playing back interactive mode session\n" COLOR_RESET);
            }
        }
        
        sessions = cJSON_GetObjectItem(json, "sessions");
        if (!sessions || !cJSON_IsArray(sessions))
        {
            fprintf(stderr, "Invalid JSON format: expected 'sessions' array\n");
            cJSON_Delete(json);
            free(content);
            return;
        }
    }
    else if (cJSON_IsArray(json))
    {
        // Legacy format - treat the entire JSON as sessions array
        sessions = json;
    }
    else
    {
        fprintf(stderr, "Invalid JSON format: expected array or metadata object\n");
        cJSON_Delete(json);
        free(content);
        return;
    }

    int session_count = cJSON_GetArraySize(sessions);
    printf(COLOR_CYAN "=== TTY REAL-TIME REPLAY ===" COLOR_RESET "\n");
    printf("Sessions to replay: %d\n", session_count);
    printf("Speed: %.1fx\n", speed_multiplier);
    printf("Interactive mode: Press ENTER to continue, 'q' to quit, 's' to skip\n");
    printf(COLOR_CYAN "============================" COLOR_RESET "\n\n");

    for (int i = 0; i < session_count && !is_replay_interrupted; i++)
    {
        cJSON *session = cJSON_GetArrayItem(sessions, i);
        if (!session)
            continue;

        cJSON *command_obj = cJSON_GetObjectItem(session, "command");
        cJSON *start_time_obj = cJSON_GetObjectItem(session, "start_time");
        cJSON *end_time_obj = cJSON_GetObjectItem(session, "end_time");
        cJSON *chunks_obj = cJSON_GetObjectItem(session, "chunks");

        if (!command_obj || !chunks_obj || !cJSON_IsArray(chunks_obj))
        {
            continue;
        }

        const char *command = command_obj->valuestring;
        double start_time = start_time_obj ? start_time_obj->valuedouble : 0;
        double end_time = end_time_obj ? end_time_obj->valuedouble : 0;
        double duration = end_time - start_time;

        printf(COLOR_BLUE "rewindtty> %s" COLOR_RESET, command);
        if (duration > 0)
        {
            printf(COLOR_YELLOW " (duration: %.2fs)" COLOR_RESET, duration);
        }
        printf("\n");

        /*

        @TODO future improvement: allow to check steps in a manual mode

        printf(COLOR_CYAN "[Press ENTER to start, 'q' to quit, 's' to skip]: " COLOR_RESET);
        fflush(stdout);

        char input[10];
        if (fgets(input, sizeof(input), stdin))
        {
            if (input[0] == 'q')
            {
                printf(COLOR_YELLOW "Replay quit by user.\n" COLOR_RESET);
                break;
            }
            else if (input[0] == 's')
            {
                printf(COLOR_YELLOW "Session skipped.\n" COLOR_RESET);
                continue;
            }
        }
        */
        int chunk_count = cJSON_GetArraySize(chunks_obj);
        double last_time = 0;

        for (int j = 0; j < chunk_count && !is_replay_interrupted; j++)
        {
            cJSON *chunk = cJSON_GetArrayItem(chunks_obj, j);
            if (!chunk)
                continue;

            cJSON *time_obj = cJSON_GetObjectItem(chunk, "time");
            cJSON *data_obj = cJSON_GetObjectItem(chunk, "data");

            if (!time_obj || !data_obj)
                continue;

            double chunk_time = time_obj->valuedouble;
            const char *data = data_obj->valuestring;

            // Calculate dalay
            double delay = (chunk_time - last_time) / speed_multiplier;
            if (delay > 0 && delay < 10.0)
            {
                sleep_for(delay);
            }

            char *processed_data = decode_escaped_sequences(data);
            if (!processed_data)
            {
                processed_data = strdup(data);
            }
            // Print processing chunk
            size_t data_len = strlen(processed_data);
            ssize_t written = write(STDOUT_FILENO, processed_data, data_len);
            if (written != (ssize_t)data_len)
            {
                // Fallback to printf if write fails
                printf("%s", processed_data);
            }
            fflush(stdout);

            free(processed_data);
            last_time = chunk_time;
        }

        printf("\n" COLOR_GREEN "[Command completed]" COLOR_RESET "\n\n");

        if (i < session_count - 1)
        {
            sleep_for(0.5 / speed_multiplier);
        }
    }

    if (is_replay_interrupted)
    {
        printf(COLOR_YELLOW "\nReplay was interrupted.\n" COLOR_RESET);
    }
    else
    {
        printf(COLOR_CYAN "=== REPLAY COMPLETED ===" COLOR_RESET "\n");
    }

    cJSON_Delete(json);
    free(content);
}