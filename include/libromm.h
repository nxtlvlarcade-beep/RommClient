#ifndef LIBROMM_H
#define LIBROMM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROMM_OK 0
#define ROMM_ERR_ARGUMENT -1
#define ROMM_ERR_TRANSPORT -2
#define ROMM_ERR_HTTP -3
#define ROMM_ERR_MEMORY -4
#define ROMM_ERR_PARSE -5
#define ROMM_ERR_IO -6

typedef struct {
    long status;
    char *body;
    size_t body_size;
} romm_http_response_t;

typedef int (*romm_http_get_fn)(
    void *userdata,
    const char *url,
    const char *authorization,
    romm_http_response_t *response
);

typedef int (*romm_http_download_fn)(
    void *userdata,
    const char *url,
    const char *authorization,
    const char *destination
);

typedef void (*romm_http_free_fn)(void *userdata, romm_http_response_t *response);

typedef struct {
    romm_http_get_fn get;
    romm_http_download_fn download;
    romm_http_free_fn free_response;
    void *userdata;
} romm_transport_t;

typedef struct {
    char *base_url;
    char *token;
    romm_transport_t transport;
} romm_client_t;

typedef struct {
    long id;
    char *name;
    char *platform;
} romm_game_t;

typedef struct {
    romm_game_t *items;
    size_t count;
} romm_game_list_t;

int romm_client_init(
    romm_client_t *client,
    const char *base_url,
    const char *token,
    romm_transport_t transport
);

void romm_client_destroy(romm_client_t *client);

/*
 * Minimal endpoint helpers.
 *
 * API paths can change between RomM releases. These defaults are intentionally
 * isolated here so a platform/app can replace them without changing the
 * transport layer.
 */
int romm_get_json(
    romm_client_t *client,
    const char *api_path,
    char **json_out
);

int romm_list_games(
    romm_client_t *client,
    const char *api_path,
    romm_game_list_t *out
);

int romm_download_file(
    romm_client_t *client,
    const char *download_path,
    const char *destination
);

void romm_game_list_free(romm_game_list_t *list);

#ifdef __cplusplus
}
#endif
#endif
