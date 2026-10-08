/* romm-whdload: AmigaOS 3.x local installation and launch helper.
 * Build: make -f Makefile.amiga romm-whdload
 * ADF/IPF conversion requires an installed, game-specific installer.
 */
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PATH_CAP 512
#define CMD_CAP 1200

static int confirm(const char *question)
{
    char answer[16];
    printf("%s [y/N]: ", question);
    fflush(stdout);
    if (!fgets(answer, sizeof(answer), stdin)) return 0;
    return answer[0] == 'y' || answer[0] == 'Y';
}

static int quoted_path(const char *path)
{
    size_t i, n;
    if (!path || !(n = strlen(path)) || n >= PATH_CAP) return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)path[i];
        if (c < 32 || c == 127 || c == '"' || c == '*' || c == '?' || c == '#' || c == '`') return 0;
    }
    return 1;
}

static int relative_slave(const char *path)
{
    const char *p;
    size_t n;
    if (!path || !(n = strlen(path)) || n > 240 || path[0] == '/') return 0;
    if (strchr(path, ':') || strchr(path, '\\') || strchr(path, '"')) return 0;
    for (p = path; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c < 32 || c == 127 || c == '*' || c == '?' || c == '#') return 0;
        if ((p == path || p[-1] == '/') && (p[0] == '.' && (p[1] == '/' || p[1] == 0 ||
            (p[1] == '.' && (p[2] == '/' || p[2] == 0))))) return 0;
    }
    return strstr(path, ".slave") != NULL || strstr(path, ".SLAVE") != NULL;
}

static int create_directory(const char *path)
{
    BPTR lock = CreateDir((STRPTR)path);
    if (!lock) return 0;
    UnLock(lock);
    return 1;
}

static int exists(const char *path)
{
    BPTR lock = Lock((STRPTR)path, ACCESS_READ);
    if (!lock) return 0;
    UnLock(lock);
    return 1;
}

static int execute_two(const char *program, const char *a, const char *b)
{
    char cmd[CMD_CAP];
    if (!quoted_path(program) || !quoted_path(a) || (b && !quoted_path(b))) return 0;
    if (snprintf(cmd, sizeof(cmd), b ? "\"%s\" \"%s\" \"%s\"" : "\"%s\" \"%s\"",
                 program, a, b ? b : "") >= (int)sizeof(cmd)) return 0;
    return Execute((STRPTR)cmd, 0, 0) != 0;
}

static int read_manifest(const char *folder, char *slave, size_t cap)
{
    char manifest[PATH_CAP], full[PATH_CAP];
    FILE *fp;
    size_t n;
    if (snprintf(manifest, sizeof(manifest), "%s/romm-launch.txt", folder) >= (int)sizeof(manifest)) return 0;
    fp = fopen(manifest, "r");
    if (!fp) return 0;
    if (!fgets(slave, (int)cap, fp)) { fclose(fp); return 0; }
    fclose(fp);
    n = strcspn(slave, "\r\n");
    if (slave[n] != '\r' && slave[n] != '\n' && slave[n] != 0) return 0;
    slave[n] = 0;
    if (!relative_slave(slave)) return 0;
    if (snprintf(full, sizeof(full), "%s/%s", folder, slave) >= (int)sizeof(full)) return 0;
    return exists(full);
}

static int launch(const char *folder)
{
    char slave[256], full[PATH_CAP];
    const char *whdload = getenv("ROMM_LAUNCHER");
    if (!whdload || !*whdload) whdload = "C:WHDLoad";
    if (!read_manifest(folder, slave, sizeof(slave))) {
        puts("No valid romm-launch.txt or .slave. Installation is not complete.");
        return 1;
    }
    if (snprintf(full, sizeof(full), "%s/%s", folder, slave) >= (int)sizeof(full)) return 1;
    printf("Ready: %s\n", full);
    if (!confirm("Start WHDLoad now?")) return 0;
    if (!execute_two(whdload, full, NULL)) {
        puts("WHDLoad could not be started. Check C:WHDLoad and game data.");
        return 1;
    }
    return 0;
}

