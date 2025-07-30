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

typedef enum
{
    OUTPUT_FILE,
    OUTPUT_PASTEBIN
} OutputType;

Output exec_and_capture(
    const char *command,
    const char *shell_path,
    int *child_running,
    pid_t *current_child_pid);
void free_output(Output *out);
char *read_file(const char *filename);
int file_exists(const char *filename);
OutputType detect_output_type(const char *input);
#endif
