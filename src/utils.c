#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>

char *to_lower(const char *str)
{
    char *lower = strdup(str);
    return lower;
}

char *read_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        perror(filename); 
        return NULL;
    }

    if (fseek(file, 0, SEEK_END))
    {
        perror("fseek"); 
        fclose(file);
        return NULL; 
    }

    long file_size = ftell(file);
    if (file_size < 0)
    {
        perror("ftell"); 
        fclose(file);
        return NULL;
    }
    rewind(file);

    char *content = malloc(file_size + 1);
    if (!content)
    {
        fprintf(stderr, "Error cannot allocate %ld bytes", file_size);
        fclose(file);
        return NULL;
    }

    size_t bytes_read = fread(content, 1, file_size, file);
    fclose(file);
    if (bytes_read != (size_t)file_size)
    {
        fprintf(stderr, "Error: fread failed (read %zu / %ld)\n", bytes_read, file_size);
        free(content); 
        return NULL;
    }

    content[file_size] = '\0';
    return content;
}
