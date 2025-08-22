#ifndef UPLOADER_H
#define UPLOADER_H

#include "consts.h"
#include "recorder.h"

// Upload session data to remote service
int upload_session_data(const char *json_data, const char *upload_url);

// Upload session file to remote service
int upload_session_file(const char *filename, const char *upload_url);

#endif