static int ends_with(const char *s, const char *suffix)
{
    size_t i, a = strlen(s), b = strlen(suffix);
    if (a < b) return 0;
    for (i = 0; i < b; ++i)
        if (tolower((unsigned char)s[a-b+i]) != tolower((unsigned char)suffix[i])) return 0;
    return 1;
}

static int prepare_folder(const char *input, char *folder, size_t cap)
{
    const char *base = input, *p;
    size_t i = 0;
    for (p = input; *p; ++p) if (*p == '/' || *p == ':') base = p + 1;
    if (cap < 20) return 0;
    strcpy(folder, "romm-whd-");
    i = strlen(folder);
    while (*base && *base != '.' && i < cap - 2 && i < 64) {
        unsigned char c = (unsigned char)*base++;
        folder[i++] = isalnum(c) || c == '-' || c == '_' ? (char)c : '_';
    }
    if (i == strlen("romm-whd-")) return 0;
    folder[i] = 0;
    return 1;
}

int main(int argc, char **argv)
{
    const char *input, *installer, *mounter;
    char folder[PATH_CAP], slave[256];
    int is_zip, is_disk;
    if (argc != 2) {
        puts("Usage: romm-whdload <downloaded-file-or-installed-folder>");
        puts("Supports prepared WHDLoad ZIPs and installed folders.");
        puts("ADF/IPF requires ROMM_WHD_INSTALLER (game-specific tool).");
        puts("ADF mounting optionally uses ROMM_ADF_MOUNTER.");
        return 2;
    }
    input = argv[1];
    if (!quoted_path(input) || !exists(input)) {
        puts("Invalid or missing input path.");
        return 2;
    }
    if (read_manifest(input, slave, sizeof(slave))) {
        /* A directory with a valid manifest is already installed. */
        return launch(input);
    }
    is_zip = ends_with(input, ".zip");
    is_disk = ends_with(input, ".adf") || ends_with(input, ".ipf");
    if (!is_zip && !is_disk) {
        puts("Unsupported input. Expected prepared ZIP, ADF or IPF.");
        return 2;
    }
    if (!prepare_folder(input, folder, sizeof(folder))) return 2;
    if (exists(folder)) {
        puts("Destination already exists; refusing to overwrite existing game data.");
        puts("Use the existing installed folder, or choose a clean directory.");
        return 1;
    }
    if (is_zip) {
        if (!confirm("Extract trusted WHDLoad ZIP with C:UnZip?")) return 0;
        if (!create_directory(folder)) { puts("Cannot create game directory."); return 1; }
        {
            char cmd[CMD_CAP];
            if (snprintf(cmd, sizeof(cmd), "C:UnZip -o \"%s\" -d \"%s\"", input, folder) >= (int)sizeof(cmd) ||
                !Execute((STRPTR)cmd, 0, 0)) {
            puts("UnZip failed. Install UnZip or inspect the archive.");
            return 1;
            }
        }
        return launch(folder);
    }
    puts("ADF/IPF is original disk media, not an installed WHDLoad game.");
    installer = getenv("ROMM_WHD_INSTALLER");
    if (installer && *installer) {
        puts("Configured installer must support: <disk-image> <output-folder>.");
        if (confirm("Run the configured game-specific installer?")) {
            if (!create_directory(folder)) { puts("Cannot create output directory."); return 1; }
            if (!execute_two(installer, input, folder)) {
                puts("Installer failed; no playable game has been produced.");
                return 1;
            }
            return launch(folder);
        }
    } else {
        puts("No ROMM_WHD_INSTALLER configured. Cannot convert this disk image.");
    }
    mounter = getenv("ROMM_ADF_MOUNTER");
    if (ends_with(input, ".adf") && mounter && *mounter) {
        puts("ADF mounting is separate from WHDLoad installation.");
        if (confirm("Mount the ADF with the configured mounter?")) {
            if (!execute_two(mounter, input, NULL)) {
                puts("ADF mount failed.");
                return 1;
            }
            puts("Mount command completed. Check your virtual floppy drive.");
            puts("No automatic reset will be performed.");
        }
    }
    return 0;
}
