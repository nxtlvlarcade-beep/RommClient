#ifndef LIBROMM_H
#define LIBROMM_H
#include <stddef.h>
#define ROMM_OK 0
#define ROMM_ERR_ARGUMENT -1
#define ROMM_ERR_TRANSPORT -2
#define ROMM_ERR_HTTP -3
#define ROMM_ERR_MEMORY -4
#define ROMM_ERR_PARSE -5
#define ROMM_ERR_IO -6
typedef struct { long status; char *body; size_t body_size; } romm_http_response_t;
typedef int (*romm_http_get_fn)(void*,const char*,const char*,romm_http_response_t*);
typedef int (*romm_http_download_fn)(void*,const char*,const char*,const char*);
typedef void (*romm_http_free_fn)(void*,romm_http_response_t*);
typedef struct { romm_http_get_fn get; romm_http_download_fn download; romm_http_free_fn free_response; void *userdata; } romm_transport_t;
typedef struct { char *base_url; char *token; romm_transport_t transport; } romm_client_t;
typedef struct { long id,rom_count,generation; unsigned long long fs_size_bytes; int is_identified,missing_from_fs; char *slug,*name,*display_name,*category,*family_name; } romm_platform_t;
typedef struct { romm_platform_t *items; size_t count; } romm_platform_list_t;
int romm_client_init(romm_client_t*,const char*,const char*,romm_transport_t);
void romm_client_destroy(romm_client_t*);
int romm_get_json(romm_client_t*,const char*,char**);
int romm_platforms(romm_client_t*,romm_platform_list_t*);
void romm_platform_list_free(romm_platform_list_t*);

typedef struct {
    long id;
    long platform_id;
    char *name;
    char *fs_name;
    char *platform_display_name;
    char *summary;
    char *genres;
    char *developers;
    char *publishers;
    char *game_modes;
    char *regions;
    char *path_cover_small;
    char *path_cover_large;
    unsigned long long fs_size_bytes;
    long long first_release_date;
    double average_rating;
    int has_manual;
    int has_multiple_files;
} romm_game_t;

typedef struct {
    romm_game_t *items;
    size_t count;
    long total;
} romm_game_list_t;

int romm_games(romm_client_t *client, long platform_id,
               size_t limit, size_t offset, romm_game_list_t *out);
int romm_search_games(romm_client_t *client, long platform_id,
                      const char *text, size_t limit, romm_game_list_t *out);
int romm_letter_games(romm_client_t *client, long platform_id, char letter,
                      size_t limit, romm_game_list_t *out);
void romm_game_list_free(romm_game_list_t *list);

int romm_game_info(romm_client_t *client, long rom_id, romm_game_t *out);
void romm_game_free(romm_game_t *game);
int romm_download_rom(romm_client_t *client, long rom_id,
                      const char *destination);

int romm_download_file(romm_client_t*,const char*,const char*);
const char *romm_strerror(int);
#endif
