#include "utils.h"
#include "analyzer.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *error_keywords[] = {
    "error", "failed", "permission denied", "no such file",
    "command not found", "segmentation fault", "core dumped",
    "syntax error", "not permitted", "timed out", "killed"};

static int has_error_indicators(const char *data)
{

    if (!data)
        return 0;
    char *lower_data = to_lower(data);

    int has_errors = 0;
    for (size_t i = 0; i < (sizeof(error_keywords) / sizeof(error_keywords[0])); i++)
    {
        if (strstr(data, error_keywords[i]) != NULL)
        {
            has_errors = 1;
            break;
        }
    }
    free(lower_data);
    return has_errors;
}

static char *format_duration(double seconds)
{
    static char buffer[64];
    int hours = (int)(seconds / 3600);
    int minutes = (int)((seconds - hours * 3600) / 60);
    int secs = (int)(seconds - hours * 3600 - minutes * 60);

    if (hours > 0)
    {
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hours, minutes, secs);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, secs);
    }
    return buffer;
}

static void count_command_frequency(CommandInfo *commands, int count, CommandInfo **top_commands, int *top_count)
{
    typedef struct
    {
        char *command;
        int frequency;
        double total_duration;
    } CommandFreq;

    CommandFreq freq_table[1000];
    int freq_count = 0;

    for (int i = 0; i < count; i++)
    {
        int found = 0;
        for (int j = 0; j < freq_count; j++)
        {
            if (strcmp(freq_table[j].command, commands[i].command) == 0)
            {
                freq_table[j].frequency++;
                freq_table[j].total_duration += commands[i].duration;
                found = 1;
                break;
            }
        }
        if (!found && freq_count < 1000)
        {
            freq_table[freq_count].command = strdup(commands[i].command);
            freq_table[freq_count].frequency = 1;
            freq_table[freq_count].total_duration = commands[i].duration;
            freq_count++;
        }
    }

    // Sort by frequency
    for (int i = 0; i < freq_count - 1; i++)
    {
        for (int j = i + 1; j < freq_count; j++)
        {
            if (freq_table[i].frequency < freq_table[j].frequency)
            {
                CommandFreq temp = freq_table[i];
                freq_table[i] = freq_table[j];
                freq_table[j] = temp;
            }
        }
    }

    *top_count = freq_count < 10 ? freq_count : 10;
    for (int i = 0; i < *top_count; i++)
    {
        top_commands[i] = malloc(sizeof(CommandInfo));
        top_commands[i]->command = strdup(freq_table[i].command);
        top_commands[i]->duration = freq_table[i].total_duration;
        top_commands[i]->chunk_count = freq_table[i].frequency; // Using chunk_count as frequency
    }

    // Cleanup
    for (int i = 0; i < freq_count; i++)
    {
        free(freq_table[i].command);
    }
}

