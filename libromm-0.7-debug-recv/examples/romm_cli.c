#include "libromm.h"
#include "libromm_curl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char*x){
    fprintf(stderr,
      "libromm CLI 0.5\n\n"
      "Usage:\n"
      "  %s BASE_URL TOKEN platforms\n"
      "  %s BASE_URL TOKEN games PLATFORM_ID [LIMIT] [OFFSET]\n"
      "  %s BASE_URL TOKEN search PLATFORM_ID TEXT [LIMIT]\n"
      "  %s BASE_URL TOKEN info ROM_ID\n"
      "  %s BASE_URL TOKEN download ROM_ID DESTINATION\n"
      "  %s BASE_URL TOKEN raw API_PATH\n"
      "  %s BASE_URL TOKEN get DOWNLOAD_PATH DESTINATION\n",x,x,x,x,x,x,x);
}
static void print_games(const romm_game_list_t*l){
    size_t i;
    printf("%-8s %-8s %s\n","ROM-ID","PLATFORM","NAME");
    printf("-------- -------- ------------------------------------------------------------\n");
    for(i=0;i<l->count;i++)
        printf("%-8ld %-8ld %s\n",l->items[i].id,l->items[i].platform_id,l->items[i].name);
    printf("\n%lu shown", (unsigned long)l->count);
    if(l->total>(long)l->count)printf(" of %ld",l->total);
    putchar('\n');
}
int main(int ac,char**av){
    romm_client_t c; int rc; romm_transport_t t=romm_curl_transport();
    if(ac<4){usage(av[0]);return 2;}
    rc=romm_client_init(&c,av[1],av[2],t);
    if(rc){fprintf(stderr,"%s\n",romm_strerror(rc));return 1;}

    if(!strcmp(av[3],"platforms")){
        romm_platform_list_t l; size_t i;
        rc=romm_platforms(&c,&l);
        if(!rc){
            printf("%-6s %-8s %-5s %-12s %s\n","ID","ROMS","GEN","CATEGORY","PLATFORM");
            for(i=0;i<l.count;i++){
                romm_platform_t*p=&l.items[i];
                printf("%-6ld %-8ld ",p->id,p->rom_count);
                if(p->generation>=0)printf("%-5ld ",p->generation);else printf("%-5s ","-");
                printf("%-12.12s %s\n",p->category,p->display_name);
            }
            romm_platform_list_free(&l);
        }
    } else if(!strcmp(av[3],"games")&&ac>=5) {
        romm_game_list_t l;
        long pid=strtol(av[4],NULL,10);
        size_t limit=ac>=6?(size_t)strtoul(av[5],NULL,10):100;
        size_t offset=ac>=7?(size_t)strtoul(av[6],NULL,10):0;
        rc=romm_games(&c,pid,limit,offset,&l);
        if(!rc){print_games(&l);romm_game_list_free(&l);}
    } else if(!strcmp(av[3],"search")&&ac>=6) {
        romm_game_list_t l;
        long pid=strtol(av[4],NULL,10);
        size_t limit=ac>=7?(size_t)strtoul(av[6],NULL,10):50;
        rc=romm_search_games(&c,pid,av[5],limit,&l);
        if(!rc){print_games(&l);romm_game_list_free(&l);}
    } else if(!strcmp(av[3],"info")&&ac>=5) {
        romm_game_t g; long id=strtol(av[4],NULL,10);
        rc=romm_game_info(&c,id,&g);
        if(!rc){
            printf("ROM-ID:   %ld\n",g.id);
            printf("Platform: %ld",g.platform_id);
            if(g.platform_display_name&&*g.platform_display_name)printf(" (%s)",g.platform_display_name);
            putchar('\n');
            printf("Name:     %s\n",g.name);
            printf("File:     %s\n",g.fs_name);
            printf("Size:     %llu bytes\n",g.fs_size_bytes);
            if(g.genres&&*g.genres)printf("Genres:   %s\n",g.genres);
            if(g.developers&&*g.developers)printf("Developer:%s%s\n",*g.developers?" ":"",g.developers);
            if(g.publishers&&*g.publishers)printf("Publisher:%s%s\n",*g.publishers?" ":"",g.publishers);
            if(g.regions&&*g.regions)printf("Regions:  %s\n",g.regions);
            if(g.average_rating>0.0)printf("Rating:   %.1f\n",g.average_rating);
            printf("Manual:   %s\n",g.has_manual?"yes":"no");
            printf("Multi:    %s\n",g.has_multiple_files?"yes":"no");
            if(g.summary&&*g.summary)printf("\nDescription:\n%s\n",g.summary);
            romm_game_free(&g);
        }
    } else if(!strcmp(av[3],"download")&&ac>=6) {
        long id=strtol(av[4],NULL,10);
        rc=romm_download_rom(&c,id,av[5]);
        if(!rc)printf("Downloaded ROM %ld to %s\n",id,av[5]);
    } else if(!strcmp(av[3],"raw")&&ac>=5) {
        char*j=NULL;rc=romm_get_json(&c,av[4],&j);if(!rc){puts(j);free(j);}
    } else if(!strcmp(av[3],"get")&&ac>=6) {
        rc=romm_download_file(&c,av[4],av[5]);
    } else {usage(av[0]);rc=ROMM_ERR_ARGUMENT;}

    if(rc)fprintf(stderr,"libromm: %s (%d)\n",romm_strerror(rc),rc);
    romm_client_destroy(&c);return rc?1:0;
}
