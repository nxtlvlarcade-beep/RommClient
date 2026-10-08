#include "libromm.h"
#include "libromm_amiga.h"
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { V_PLATFORMS,V_GAMES } view_t;
#define PAGE_GAMES 50
#define PLATFORM_ROWS 18
#define GAME_ROWS 11
#define KEY_UP 1001
#define KEY_DOWN 1002
#define KEY_LEFT 1003
#define KEY_RIGHT 1004

static int getkey(void)
{
    UBYTE c=0,a=0;
    if(Read(Input(),&c,1)!=1) return -1;
    if(c==0x9b) {
        if(Read(Input(),&c,1)!=1) return -1;
        if(c=='A') return KEY_UP;
        if(c=='B') return KEY_DOWN;
        if(c=='C') return KEY_RIGHT;
        if(c=='D') return KEY_LEFT;
        return c;
    }
    if(c==27) {
        /* Some console handlers use ESC [ A/B, others return ESC alone. */
        if(WaitForChar(Input(),2000)) {
            if(Read(Input(),&a,1)==1 && a=='[' && Read(Input(),&c,1)==1) {
                if(c=='A') return KEY_UP;
                if(c=='B') return KEY_DOWN;
        if(c=='C') return KEY_RIGHT;
        if(c=='D') return KEY_LEFT;
            }
        }
        return 27;
    }
    if(c=='\r'||c=='\n') return 13;
    if(c=='w'||c=='W'||c=='k'||c=='K') return KEY_UP;
    if(c=='s'||c=='S'||c=='j'||c=='J') return KEY_DOWN;
    if(c=='b'||c=='B') return 27;
    return c;
}

static void cls(void){printf("\2332J\233H");fflush(stdout);}
static void marker(int selected){if(selected) printf("\273 "); else printf("  ");}
static void cut(const char*s,int n){int i=0;if(!s)s="";while(*s&&i<n){char c=*s++;if(c=='\r'||c=='\n')c=' ';putchar(c);i++;}}


/* Keep the description inside the console viewport; never scroll the header away. */
static void wrap_text(const char *s,int width,int maxlines)
{
    int col=0,lines=0; const char *p,*q;
    if(!s||!*s){puts("(no description)");return;}
    p=s;
    while(*p && lines<maxlines){
        int word, i;
        while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
        if(!*p)break;
        q=p;word=0;
        while(q[word]&&q[word]!=' '&&q[word]!='\t'&&q[word]!='\r'&&q[word]!='\n')word++;
        if(col && col+1+word>width){putchar('\n');lines++;col=0;}
        if(lines>=maxlines)break;
        if(col){putchar(' ');col++;}
        for(i=0;i<word && lines<maxlines;i++){
            if(col>=width){putchar('\n');lines++;col=0;}
            if(lines<maxlines){putchar(*p++);col++;}
        }
    }
    if(lines<maxlines && col)putchar('\n');
    if(*p)puts("... (description truncated)");
}

static void drawp(const romm_platform_list_t*p,size_t s,size_t top)
{
    size_t i,end=top+PLATFORM_ROWS;
    if(end>p->count) end=p->count;
    cls(); puts("ROMM Amiga 1.0-dev - Platforms\n");
    if(top) puts("  ^ more");
    for(i=top;i<end;i++) {
        marker(i==s);
        printf("%-28.28s %6ld\n",p->items[i].display_name?p->items[i].display_name:"",p->items[i].rom_count);
    }
    if(end<p->count) puts("  v more");
    puts("\nUp/Down or W/S: select   Return: games   Q: quit");
}

static void drawg(const romm_game_list_t*g,size_t s,size_t top,const romm_game_t*d,long total,size_t off)
{
    size_t i,end=top+GAME_ROWS;
    if(end>g->count)end=g->count;
    cls();
    printf("ROMM Amiga 1.0-dev - Games\n");
    printf("Game %lu / %ld  (loaded %lu-%lu)\n",(unsigned long)(off+s+1),total,
        (unsigned long)(off+1),(unsigned long)(off+g->count));
    for(i=top;i<end;i++){
        marker(i==s);
        printf("%-35.35s\n",g->items[i].name?g->items[i].name:"");
    }
    /* Reserve fixed screen rows for the list, even on the final page. */
    for(i=end-top;i<GAME_ROWS;i++)putchar('\n');
    printf("%s  %s\n",(off||top)?"^ more":"      ",
        (off+g->count<(size_t)total||end<g->count)?"v more":"      ");
    puts("----------------------------------------");
    if(d&&d->name){
        printf("Name: ");cut(d->name,48);putchar('\n');
        printf("File: ");cut(d->fs_name,48);putchar('\n');
        printf("Platform: ");cut(d->platform_display_name,40);putchar('\n');
        printf("Description: ");wrap_text(d->summary,60,3);
    }
    puts("/: search   Left/Right: A-Z   D: RAW download");
    puts("Return: WHDLoad (if available)   Esc/B: back   Q: quit");
}

