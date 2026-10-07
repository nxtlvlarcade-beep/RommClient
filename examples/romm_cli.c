#include "libromm.h"
#include "libromm_curl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *exe) {
    fprintf(stderr,
        "Usage:\n"
        "  %s BASE_URL TOKEN raw API_PATH\n"
        "  %s BASE_URL TOKEN list API_PATH\n"
        "  %s BASE_URL TOKEN get DOWNLOAD_PATH DESTINATION\n\n"
        "Examples:\n"
        "  %s http://192.168.1.10:8080 TOKEN raw /api/platforms\n"
        "  %s http://192.168.1.10:8080 TOKEN list /api/roms\n",
        exe, exe, exe, exe, exe);
}

int main(int argc, char **argv) {
    romm_client_t client;
    romm_transport_t transport;
    int rc;

    if (argc < 5) { usage(argv[0]); return 2; }

    transport = romm_curl_transport();
    rc = romm_client_init(&client, argv[1], argv[2], transport);
    if (rc != ROMM_OK) {
        fprintf(stderr, "client init failed: %d\n", rc);
        return 1;
    }

    if (strcmp(argv[3], "raw") == 0) {
        char *json = NULL;
        rc = romm_get_json(&client, argv[4], &json);
        if (rc == ROMM_OK) {
            puts(json);
            free(json);
        }
    } else if (strcmp(argv[3], "list") == 0) {
        romm_game_list_t games;
        size_t i;
        rc = romm_list_games(&client, argv[4], &games);
        if (rc == ROMM_OK) {
            for (i = 0; i < games.count; ++i)
                printf("%ld\t%s\t%s\n",
                    games.items[i].id,
                    games.items[i].platform,
                    games.items[i].name);
            romm_game_list_free(&games);
        }
    } else if (strcmp(argv[3], "get") == 0 && argc >= 6) {
        rc = romm_download_file(&client, argv[4], argv[5]);
    } else {
        usage(argv[0]);
        rc = ROMM_ERR_ARGUMENT;
    }

    if (rc != ROMM_OK) fprintf(stderr, "libromm error: %d\n", rc);
    romm_client_destroy(&client);
    return rc == ROMM_OK ? 0 : 1;
}
