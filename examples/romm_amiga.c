#include "libromm.h"
#include "libromm_amiga.h"
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { V_PLATFORMS,V_GAMES } view_t;
static int getkey(void){UBYTE c=0;if(Read(Input(),&c,1)!=1)return -1;if(c==0x9b){if(Read(Input(),&c,1)!=1)return -1;if(c=='A')return 1001;if(c=='B')return 1002;return c;}if(c==27){UBYTE a;if(Read(Input(),&a,1)!=1)return 27;if(a=='['&&Read(Input(),&c,1)==1){if(c=='A')return 1001;if(c=='B')return 1002;}return 27;}if(c=='\r'||c=='\n')return 13;return c;}
static void cls(void){printf("\033[2J\033[H");}
static void cut(const char*s,int n){int i=0;if(!s)s="";while(*s&&i<n){char c=*s++;if(c=='\r'||c=='\n')c=' ';putchar(c);i++;}}
static void drawp(const romm_platform_list_t*p,size_t s){size_t i,from=s>8?s-8:0;cls();puts("ROMM Amiga 0.6 - Platforms\n");for(i=from;i<p->count&&i<from+18;i++)printf("%c %-28.28s %6ld\n",i==s?'>':' ',p->items[i].display_name,p->items[i].rom_count);puts("\nUp/Down: select   Return: games   Q: quit");}
static void drawg(const romm_game_list_t*g,size_t s,const romm_game_t*d,long total){size_t i,from=s>7?s-7:0;cls();printf("ROMM Amiga 0.6 - Games (%ld total)\n\n",total);for(i=from;i<g->count&&i<from+15;i++)printf("%c %-35.35s\n",i==s?'>':' ',g->items[i].name);puts("\n----------------------------------------");if(d&&d->name){printf("Name: ");cut(d->name,60);printf("\nFile: ");cut(d->fs_name,60);printf("\nPlatform: ");cut(d->platform_display_name,50);printf("\nDescription: ");cut(d->summary,500);puts("");}puts("\nReturn: download   Esc: back   Q: quit");}
int main(int ac,char**av){romm_client_t c;romm_platform_list_t p={0};romm_game_list_t g={0};romm_game_t d={0};romm_transport_t t;view_t v=V_PLATFORMS;size_t ps=0,gs=0,off=0;long pid=0;int k,rc;if(ac!=3){printf("Usage: %s http://PROXY:PORT TOKEN\n",av[0]);return 2;}t=romm_amiga_transport();rc=romm_client_init(&c,av[1],av[2],t);if(rc)return 1;rc=romm_platforms(&c,&p);if(rc){printf("Platforms: %s\n",romm_strerror(rc));goto done;}for(;;){if(v==V_PLATFORMS)drawp(&p,ps);else drawg(&g,gs,&d,g.total);k=getkey();if(k=='q'||k=='Q')break;if(v==V_PLATFORMS){if(k==1001&&ps)ps--;else if(k==1002&&ps+1<p.count)ps++;else if(k==13&&p.count){pid=p.items[ps].id;off=0;romm_game_list_free(&g);rc=romm_games(&c,pid,50,off,&g);if(!rc&&g.count){gs=0;romm_game_info(&c,g.items[0].id,&d);v=V_GAMES;}}}else{if(k==27){romm_game_free(&d);romm_game_list_free(&g);v=V_PLATFORMS;}else if(k==1001&&gs){gs--;romm_game_free(&d);romm_game_info(&c,g.items[gs].id,&d);}else if(k==1002&&gs+1<g.count){gs++;romm_game_free(&d);romm_game_info(&c,g.items[gs].id,&d);}else if(k==13&&d.fs_name){printf("\nDownloading %s...\n",d.fs_name);rc=romm_download_rom(&c,d.id,d.fs_name);printf(rc?"Download failed: %s\n":"Downloaded: %s\n",rc?romm_strerror(rc):d.fs_name);Delay(75);}}}
done:romm_game_free(&d);romm_game_list_free(&g);romm_platform_list_free(&p);romm_client_destroy(&c);romm_amiga_transport_shutdown();return rc?1:0;}