void analyze_session(const char *session_file)
{
    char *json_string = read_file(session_file);
    if (!json_string)
    {
        return;
    }

    cJSON *json = cJSON_Parse(json_string);
    if (!json)
    {
        fprintf(stderr, "Error: Invalid JSON in session file\n");
        free(json_string);
        return;
    }

    if (!cJSON_IsArray(json))
    {
        fprintf(stderr, "Error: Session file should contain an array of commands\n");
        cJSON_Delete(json);
        free(json_string);
        return;
    }

    SessionAnalysis analysis = {0};
    int array_size = cJSON_GetArraySize(json);
    analysis.total_commands = array_size;
    analysis.commands = malloc(array_size * sizeof(CommandInfo));

    double total_duration = 0;
    double first_start_time = -1;
    double last_end_time = 0;

    for (int i = 0; i < array_size; i++)
    {
        cJSON *command_obj = cJSON_GetArrayItem(json, i);
        if (!command_obj)
            continue;

        cJSON *command = cJSON_GetObjectItem(command_obj, "command");
        cJSON *start_time = cJSON_GetObjectItem(command_obj, "start_time");
        cJSON *end_time = cJSON_GetObjectItem(command_obj, "end_time");
        cJSON *duration = cJSON_GetObjectItem(command_obj, "duration");
        cJSON *chunks = cJSON_GetObjectItem(command_obj, "chunks");

        if (command && start_time && end_time && duration)
        {
            analysis.commands[i].command = strdup(cJSON_GetStringValue(command));
            analysis.commands[i].start_time = cJSON_GetNumberValue(start_time);
            analysis.commands[i].end_time = cJSON_GetNumberValue(end_time);
            analysis.commands[i].duration = cJSON_GetNumberValue(duration);
            analysis.commands[i].chunk_count = chunks ? cJSON_GetArraySize(chunks) : 0;
            analysis.commands[i].has_stderr = 0;

            // Track session duration
            if (first_start_time < 0)
            {
                first_start_time = analysis.commands[i].start_time;
            }
            if (analysis.commands[i].end_time > last_end_time)
            {
                last_end_time = analysis.commands[i].end_time;
            }

            // Check for stderr/errors in chunks
            if (chunks)
            {
                int chunk_count = cJSON_GetArraySize(chunks);
                for (int j = 0; j < chunk_count; j++)
                {
                    cJSON *chunk = cJSON_GetArrayItem(chunks, j);
                    cJSON *data = cJSON_GetObjectItem(chunk, "data");
                    if (data && has_error_indicators(cJSON_GetStringValue(data)))
                    {
                        analysis.commands[i].stderr_data = strdup(cJSON_GetStringValue(data));
                        analysis.commands[i].has_stderr = 1;
                        analysis.commands_with_stderr++;
                        break;
                    }
                }
            }

            total_duration += analysis.commands[i].duration;
        }
    }

    analysis.total_duration = last_end_time - first_start_time;
    analysis.avg_time_per_command = total_duration / analysis.total_commands;
    analysis.stderr_percentage = (double)analysis.commands_with_stderr / analysis.total_commands * 100;

    // Find top commands by frequency
    count_command_frequency(analysis.commands, analysis.total_commands,
                            analysis.top_commands, &analysis.top_commands_count);

    // Find slowest commands
    CommandInfo *sorted_by_duration = malloc(analysis.total_commands * sizeof(CommandInfo));
    memcpy(sorted_by_duration, analysis.commands, analysis.total_commands * sizeof(CommandInfo));

    for (int i = 0; i < analysis.total_commands - 1; i++)
    {
        for (int j = i + 1; j < analysis.total_commands; j++)
        {
            if (sorted_by_duration[i].duration < sorted_by_duration[j].duration)
            {
                CommandInfo temp = sorted_by_duration[i];
                sorted_by_duration[i] = sorted_by_duration[j];
                sorted_by_duration[j] = temp;
            }
        }
    }

    analysis.slowest_commands_count = analysis.total_commands < 5 ? analysis.total_commands : 5;
    for (int i = 0; i < analysis.slowest_commands_count; i++)
    {
        analysis.slowest_commands[i] = &sorted_by_duration[i];
    }

    // Find error commands
    analysis.error_commands_count = 0;
    for (int i = 0; i < analysis.total_commands && analysis.error_commands_count < 10; i++)
    {
        if (analysis.commands[i].has_stderr)
        {
            analysis.error_commands[analysis.error_commands_count] = &analysis.commands[i];
            analysis.error_commands_count++;
        }
    }

    print_session_summary(&analysis);

    // Cleanup
    for (int i = 0; i < analysis.top_commands_count; i++)
    {
        free(analysis.top_commands[i]->command);
        free(analysis.top_commands[i]);
    }
    free(sorted_by_duration);
    free_session_analysis(&analysis);
    cJSON_Delete(json);
    free(json_string);
}

void print_session_summary(SessionAnalysis *analysis)
{
    printf("📊 Session Summary\n");
    printf("--------------------\n");
    printf("Total commands:           %d\n", analysis->total_commands);
    printf("Session duration:         %s\n", format_duration(analysis->total_duration));
    printf("Average time per command: %.1fs\n", analysis->avg_time_per_command);
    printf("Commands with stderr:     %d (%.1f%%)\n",
           analysis->commands_with_stderr, analysis->stderr_percentage);
    printf("\n");

    if (analysis->top_commands_count > 0)
    {
        printf("🔥 Top Commands\n");
        for (int i = 0; i < analysis->top_commands_count && i < 3; i++)
        {
            printf("%d. %-12s %d times\n", i + 1,
                   analysis->top_commands[i]->command,
                   analysis->top_commands[i]->chunk_count);
        }
        printf("\n");
    }

    if (analysis->slowest_commands_count > 0)
    {
        printf("⚠️  Slowest Commands\n");
        for (int i = 0; i < analysis->slowest_commands_count && i < 2; i++)
        {
            printf("%-12s (%.1fs)\n",
                   analysis->slowest_commands[i]->command,
                   analysis->slowest_commands[i]->duration);
        }
        printf("\n");
    }

    if (analysis->error_commands_count > 0)
    {
        printf("❌ Errors\n");
        for (int i = 0; i < analysis->error_commands_count && i < 2; i++)
        {
            printf("- %-12s → %s\n",
                   analysis->error_commands[i]->command,
                   analysis->error_commands[i]->stderr_data);
        }
        printf("\n");
    }

    printf("💬 Suggestions\n");
    printf("- Try using `grep -i` for case-insensitive search\n");
}

void free_session_analysis(SessionAnalysis *analysis)
{
    if (analysis->commands)
    {
        for (int i = 0; i < analysis->total_commands; i++)
        {
            free(analysis->commands[i].command);
        }
        free(analysis->commands);
    }
}