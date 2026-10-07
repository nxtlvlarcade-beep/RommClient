#include "libromm_curl.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    size_t size;
} memory_t;

static size_t write_memory(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t n = size * nmemb;
    memory_t *m = (memory_t *)userdata;
    char *p = (char *)realloc(m->data, m->size + n + 1);
    if (!p) return 0;
    m->data = p;
    memcpy(m->data + m->size, ptr, n);
    m->size += n;
    m->data[m->size] = 0;
    return n;
}

static struct curl_slist *headers_for(const char *authorization) {
    struct curl_slist *h = NULL;
    if (authorization && authorization[0]) {
        char line[1024];
        snprintf(line, sizeof(line), "Authorization: %s", authorization);
        h = curl_slist_append(h, line);
    }
    h = curl_slist_append(h, "Accept: application/json");
    return h;
}

static int curl_get(void *userdata, const char *url, const char *authorization,
                    romm_http_response_t *response) {
    CURL *curl;
    CURLcode cc;
    struct curl_slist *headers;
    memory_t mem = {0, 0};
    (void)userdata;

    curl = curl_easy_init();
    if (!curl) return ROMM_ERR_TRANSPORT;
    headers = headers_for(authorization);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mem);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libromm-simple/0.1");

    cc = curl_easy_perform(curl);
    if (cc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (cc != CURLE_OK) {
        free(mem.data);
        return ROMM_ERR_TRANSPORT;
    }
    response->body = mem.data;
    response->body_size = mem.size;
    return ROMM_OK;
}

static size_t write_file(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return fwrite(ptr, size, nmemb, (FILE *)userdata);
}

static int curl_download(void *userdata, const char *url, const char *authorization,
                         const char *destination) {
    CURL *curl;
    CURLcode cc;
    struct curl_slist *headers;
    FILE *f;
    long status = 0;
    (void)userdata;

    f = fopen(destination, "wb");
    if (!f) return ROMM_ERR_IO;

    curl = curl_easy_init();
    if (!curl) { fclose(f); return ROMM_ERR_TRANSPORT; }
    headers = headers_for(authorization);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libromm-simple/0.1");

    cc = curl_easy_perform(curl);
    if (cc == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    fclose(f);

    if (cc != CURLE_OK) {
        remove(destination);
        return ROMM_ERR_TRANSPORT;
    }
    if (status < 200 || status >= 300) {
        remove(destination);
        return ROMM_ERR_HTTP;
    }
    return ROMM_OK;
}

static void curl_free_response(void *userdata, romm_http_response_t *response) {
    (void)userdata;
    if (!response) return;
    free(response->body);
    memset(response, 0, sizeof(*response));
}

romm_transport_t romm_curl_transport(void) {
    romm_transport_t t;
    memset(&t, 0, sizeof(t));
    t.get = curl_get;
    t.download = curl_download;
    t.free_response = curl_free_response;
    return t;
}
