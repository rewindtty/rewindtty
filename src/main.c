#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "recorder.h"
#include "replayer.h"
#include <sys/stat.h>

#define DEFAULT_SESSION_FILE "data/session.json"

#include <stdio.h>

#ifndef REWINDTTY_VERSION
#define REWINDTTY_VERSION "dev"
#endif

int main(int argc, char *argv[])
{

    if (argc > 1)
    {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)
        {
            printf("rewindtty version %s\n", REWINDTTY_VERSION);
            return 0;
        }
        // altre opzioni...
    }

    const char *session_file = DEFAULT_SESSION_FILE;

    if (argc >= 3)
    {
        session_file = argv[2];
    }
    else
    {
#ifdef _WIN32
        _mkdir("data");
#else
        mkdir("data", 0755);
#endif
    }

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <record|replay> [session_file]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "record") == 0)
    {
        start_recording(session_file);
    }
    else if (strcmp(argv[1], "replay") == 0)
    {
        start_replay(session_file);
    }
    else
    {
        fprintf(stderr, "Unknown command '%s'. Use 'record' or 'replay'\n", argv[1]);
        return 1;
    }

    return 0;
}
