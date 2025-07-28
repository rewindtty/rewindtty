#ifndef RECORDER_H
#define RECORDER_H

void close_session_file(void);
void signal_handler(int signal);
void start_recording(const char *filename);

#endif