static int prompt_search(char *buf,size_t n)
{
    size_t i=0; UBYTE c=0;
    if(!buf||n<2)return 0;
    SetMode(Input(),0);
    cls();printf("Search games in platform: ");fflush(stdout);
    if(fgets(buf,(int)n,stdin)==NULL){SetMode(Input(),1);return 0;}
    SetMode(Input(),1);
    while(buf[i] && buf[i]!='\n' && buf[i]!='\r')i++;
    buf[i]=0;
    (void)c;
    return i>0;
}
static int search_page(romm_client_t*c,long pid,const char*term,romm_game_list_t*g,
                       romm_game_t*d,size_t*gs,size_t*gtop,size_t*off)
{
    romm_game_list_t ng={0};int rc=romm_search_games(c,pid,term,500,&ng);
    if(rc)return rc;
    romm_game_free(d);romm_game_list_free(g);*g=ng;*gs=*gtop=*off=0;
    if(g->count)return romm_game_info(c,g->items[0].id,d);
    return ROMM_OK;
}
static int load_info(romm_client_t *c,romm_game_list_t *g,size_t gs,romm_game_t *d)
{
    int rc; romm_game_free(d); if(!g->count) return ROMM_OK;
    rc=romm_game_info(c,g->items[gs].id,d); return rc;
}

/* UnZip must be installed in C: or PATH. WHDLoad must be installed separately. */
static int whdload_game(romm_client_t *c,long id)
{
    char archive[80],folder[80],manifest[120],slave[256],cmd[650];
    char *launcher=getenv("ROMM_LAUNCHER");
    FILE *fp; size_t n; int rc;
    if(!launcher||!*launcher)launcher="C:WHDLoad";
    if(strchr(launcher,'"')||strchr(launcher,'\n'))return -1;
    sprintf(archive,"romm-%ld-whdload.zip",id);
    sprintf(folder,"romm-%ld",id);
    sprintf(manifest,"%s/romm-launch.txt",folder);
    sprintf(cmd,"/whdload/%ld",id);
    puts("Requesting prepared WHDLoad package...");
    rc=romm_download_file(c,cmd,archive);
    if(rc){printf("WHDLoad unavailable: %s\n",romm_strerror(rc));return -1;}
    /* Numeric-only output directory and archive names are safe to quote. */
    sprintf(cmd,"UnZip -o \"%s\" -d \"%s\"",archive,folder);
    if(!Execute((STRPTR)cmd,0,0)){
        puts("UnZip failed. Install UnZip in C: or PATH.");return -1;
    }
    fp=fopen(manifest,"r");
    if(!fp){puts("Missing WHDLoad manifest");return -1;}
    if(!fgets(slave,sizeof(slave),fp)){fclose(fp);return -1;}
    fclose(fp);
    n=strcspn(slave,"\r\n");slave[n]=0;
    /* Restrict manifest to a safe relative path; never execute arbitrary commands. */
    if(!n||strstr(slave,"..")||strchr(slave,':')||strchr(slave,'"')||slave[0]=='/'||strchr(slave,'\\')){
        puts("Unsafe slave path");return -1;
    }
    sprintf(cmd,"\"%s\" \"%s/%s\"",launcher,folder,slave);
    printf("Starting %s\n",slave);
    if(!Execute((STRPTR)cmd,0,0)){puts("WHDLoad launch failed");return -1;}
    return 0;
}

