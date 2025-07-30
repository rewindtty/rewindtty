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

char *read_file(const char *filename);
#endif
