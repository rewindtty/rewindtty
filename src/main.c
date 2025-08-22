#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "consts.h"
#include "recorder.h"
#include "replayer.h"
#include "analyzer.h"
#include "uploader.h"
#include <sys/stat.h>

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <record|replay|analyze|upload> [options] [session_file]\n", argv[0]);
        fprintf(stderr, "Options for record:\n");
        fprintf(stderr, "  --interactive    Record in interactive mode (script-like behavior)\n");
        fprintf(stderr, "  --upload         Upload recorded session to " UPLOAD_URL "\n");
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  upload           Upload existing session file to " UPLOAD_URL "\n");
        return 1;
    }

    if (argc > 1)
    {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)
        {
            printf("rewindtty version %s\n", REWINDTTY_VERSION);
            return 0;
        }
    }

    const char *session_file = DEFAULT_SESSION_FILE;
    int interactive_mode = 0;
    int upload_enabled = 0;
    const char *upload_url = UPLOAD_URL;
    int arg_index = 2;

    // Parse flags for record command
    if (strcmp(argv[1], "record") == 0 && argc > 2)
    {
        for (int i = 2; i < argc; i++)
        {
            if (strcmp(argv[i], "--interactive") == 0)
            {
                interactive_mode = 1;
                arg_index = i + 1;
            }
            else if (strcmp(argv[i], "--upload") == 0)
            {
                upload_enabled = 1;
                arg_index = i + 1;
            }
            else if (argv[i][0] != '-')
            {
                // This is the session file
                session_file = argv[i];
                break;
            }
        }
    }

    if (argc > arg_index)
    {
        session_file = argv[arg_index];
    }

    if (strcmp(argv[1], "record") == 0)
    {
        if (interactive_mode)
        {
            start_interactive_recording_with_upload(session_file, upload_enabled, upload_url);
        }
        else
        {
            start_recording_with_upload(session_file, upload_enabled, upload_url);
        }
    }
    else if (strcmp(argv[1], "replay") == 0)
    {
        replay_session_from_file(session_file, 1.0);
    }
    else if (strcmp(argv[1], "analyze") == 0)
    {
        analyze_session(session_file);
    }
    else if (strcmp(argv[1], "upload") == 0)
    {
        printf("Uploading session file: %s\n", session_file);
        printf("Upload url is: %s\n", upload_url);
        if (upload_session_file(session_file, upload_url))
        {
            printf("Upload completed successfully!\n");
        }
        else
        {
            printf("Upload failed.\n");
            return 1;
        }
    }
    else
    {
        fprintf(stderr, "Unknown command '%s'. Use 'record', 'replay', 'analyze', or 'upload'\n", argv[1]);
        return 1;
    }

    return 0;
}
