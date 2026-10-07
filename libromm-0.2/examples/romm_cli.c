#include "libromm.h"
#include "libromm_curl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void usage(const char*x){fprintf(stderr,"Usage:\n  %s BASE_URL TOKEN platforms\n  %s BASE_URL TOKEN raw API_PATH\n  %s BASE_URL TOKEN get DOWNLOAD_PATH DESTINATION\n",x,x,x);}
int main(int ac,char**av){romm_client_t c;int rc;romm_transport_t t=romm_curl_transport();if(ac<4){usage(av[0]);return 2;}rc=romm_client_init(&c,av[1],av[2],t);if(rc)return 1;if(!strcmp(av[3],"platforms")){romm_platform_list_t l;size_t i;rc=romm_platforms(&c,&l);if(!rc){printf("%-6s %-8s %-5s %-12s %s\n","ID","ROMS","GEN","CATEGORY","PLATFORM");for(i=0;i<l.count;i++){romm_platform_t*p=&l.items[i];printf("%-6ld %-8ld ",p->id,p->rom_count);if(p->generation>=0)printf("%-5ld ",p->generation);else printf("%-5s ","-");printf("%-12.12s %s\n",p->category,p->display_name);}romm_platform_list_free(&l);}}else if(!strcmp(av[3],"raw")&&ac>=5){char*j=NULL;rc=romm_get_json(&c,av[4],&j);if(!rc){puts(j);free(j);}}else if(!strcmp(av[3],"get")&&ac>=6)rc=romm_download_file(&c,av[4],av[5]);else{usage(av[0]);rc=ROMM_ERR_ARGUMENT;}if(rc)fprintf(stderr,"libromm: %s (%d)\n",romm_strerror(rc),rc);romm_client_destroy(&c);return rc?1:0;}
