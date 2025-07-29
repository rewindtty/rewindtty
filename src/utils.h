#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <unistd.h>

typedef struct
{
    char *stdout_buf;
    size_t stdout_size;
    char *stderr_buf;
    size_t stderr_size;
} Output;

Output exec_and_capture(
    const char *command,
    int *child_running,
    pid_t *current_child_pid);
void free_output(Output *out);
char *read_file(const char *filename);
#endif
