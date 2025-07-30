typedef struct
{
    char *hostname;
    int port;
    char *path;
    char *url;
} HttpParsedUrl;

#define BUFFER_SIZE 1024

HttpParsedUrl get_parsed_url(char *url);

int request_http_post(
    char *url,
    cJSON *body);