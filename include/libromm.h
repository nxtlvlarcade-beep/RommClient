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
int romm_download_file(romm_client_t*,const char*,const char*);
const char *romm_strerror(int);
#endif
