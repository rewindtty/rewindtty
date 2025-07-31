#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>


char *read_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("fopen");
        return NULL;
    }

    fseek(fp, 0, SEEK_END);

    long file_length = ftell(fp);
    rewind(fp);

    char *content = malloc(file_length + 1);

    if (!content)
    {
        fclose(fp);
        return NULL;
    }

    fread(content, 1, file_length, fp);

    content[file_length] = '\0';

    fclose(fp);

    return content;
}