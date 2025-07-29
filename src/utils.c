#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define BUF_SIZE 256

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

Output exec_and_capture(
    const char *command,
    const char *shell_path,
    int *child_running,
    pid_t *current_child_pid)
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

    *current_child_pid = pid;
    *child_running = 1;

    if (pid == 0)
    {
        // Child process
        close(outpipe[0]);
        close(errpipe[0]);

        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGHUP, SIG_DFL);

        dup2(outpipe[1], STDOUT_FILENO);
        dup2(errpipe[1], STDERR_FILENO);

        close(outpipe[1]);
        close(errpipe[1]);

        execl(shell_path, shell_path, "-c", command, (char *)NULL);
        perror("execl");
        exit(1);
    }
    else
    {
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

        *child_running = 0;
        *current_child_pid = 0;

        if (out.stdout_buf)
            out.stdout_buf[out.stdout_size] = '\0';
        if (out.stderr_buf)
            out.stderr_buf[out.stderr_size] = '\0';

        return out;
    }
}

int file_exists(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    int is_exist = 0;
    if (fp != NULL)
    {
        is_exist = 1;
        fclose(fp);
    }
    return is_exist;
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