int main(int ac,char**av)
{
    romm_client_t c; romm_platform_list_t p={0}; romm_game_list_t g={0}; romm_game_t d={0};
    romm_transport_t t; view_t v=V_PLATFORMS; size_t ps=0,ptop=0,gs=0,gtop=0,off=0; long pid=0; int k,rc=0,raw=0,debug=0; char query[128]=""; int filtered=0; char letter='A';
    if(ac==4 && !strcmp(av[3],"--debug")) debug=1;
    else if(ac!=3){printf("Usage: %s http://PROXY:PORT TOKEN [--debug]\n",av[0]);return 2;}
    romm_amiga_set_debug(debug);
    t=romm_amiga_transport();
    rc=romm_client_init(&c,av[1],av[2],t); if(rc){printf("client init: %s\n",romm_strerror(rc));goto shutdown;}
    rc=romm_platforms(&c,&p); if(rc){printf("Platforms: %s\n",romm_strerror(rc));goto done;}

    /* Amiga console must be RAW or cursor keys are line-buffered. */
    if(SetMode(Input(),1)) raw=1;

    for(;;) {
        if(v==V_PLATFORMS) drawp(&p,ps,ptop); else drawg(&g,gs,gtop,&d,g.total,off);
        k=getkey(); if(k=='q'||k=='Q') break;
        if(v==V_PLATFORMS) {
            if(k==KEY_UP && ps) { ps--; if(ps<ptop) ptop=ps; }
            else if(k==KEY_DOWN && ps+1<p.count) { ps++; if(ps>=ptop+PLATFORM_ROWS) ptop=ps-PLATFORM_ROWS+1; }
            else if(k==13 && p.count) {
                pid=p.items[ps].id; filtered=0; query[0]=0; letter='A'; off=0; gs=gtop=0; romm_game_list_free(&g); romm_game_free(&d);
                rc=romm_games(&c,pid,PAGE_GAMES,off,&g);
                if(!rc&&g.count) { rc=load_info(&c,&g,0,&d); if(!rc)v=V_GAMES; }
            }
        } else {
            if(k==27) { romm_game_free(&d);romm_game_list_free(&g);v=V_PLATFORMS; }
            else if(k=='/') {
                if(prompt_search(query,sizeof(query))){filtered=1;rc=search_page(&c,pid,query,&g,&d,&gs,&gtop,&off);}
            }
            else if(k==KEY_LEFT||k==KEY_RIGHT){
                int step=(k==KEY_RIGHT?1:25), tries;
                for(tries=0;tries<26;tries++) {
                    romm_game_list_t ng={0};
                    letter=(char)((letter-'A'+step)%26+'A');
                    rc=romm_letter_games(&c,pid,letter,500,&ng);
                    if(rc){romm_game_list_free(&ng);break;}
                    if(ng.count){
                        romm_game_free(&d);romm_game_list_free(&g);
                        g=ng;gs=gtop=off=0;filtered=1;
                        query[0]=letter;query[1]=0;
                        rc=load_info(&c,&g,0,&d);break;
                    }
                    romm_game_list_free(&ng);
                }
            }
            else if((k=='d'||k=='D')&&d.fs_name){
                cls();printf("RAW download %s...\n",d.fs_name);
                rc=romm_download_rom(&c,d.id,d.fs_name);
                printf(rc?"Download failed: %s\n":"Downloaded: %s\n",rc?romm_strerror(rc):d.fs_name);Delay(75);
            }
            else if(k==KEY_UP) {
                if(gs) { gs--; if(gs<gtop) gtop=gs; rc=load_info(&c,&g,gs,&d); }
                else if(!filtered&&off>=PAGE_GAMES) {
                    size_t newoff=off-PAGE_GAMES; romm_game_list_t ng={0};
                    rc=romm_games(&c,pid,PAGE_GAMES,newoff,&ng);
                    if(!rc&&ng.count){romm_game_free(&d);romm_game_list_free(&g);g=ng;off=newoff;gs=g.count-1;gtop=(g.count>GAME_ROWS)?g.count-GAME_ROWS:0;rc=load_info(&c,&g,gs,&d);} else romm_game_list_free(&ng);
                }
            }
            else if(k==KEY_DOWN) {
                if(gs+1<g.count) { gs++; if(gs>=gtop+GAME_ROWS) gtop=gs-GAME_ROWS+1; rc=load_info(&c,&g,gs,&d); }
                else if(!filtered&&off+g.count<(size_t)g.total) {
                    size_t newoff=off+g.count; romm_game_list_t ng={0};
                    rc=romm_games(&c,pid,PAGE_GAMES,newoff,&ng);
                    if(!rc&&ng.count){romm_game_free(&d);romm_game_list_free(&g);g=ng;off=newoff;gs=gtop=0;rc=load_info(&c,&g,0,&d);} else romm_game_list_free(&ng);
                }
            }
            else if(k==13&&d.fs_name){
                cls();whdload_game(&c,d.id);puts("Press a key to continue.");getkey();
            }
        }
    }
    if(raw) SetMode(Input(),0);
done:
    romm_game_free(&d);romm_game_list_free(&g);romm_platform_list_free(&p);romm_client_destroy(&c);
shutdown:
    romm_amiga_transport_shutdown();return rc?1:0;
}
