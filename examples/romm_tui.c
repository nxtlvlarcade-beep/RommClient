#define _POSIX_C_SOURCE 200809L
#include "libromm.h"
#include "libromm_curl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

typedef enum { VIEW_PLATFORMS, VIEW_GAMES } view_t;
static struct termios saved_term;
static int raw_active=0;
static void term_restore(void){if(raw_active){tcsetattr(STDIN_FILENO,TCSAFLUSH,&saved_term);raw_active=0;}printf("\033[?25h\033[0m\n");fflush(stdout);}
static int term_raw(void){struct termios t;if(tcgetattr(STDIN_FILENO,&saved_term))return -1;t=saved_term;t.c_lflag&=(tcflag_t)~(ECHO|ICANON);t.c_iflag&=(tcflag_t)~(IXON|IXOFF|ICRNL|INLCR|IGNCR|ISTRIP);t.c_oflag&=(tcflag_t)~OPOST;t.c_cc[VMIN]=1;t.c_cc[VTIME]=0;if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&t))return -1;raw_active=1;atexit(term_restore);printf("\033[?25l");return 0;}
static void dims(int *rows,int *cols){struct winsize w;if(!ioctl(STDOUT_FILENO,TIOCGWINSZ,&w)&&w.ws_row&&w.ws_col){*rows=w.ws_row;*cols=w.ws_col;}else{*rows=24;*cols=80;}}
static int key(void){unsigned char c;if(read(STDIN_FILENO,&c,1)!=1)return -1;if(c==27){unsigned char a,b;if(read(STDIN_FILENO,&a,1)!=1)return 27;if(a=='['&&read(STDIN_FILENO,&b,1)==1){if(b=='A')return 1001;if(b=='B')return 1002;if(b=='C')return 1003;if(b=='D')return 1004;}return 27;}if(c=='\r'||c=='\n')return 13;return c;}
static int exists(const char*p){struct stat st;return p&&*p&&!stat(p,&st)&&S_ISREG(st.st_mode);}
static void clip(const char*s,int width){int n=0;if(!s)s="";while(*s&&n<width){unsigned char c=(unsigned char)*s++;if(c=='\n'||c=='\r')break;putchar(c);n++;}while(n++<width)putchar(' ');}
static void field(int row,int col,int width,const char *label,const char *value){if(width<12)return;printf("\033[%d;%dH",row,col);printf("%-10.10s",label);clip(value,width-10);}
static void wrap(int row,int col,int width,int maxrows,const char*s){int r=0,n=0;if(!s)s="";while(*s&&r<maxrows){printf("\033[%d;%dH",row+r,col);n=0;while(*s&&n<width){if(*s=='\r'){s++;continue;}if(*s=='\n'){s++;break;}putchar((unsigned char)*s++);n++;}while(n++<width)putchar(' ');r++;}while(r<maxrows){printf("\033[%d;%dH",row+r,col);for(n=0;n<width;n++)putchar(' ');r++;}}
static void launch_file(const char *path,char *status,size_t cap){const char*launcher=getenv("ROMM_LAUNCHER");pid_t p;if(!launcher||!*launcher){snprintf(status,cap,"Downloaded. Set ROMM_LAUNCHER to start games.");return;}p=fork();if(p==0){execlp(launcher,launcher,path,(char*)NULL);_exit(127);}if(p<0)snprintf(status,cap,"Could not start launcher");else{snprintf(status,cap,"Started: %s %s",launcher,path);waitpid(p,NULL,WNOHANG);}}
/* ZMODEM uses the existing PTY. sz must be installed in the container. */
static int send_zmodem(const char *path,char *status,size_t cap){
    pid_t pid;int st=0;struct termios before,t;
    if(!path||!*path)return -1;
    fflush(stdout);
    if(tcgetattr(STDIN_FILENO,&before)!=0){snprintf(status,cap,"No terminal for ZMODEM");return -1;}
    t=before;
    t.c_iflag&=(tcflag_t)~(IXON|IXOFF|ICRNL|INLCR|IGNCR|ISTRIP|BRKINT|PARMRK);
    t.c_oflag&=(tcflag_t)~OPOST;
    t.c_lflag&=(tcflag_t)~(ECHO|ICANON|ISIG|IEXTEN);
    t.c_cflag|=CS8;
    t.c_cc[VMIN]=1;t.c_cc[VTIME]=0;
    if(tcsetattr(STDIN_FILENO,TCSANOW,&t)!=0){snprintf(status,cap,"Cannot set binary terminal");return -1;}
    pid=fork();
    if(pid==0){execlp("sz","sz","--binary","--",path,(char*)NULL);_exit(127);}
    if(pid<0){tcsetattr(STDIN_FILENO,TCSANOW,&before);snprintf(status,cap,"Cannot start sz");return -1;}
    while(waitpid(pid,&st,0)<0){if(errno!=EINTR){st=-1;break;}}
    tcsetattr(STDIN_FILENO,TCSAFLUSH,&before);
    if(st!=-1&&WIFEXITED(st)&&WEXITSTATUS(st)==0){snprintf(status,cap,"ZMODEM transfer complete");return 0;}
    if(st!=-1&&WIFEXITED(st)&&WEXITSTATUS(st)==127)snprintf(status,cap,"sz missing: install lrzsz in Docker image");
    else snprintf(status,cap,"ZMODEM failed (check DCTelnet receive)");
    return -1;
}
static const char *safe_name(const char *name){
    const char *p,*base=name;
    if(!name)return NULL;
    for(p=name;*p;p++)if(*p=='/'||*p=='\\')base=p+1;
    if(!*base||!strcmp(base,".")||!strcmp(base,".."))return NULL;
    return base;
}
static int load_detail(romm_client_t*c,const romm_game_list_t*g,size_t sel,romm_game_t*d,char*status,size_t cap){int rc;romm_game_free(d);rc=romm_game_info(c,g->items[sel].id,d);if(rc)snprintf(status,cap,"Info error: %s",romm_strerror(rc));return rc;}
static void draw(view_t view,const romm_platform_list_t*p,size_t ps,const romm_game_list_t*g,size_t gs,size_t offset,const romm_game_t*d,const char*status){int rows,cols,left,right,start=3,visible,i;dims(&rows,&cols);left=cols/2;if(left<28)left=28;if(left>42)left=42;right=cols-left-4;visible=rows-6;if(visible<5)visible=5;printf("\033[2J\033[H");printf("libromm-1.0  %s",view==VIEW_PLATFORMS?"PLATFORMS":"GAMES");printf("\033[2;%dH| DETAILS",left+1);for(i=0;i<visible;i++){size_t idx=(view==VIEW_PLATFORMS?ps:gs);size_t base=idx>=(size_t)visible?idx-(size_t)visible+1:0;size_t n=base+(size_t)i;printf("\033[%d;1H",start+i);if(view==VIEW_PLATFORMS){if(n<p->count){if(n==ps)printf("\033[7m");printf("%c %5ld ",n==ps?'>':' ',p->items[n].rom_count);clip(p->items[n].display_name,left-9);if(n==ps)printf("\033[0m");}else clip("",left);}else{if(n<g->count){if(n==gs)printf("\033[7m");printf("%c %6ld ",n==gs?'>':' ',g->items[n].id);clip(g->items[n].name,left-10);if(n==gs)printf("\033[0m");}else clip("",left);}printf("\033[%d;%dH|",start+i,left+1);}if(view==VIEW_PLATFORMS&&p->count){char b[64];const romm_platform_t*x=&p->items[ps];field(3,left+3,right,"Platform",x->display_name);snprintf(b,sizeof(b),"%ld",x->rom_count);field(4,left+3,right,"Games",b);field(5,left+3,right,"Category",x->category);}else if(view==VIEW_GAMES&&g->count){char b[128];field(3,left+3,right,"Name",d->name);field(4,left+3,right,"Platform",d->platform_display_name);field(5,left+3,right,"File",d->fs_name);snprintf(b,sizeof(b),"%llu bytes",d->fs_size_bytes);field(6,left+3,right,"Size",b);field(7,left+3,right,"Genres",d->genres);field(8,left+3,right,"Developer",d->developers);field(9,left+3,right,"Publisher",d->publishers);field(10,left+3,right,"Region",d->regions);if(d->average_rating>0.0)snprintf(b,sizeof(b),"%.1f",d->average_rating);else strcpy(b,"-");field(11,left+3,right,"Rating",b);if(right>2&&rows>18)wrap(13,left+3,right-1,rows-17,d->summary);}printf("\033[%d;1H",rows-2);for(i=0;i<cols-1;i++)putchar('-');printf("\033[%d;1H",rows-1);if(view==VIEW_PLATFORMS)printf("Up/Down select   Enter games   Q quit");else printf("Up/Down select   /: search  Left/Right A-Z  D RAW  Enter WHDLoad  Esc back   Pg: %lu",(unsigned long)offset);printf("\033[%d;1H",rows);clip(status,cols-1);printf("\033[1;1H");fflush(stdout);}
int main(int ac,char**av){romm_client_t c;romm_transport_t t=romm_curl_transport();romm_platform_list_t p={0};romm_game_list_t g={0};romm_game_t d={0};view_t view=VIEW_PLATFORMS;size_t ps=0,gs=0,offset=0;long pid=0;int rc,k;int filtered=0;char letter='A';char status[256]="Ready";if(ac!=3){fprintf(stderr,"Usage: %s BASE_URL TOKEN\n",av[0]);return 2;}rc=romm_client_init(&c,av[1],av[2],t);if(rc)return 1;rc=romm_platforms(&c,&p);if(rc){fprintf(stderr,"platforms: %s\n",romm_strerror(rc));romm_client_destroy(&c);return 1;}if(term_raw()){fprintf(stderr,"terminal raw mode failed\n");romm_platform_list_free(&p);romm_client_destroy(&c);return 1;}for(;;){draw(view,&p,ps,&g,gs,offset,&d,status);k=key();if(k=='q'||k=='Q')break;if(view==VIEW_PLATFORMS){if(k==1001&&ps>0)ps--;else if(k==1002&&ps+1<p.count)ps++;else if(k==13&&p.count){pid=p.items[ps].id;filtered=0;letter='A';offset=0;romm_game_list_free(&g);rc=romm_games(&c,pid,100,offset,&g);if(rc){snprintf(status,sizeof(status),"Games error: %s",romm_strerror(rc));continue;}if(!g.count){snprintf(status,sizeof(status),"No games on %s",p.items[ps].display_name);continue;}gs=0;view=VIEW_GAMES;snprintf(status,sizeof(status),"%ld games total",g.total);load_detail(&c,&g,gs,&d,status,sizeof(status));}}else{if(k==27){view=VIEW_PLATFORMS;romm_game_list_free(&g);romm_game_free(&d);snprintf(status,sizeof(status),"Back to platforms");}else if(k==1001){if(gs>0){gs--;load_detail(&c,&g,gs,&d,status,sizeof(status));}else if(!filtered&&offset>=100){offset-=100;romm_game_list_free(&g);if(!romm_games(&c,pid,100,offset,&g)&&g.count){gs=g.count-1;load_detail(&c,&g,gs,&d,status,sizeof(status));}}}else if(k==1002){if(gs+1<g.count){gs++;load_detail(&c,&g,gs,&d,status,sizeof(status));}else if(!filtered&&offset+g.count<(size_t)g.total){offset+=g.count;romm_game_list_free(&g);if(!romm_games(&c,pid,100,offset,&g)&&g.count){gs=0;load_detail(&c,&g,gs,&d,status,sizeof(status));}}}else if(k=='/' || k==1003 || k==1004){
    char search[128]="";romm_game_list_t ng={0};
    if(k=='/'){
            size_t n=0;
            int cancelled=0;
            printf("\033[2J\033[H");
            printf("libromm-1.0 - Search\n\n");
            printf("Search: ");
            fflush(stdout);
            while(n+1<sizeof(search)){
                int ch=key();
                if(ch==13||ch==10)break;
                if(ch==27){cancelled=1;break;}
                if(ch==127||ch==8){
                    if(n){n--;printf("\b \b");fflush(stdout);}
                    continue;
                }
                if(ch>=32&&ch<127){
                    search[n++]=(char)ch;
                    putchar(ch);
                    fflush(stdout);
                }
            }
            search[cancelled?0:n]=0;
    }else{
        letter=(char)((letter-'A'+(k==1004?1:25))%26+'A');
        search[0]=letter;search[1]=0;
    }
    if(search[0]){
        rc=(k==1003||k==1004)?romm_letter_games(&c,pid,letter,500,&ng):romm_search_games(&c,pid,search,500,&ng);
        if(rc)snprintf(status,sizeof(status),"Search failed: %s",romm_strerror(rc));
        else{romm_game_list_free(&g);romm_game_free(&d);g=ng;gs=offset=0;filtered=1;
             snprintf(status,sizeof(status),"%s: %s (%ld total, showing %lu)",(k==1003||k==1004)?"Prefix":"Search",search,g.total,(unsigned long)g.count);
             if(g.count)load_detail(&c,&g,gs,&d,status,sizeof(status));}
    }
}else if((k=='d'||k=='D')&&g.count){const char *name=safe_name(d.fs_name);const char *zmode_env=getenv("ROMM_ZMODEM");int zmode=zmode_env&&strcmp(zmode_env,"1")==0;
if(!name){snprintf(status,sizeof(status),"No safe downloadable filename");}
else if(zmode){
    char path[1024];
    if(snprintf(path,sizeof(path),"/downloads/%ld-%s",d.id,name)>=(int)sizeof(path)){snprintf(status,sizeof(status),"Filename too long");}
    else{
        if(!exists(path)){
            snprintf(status,sizeof(status),"Downloading %s ...",name);
            draw(view,&p,ps,&g,gs,offset,&d,status);
            rc=romm_download_rom(&c,d.id,path);
            if(rc){snprintf(status,sizeof(status),"Download failed: %s",romm_strerror(rc));continue;}
        }
        snprintf(status,sizeof(status),"Start ZMODEM receive in DCTelnet...");
        draw(view,&p,ps,&g,gs,offset,&d,status);
        send_zmodem(path,status,sizeof(status));
    }
}
else if(exists(name)){launch_file(name,status,sizeof(status));}
else{snprintf(status,sizeof(status),"Downloading %s ...",name);draw(view,&p,ps,&g,gs,offset,&d,status);rc=romm_download_rom(&c,d.id,name);if(rc)snprintf(status,sizeof(status),"Download failed: %s",romm_strerror(rc));else snprintf(status,sizeof(status),"Downloaded %s - Enter again to start",name);}}else if(k==13&&g.count){
    char url[96],path[256];const char *zmode_env=getenv("ROMM_ZMODEM");
    int zmode=zmode_env&&strcmp(zmode_env,"1")==0;
    snprintf(url,sizeof(url),"/whdload/%ld",d.id);
    snprintf(path,sizeof(path),"%sromm-%ld-whdload.zip",zmode?"/downloads/":"",d.id);
    snprintf(status,sizeof(status),"Preparing WHDLoad package...");
    draw(view,&p,ps,&g,gs,offset,&d,status);
    rc=romm_download_file(&c,url,path);
    if(rc)snprintf(status,sizeof(status),"WHDLoad package unavailable (%s). D = RAW",romm_strerror(rc));
    else if(zmode){snprintf(status,sizeof(status),"Receive WHDLoad ZIP via ZMODEM...");draw(view,&p,ps,&g,gs,offset,&d,status);send_zmodem(path,status,sizeof(status));}
    else snprintf(status,sizeof(status),"WHDLoad ZIP saved: %.150s (extract on Amiga)",path);
}}}
romm_game_free(&d);romm_game_list_free(&g);romm_platform_list_free(&p);romm_client_destroy(&c);term_restore();return 0;}
