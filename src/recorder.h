#ifndef RECORDER_H
#define RECORDER_H

#include <stddef.h>
#include <time.h>

typedef struct
{
    double timestamp;
    size_t data_length;
    char *data;
} TTYChunk;

typedef struct
{
    char command[1024];
    double start_time;
    double end_time;
    TTYChunk *chunks;
    size_t chunk_count;
    size_t chunk_capacity;
} TTYSession;

typedef struct
{
    TTYSession **sessions;
    size_t session_count;
    size_t session_capacity;
    int interactive_mode;
    double start_timestamp;
} SessionData;

// Command detection structures
typedef struct
{
    char *buffer;
    size_t size;
    size_t capacity;
} InputBuffer;

typedef struct
{
    char command[1024];
    int command_ready;
    double command_start_time;
} CommandDetector;

double get_timestamp(void);
TTYSession *create_tty_session(const char *command);
void add_chunk_to_session(TTYSession *session, double timestamp, const char *data, size_t length);
void finish_tty_session(TTYSession *session);
void free_tty_session(TTYSession *session);
void signal_handler(int signal);
void start_recording(const char *filename);
void start_interactive_recording(const char *filename);
char *create_json_session_step(
    time_t timestamp,
    char command[1024],
    char *stdout_str,
    char *stderr_str);

#endif
