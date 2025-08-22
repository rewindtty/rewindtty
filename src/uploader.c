#include "uploader.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

struct UploadResponse
{
    char *data;
    size_t size;
};

static size_t write_response_callback(void *contents, size_t size, size_t nmemb, struct UploadResponse *response)
{
    size_t realsize = size * nmemb;
    char *ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr)
    {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

char *get_curl_useragent(void)
{

    const char *prefix = "rewindtty-cli/";
    size_t ua_str_len = strlen(prefix) + strlen(REWINDTTY_VERSION) + 1;

    char *buf = malloc(ua_str_len);

    if (!buf)
    {
        return prefix;
    }

    strcpy(buf, "rewindtty-cli/");
    strcat(buf, REWINDTTY_VERSION);
    return buf;
}

int upload_session_data(const char *json_data, const char *upload_url)
{
    CURL *curl;
    CURLcode res;
    struct UploadResponse response = {0};
    int success = 0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl)
    {
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, upload_url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(json_data));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, get_curl_useragent());

        res = curl_easy_perform(curl);

        if (res == CURLE_OK)
        {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            if (response_code >= 200 && response_code < 300)
            {
                printf("Upload successful!\n");

                if (response.data)
                {
                    cJSON *json = cJSON_Parse(response.data);
                    if (json)
                    {
                        cJSON *id_field = cJSON_GetObjectItemCaseSensitive(json, "id");
                        char *id_value;

                        if (id_field && cJSON_IsString(id_field))
                        {
                            id_value = malloc(strlen(id_field->valuestring) + 1);
                            if (id_value)
                            {
                                strcpy(id_value, id_field->valuestring);
                            }
                        }
                        else if (id_field && cJSON_IsNumber(id_field))
                        {
                            char buffer[64];
                            snprintf(buffer, sizeof(buffer), "%.0f", id_field->valuedouble);
                            id_value = malloc(strlen(buffer) + 1);
                            if (id_value)
                            {
                                strcpy(id_value, buffer);
                            }
                        }

                        if (id_value)
                        {
                            printf("You can check here: %s/%s\n", PLAYER_URL, id_value);
                            free(id_value);
                        }

                        cJSON_Delete(json);
                    }
                    else
                    {
                        printf("Failed to parse JSON response\n");
                    }
                }

                success = 1;
            }
            else
            {
                printf("Upload failed with HTTP status %ld\n", response_code);
                if (response.data)
                {
                    printf("Error response: %s\n", response.data);
                }
            }
        }
        else
        {
            printf("Upload failed: %s\n", curl_easy_strerror(res));
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    if (response.data)
    {
        free(response.data);
    }

    curl_global_cleanup();
    return success;
}

int upload_session_file(const char *filename, const char *upload_url)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error: Cannot open file %s\n", filename);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *json_data = malloc(file_size + 1);
    if (!json_data)
    {
        printf("Error: Cannot allocate memory for file content\n");
        fclose(file);
        return 0;
    }

    size_t read_size = fread(json_data, 1, file_size, file);
    json_data[read_size] = '\0';
    fclose(file);

    // Validate JSON structure
    cJSON *json = cJSON_Parse(json_data);
    if (!json)
    {
        printf("Error: Invalid JSON format in file %s\n", filename);
        free(json_data);
        return 0;
    }

    // Check for required fields
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(json, "metadata");
    cJSON *sessions = cJSON_GetObjectItemCaseSensitive(json, "sessions");

    if (!metadata)
    {
        printf("Error: Missing 'metadata' field in JSON\n");
        cJSON_Delete(json);
        free(json_data);
        return 0;
    }

    if (!sessions)
    {
        printf("Error: Missing 'sessions' field in JSON\n");
        cJSON_Delete(json);
        free(json_data);
        return 0;
    }

    if (!cJSON_IsArray(sessions))
    {
        printf("Error: 'sessions' field must be an array\n");
        cJSON_Delete(json);
        free(json_data);
        return 0;
    }

    cJSON_Delete(json);

    int result = upload_session_data(json_data, upload_url);

    free(json_data);
    return result;
}
