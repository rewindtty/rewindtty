#include "cJSON.h"
#include <string.h>
#include "http.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

HttpParsedUrl get_parsed_url(char *url)
{

    HttpParsedUrl parsed_url = {
        NULL,
        80,
        NULL,
        url};

    char *url_copy = strdup(
        url);

    char *scheme_end = strstr(url_copy, "://");
    char *host_start = url_copy;
    if (scheme_end)
    {
        *scheme_end = '\0';
        host_start = scheme_end + 3;
        char *scheme = strdup(url_copy);

        if (strcmp(scheme, "https") == 0)
        {
            parsed_url.port = 443;
        }
        else if (strcmp(scheme, "http") == 0)
        {
            parsed_url.port = 80;
        }
    }

    char *path_start = strchr(host_start, '/');
    if (path_start)
    {
        parsed_url.path = strdup(path_start);
        *path_start = '\0';
    }
    else
    {
        parsed_url.path = strdup("/");
    }

    char *colon = strchr(path_start, ':');
    if (colon)
    {
        *colon = '\0';
        parsed_url.hostname = strdup(host_start);
        parsed_url.port = atoi(colon + 1);
    }
    else
    {
        parsed_url.hostname = strdup(host_start);
    }

    return parsed_url;
}

int socket_connect(char *host, in_port_t port)
{
    struct hostent *hp;
    struct sockaddr_in addr;
    int on = 1, sock;

    if ((hp = gethostbyname(host)) == NULL)
    {
        herror("gethostbyname");
        exit(1);
    }
    bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
    addr.sin_port = htons(port);
    addr.sin_family = AF_INET;
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

    if (sock == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    {
        perror("connect");
        exit(1);
    }
    return sock;
}

int request_http_post(char *url, cJSON *body)
{
    int fd;
    char buffer[BUFFER_SIZE];

    HttpParsedUrl parsed_url = get_parsed_url(url);
    fd = socket_connect(parsed_url.hostname, parsed_url.port);

    char *json_body = cJSON_PrintUnformatted(body);
    int content_length = strlen(json_body);

    char request[BUFFER_SIZE * 4];
    snprintf(request, sizeof(request),
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             parsed_url.path,
             parsed_url.hostname,
             content_length,
             json_body);

    if (parsed_url.port == 443)
    {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();
        const SSL_METHOD *method = TLS_client_method();
        SSL_CTX *ctx = SSL_CTX_new(method);
        if (!ctx)
        {
            ERR_print_errors_fp(stderr);
            return 1;
        }

        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, fd);

        SSL_set_tlsext_host_name(ssl, parsed_url.hostname);

        if (SSL_connect(ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            SSL_CTX_free(ctx);
            return 1;
        }

        SSL_write(ssl, request, strlen(request));

        while (1)
        {
            int bytes = SSL_read(ssl, buffer, BUFFER_SIZE - 1);
            if (bytes <= 0)
                break;
            buffer[bytes] = 0;
            fprintf(stderr, "%s", buffer);
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
    }
    else
    {
        write(fd, request, strlen(request));
        bzero(buffer, BUFFER_SIZE);
        while (read(fd, buffer, BUFFER_SIZE - 1) > 0)
        {
            fprintf(stderr, "%s", buffer);
            bzero(buffer, BUFFER_SIZE);
        }
    }

    free(json_body);
    shutdown(fd, SHUT_RDWR);
    close(fd);

    return 0;
}
