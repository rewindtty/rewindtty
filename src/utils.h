#include <stddef.h>

#define MAX_LINE 8192

#ifndef UTILS_H
#define UTILS_H

typedef struct
{
    char *stdout_buf;
    size_t stdout_size;
    char *stderr_buf;
    size_t stderr_size;
} Output;

Output exec_and_capture(const char *command);
void free_output(Output *out);
char *escape_json_string(char *input);
char *extract_json_field(const char *line, const char *field);
char *unescape_json_string(const char *str);
#endif
