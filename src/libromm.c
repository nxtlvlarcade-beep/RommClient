#include "libromm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *romm_strdup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static char *join_url(const char *base, const char *path) {
    size_t a, b;
    int slash_a, slash_b;
    char *out;

    if (!base || !path) return NULL;
    a = strlen(base);
    b = strlen(path);
    slash_a = a > 0 && base[a - 1] == '/';
    slash_b = b > 0 && path[0] == '/';

    out = (char *)malloc(a + b + 2);
    if (!out) return NULL;

    strcpy(out, base);
    if (!slash_a && !slash_b) strcat(out, "/");
    else if (slash_a && slash_b) path++;
    strcat(out, path);
    return out;
}

static char *auth_header(const romm_client_t *client) {
    const char *prefix = "Bearer ";
    char *out;
    size_t n;
    if (!client || !client->token || !client->token[0]) return NULL;
    n = strlen(prefix) + strlen(client->token) + 1;
    out = (char *)malloc(n);
    if (!out) return NULL;
    strcpy(out, prefix);
    strcat(out, client->token);
    return out;
}

int romm_client_init(
    romm_client_t *client,
    const char *base_url,
    const char *token,
    romm_transport_t transport
) {
    if (!client || !base_url || !transport.get) return ROMM_ERR_ARGUMENT;
    memset(client, 0, sizeof(*client));
    client->base_url = romm_strdup(base_url);
    client->token = romm_strdup(token ? token : "");
    client->transport = transport;
    if (!client->base_url || !client->token) {
        romm_client_destroy(client);
        return ROMM_ERR_MEMORY;
    }
    return ROMM_OK;
}

void romm_client_destroy(romm_client_t *client) {
    if (!client) return;
    free(client->base_url);
    free(client->token);
    memset(client, 0, sizeof(*client));
}

int romm_get_json(
    romm_client_t *client,
    const char *api_path,
    char **json_out
) {
    char *url, *auth;
    romm_http_response_t response;
    int rc;

    if (!client || !api_path || !json_out || !client->transport.get)
        return ROMM_ERR_ARGUMENT;

    *json_out = NULL;
    memset(&response, 0, sizeof(response));

    url = join_url(client->base_url, api_path);
    auth = auth_header(client);
    if (!url) { free(auth); return ROMM_ERR_MEMORY; }

    rc = client->transport.get(client->transport.userdata, url, auth, &response);
    free(url);
    free(auth);

    if (rc != ROMM_OK) return ROMM_ERR_TRANSPORT;
    if (response.status < 200 || response.status >= 300) {
        if (client->transport.free_response)
            client->transport.free_response(client->transport.userdata, &response);
        return ROMM_ERR_HTTP;
    }

    if (!response.body) {
        if (client->transport.free_response)
            client->transport.free_response(client->transport.userdata, &response);
        return ROMM_ERR_PARSE;
    }

    *json_out = romm_strdup(response.body);
    if (client->transport.free_response)
        client->transport.free_response(client->transport.userdata, &response);

    return *json_out ? ROMM_OK : ROMM_ERR_MEMORY;
}

/*
 * Tiny dependency-free demo parser.
 * It understands a deliberately small JSON subset:
 *   [{"id":1,"name":"Game","platform":"amiga"}, ...]
 *
 * Production code should plug in a real small JSON parser (e.g. jsmn/cJSON)
 * behind this module. Keeping parsing here demonstrates that UI/platform code
 * does not need to know about RomM's wire format.
 */
static const char *find_key(const char *p, const char *key) {
    char pattern[64];
    if (strlen(key) + 3 >= sizeof(pattern)) return NULL;
    sprintf(pattern, "\"%s\"", key);
    return strstr(p, pattern);
}

static char *read_string_value(const char *obj, const char *key) {
    const char *p = find_key(obj, key), *q, *e;
    char *s;
    size_t n;
    if (!p) return romm_strdup("");
    q = strchr(p, ':');
    if (!q) return romm_strdup("");
    q++;
    while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
    if (*q != '"') return romm_strdup("");
    q++;
    e = q;
    while (*e && *e != '"') {
        if (*e == '\\' && e[1]) e++;
        e++;
    }
    n = (size_t)(e - q);
    s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, q, n);
    s[n] = 0;
    return s;
}

static long read_long_value(const char *obj, const char *key) {
    const char *p = find_key(obj, key);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    return strtol(p + 1, NULL, 10);
}

int romm_list_games(
    romm_client_t *client,
    const char *api_path,
    romm_game_list_t *out
) {
    char *json = NULL;
    const char *p;
    size_t capacity = 0;
    int rc;

    if (!out) return ROMM_ERR_ARGUMENT;
    memset(out, 0, sizeof(*out));

    rc = romm_get_json(client, api_path, &json);
    if (rc != ROMM_OK) return rc;

    p = json;
    while ((p = strchr(p, '{')) != NULL) {
        const char *end = strchr(p, '}');
        char *obj;
        size_t n;
        romm_game_t *grown, *g;

        if (!end) break;
        n = (size_t)(end - p + 1);
        obj = (char *)malloc(n + 1);
        if (!obj) { rc = ROMM_ERR_MEMORY; break; }
        memcpy(obj, p, n);
        obj[n] = 0;

        if (out->count == capacity) {
            size_t next = capacity ? capacity * 2 : 8;
            grown = (romm_game_t *)realloc(out->items, next * sizeof(*grown));
            if (!grown) {
                free(obj);
                rc = ROMM_ERR_MEMORY;
                break;
            }
            out->items = grown;
            capacity = next;
        }

        g = &out->items[out->count];
        memset(g, 0, sizeof(*g));
        g->id = read_long_value(obj, "id");
        g->name = read_string_value(obj, "name");
        g->platform = read_string_value(obj, "platform");

        free(obj);
        if (!g->name || !g->platform) {
            free(g->name);
            free(g->platform);
            rc = ROMM_ERR_MEMORY;
            break;
        }
        out->count++;
        p = end + 1;
    }

    free(json);
    if (rc != ROMM_OK) {
        romm_game_list_free(out);
        return rc;
    }
    return ROMM_OK;
}

int romm_download_file(
    romm_client_t *client,
    const char *download_path,
    const char *destination
) {
    char *url, *auth;
    int rc;

    if (!client || !download_path || !destination || !client->transport.download)
        return ROMM_ERR_ARGUMENT;

    url = join_url(client->base_url, download_path);
    auth = auth_header(client);
    if (!url) { free(auth); return ROMM_ERR_MEMORY; }

    rc = client->transport.download(
        client->transport.userdata, url, auth, destination
    );

    free(url);
    free(auth);
    return rc;
}

void romm_game_list_free(romm_game_list_t *list) {
    size_t i;
    if (!list) return;
    for (i = 0; i < list->count; ++i) {
        free(list->items[i].name);
        free(list->items[i].platform);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}
