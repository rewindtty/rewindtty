#ifndef ANALYZER_H
#define ANALYZER_H

#include <time.h>

typedef struct
{
    char *command;
    char *stderr_data;
    double start_time;
    double end_time;
    double duration;
    int has_stderr;
    int chunk_count;
} CommandInfo;

typedef struct
{
    int total_commands;
    double total_duration;
    double avg_time_per_command;
    int commands_with_stderr;
    double stderr_percentage;
    CommandInfo *commands;
    CommandInfo *top_commands[10];
    CommandInfo *slowest_commands[5];
    CommandInfo *error_commands[10];
    int top_commands_count;
    int slowest_commands_count;
    int error_commands_count;
} SessionAnalysis;

void analyze_session(const char *session_file);
void print_session_summary(SessionAnalysis *analysis);
void free_session_analysis(SessionAnalysis *analysis);

#endif