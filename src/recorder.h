#ifndef RECORDER_H
#define RECORDER_H

#include <stddef.h>

void close_session_file(void);
void signal_handler(int signal);
void start_recording(const char *filename);
char *create_json_session_step(
    time_t timestamp,
    char command[1024],
    char *stdout_str,
    char *stderr_str);

#endif
