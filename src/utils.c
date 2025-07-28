#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define BUF_SIZE 256

Output exec_and_capture(const char *command)
{
    int outpipe[2];
    int errpipe[2];
    pid_t pid;
    Output out = {NULL, 0, NULL, 0};

    if (pipe(outpipe) == -1 || pipe(errpipe) == -1)
    {
        perror("pipe");
        return out;
    }

    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return out;
    }

    if (pid == 0)
    {
        // Child process
        close(outpipe[0]);
        close(errpipe[0]);

        dup2(outpipe[1], STDOUT_FILENO);
        dup2(errpipe[1], STDERR_FILENO);

        close(outpipe[1]);
        close(errpipe[1]);

        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        perror("execl");
        exit(1);
    }
    else
    {
        // Father process
        close(outpipe[1]);
        close(errpipe[1]);

        fcntl(outpipe[0], F_SETFL, O_NONBLOCK);
        fcntl(errpipe[0], F_SETFL, O_NONBLOCK);

        char buffer[BUF_SIZE];
        ssize_t n;
        int out_closed = 0, err_closed = 0;
        int status;

        while (!out_closed || !err_closed)
        {
            if (!out_closed)
            {
                n = read(outpipe[0], buffer, BUF_SIZE - 1);
                if (n > 0)
                {
                    buffer[n] = '\0';
                    fwrite(buffer, 1, n, stdout);
                    fflush(stdout);
                    out.stdout_buf = realloc(out.stdout_buf, out.stdout_size + n + 1);
                    if (!out.stdout_buf)
                    {
                        perror("realloc");
                        break;
                    }
                    memcpy(out.stdout_buf + out.stdout_size, buffer, n + 1);
                    out.stdout_size += n;
                }
                else if (n == 0)
                {
                    out_closed = 1;
                    close(outpipe[0]);
                }
            }

            if (!err_closed)
            {
                n = read(errpipe[0], buffer, BUF_SIZE - 1);
                if (n > 0)
                {
                    buffer[n] = '\0';
                    fwrite(buffer, 1, n, stderr);
                    fflush(stderr);
                    out.stderr_buf = realloc(out.stderr_buf, out.stderr_size + n + 1);
                    if (!out.stderr_buf)
                    {
                        perror("realloc");
                        break;
                    }
                    memcpy(out.stderr_buf + out.stderr_size, buffer, n + 1);
                    out.stderr_size += n;
                }
                else if (n == 0)
                {
                    err_closed = 1;
                    close(errpipe[0]);
                }
            }
            usleep(10000);
        }

        waitpid(pid, &status, 0);

        if (out.stdout_buf)
            out.stdout_buf[out.stdout_size] = '\0';
        if (out.stderr_buf)
            out.stderr_buf[out.stderr_size] = '\0';

        return out;
    }
}

void free_output(Output *out)
{
    if (!out)
        return;
    if (out->stdout_buf)
        free(out->stdout_buf);
    if (out->stderr_buf)
        free(out->stderr_buf);
    out->stdout_buf = NULL;
    out->stderr_buf = NULL;
    out->stdout_size = 0;
    out->stderr_size = 0;
}

char *escape_json_string(char *input)
{
    char *sanitized = malloc(strlen(input) * 2 + 1);
    if (sanitized)
    {
        char *src = input;
        char *dst = sanitized;
        while (*src)
        {
            if (*src == '"')
            {
                *dst++ = '\\';
                *dst++ = '"';
            }
            else if (*src == '\n')
            {
                *dst++ = '\\';
                *dst++ = 'n';
            }
            else
            {
                *dst++ = *src;
            }
            src++;
        }
        *dst = '\0';
        return sanitized;
    }
    return "";
}

char *extract_json_field(const char *line, const char *field)
{
    static char buffer[MAX_LINE];
    char *start = strstr(line, field);
    if (!start)
        return NULL;
    start = strchr(start, ':');
    if (!start)
        return NULL;
    start++;
    while (*start == ' ' || *start == '"')
        start++;
    char *end = start;
    while (*end && *end != '"' && *end != '\n' && *end != ',')
        end++; // stop anche a virgola
    size_t len = end - start;
    if (len >= MAX_LINE)
        len = MAX_LINE - 1;
    strncpy(buffer, start, len);
    buffer[len] = '\0';
    return buffer;
}

char *unescape_json_string(const char *str)
{
    if (!str)
        return NULL;

    size_t len = strlen(str);
    char *result = malloc(len + 1); // worst case: no escapes
    if (!result)
        return NULL;

    char *dst = result;
    const char *src = str;

    while (*src)
    {
        if (*src == '\\')
        {
            src++;
            switch (*src)
            {
            case 'n':
                *dst++ = '\n';
                break;
            case 't':
                *dst++ = '\t';
                break;
            case 'r':
                *dst++ = '\r';
                break;
            case '\\':
                *dst++ = '\\';
                break;
            case '"':
                *dst++ = '"';
                break;
            default:
                *dst++ = *src;
                break;
            }
            src++;
        }
        else
        {
            *dst++ = *src++;
        }
    }

    *dst = '\0';
    return result;
}