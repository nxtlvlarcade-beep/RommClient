#include "libromm.h"
#include "minijson.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static char*dupstr(const char*s){size_t n;char*p;if(!s)return NULL;n=strlen(s)+1;p=malloc(n);if(p)memcpy(p,s,n);return p;}
static char*tokstr(const char*j,const mj_token_t*t){size_t n=t->end-t->start;char*p=malloc(n+1);if(!p)return NULL;memcpy(p,j+t->start,n);p[n]=0;return p;}
static long tlong(const char*j,const mj_token_t*t){char b[64];size_t n=t->end-t->start;if(n>63)n=63;memcpy(b,j+t->start,n);b[n]=0;return strtol(b,NULL,10);}
static unsigned long long tull(const char*j,const mj_token_t*t){char b[64];size_t n=t->end-t->start;if(n>63)n=63;memcpy(b,j+t->start,n);b[n]=0;return strtoull(b,NULL,10);}
static int tbool(const char*j,const mj_token_t*t){return t->end-t->start==4&&!strncmp(j+t->start,"true",4);}
static double tdouble(const char*j,const mj_token_t*t){char b[64];size_t n=(size_t)(t->end-t->start);if(n>63)n=63;memcpy(b,j+t->start,n);b[n]=0;return strtod(b,NULL);}
static char *tokstr_unescape(const char*j,const mj_token_t*t){size_t n=(size_t)(t->end-t->start),i,o=0;char*p=malloc(n+1);if(!p)return NULL;for(i=0;i<n;i++){char c=j[t->start+(int)i];if(c=='\\'&&i+1<n){char e=j[t->start+(int)++i];switch(e){case 'n':p[o++]='\n';break;case 'r':p[o++]='\r';break;case 't':p[o++]='\t';break;case 'b':p[o++]='\b';break;case 'f':p[o++]='\f';break;case '"':p[o++]='"';break;case '\\':p[o++]='\\';break;case '/':p[o++]='/';break;default:p[o++]='?';break;}}else p[o++]=c;}p[o]=0;return p;}
static char *join_string_array(const char*j,mj_token_t*t,int arr,int nt){int i;size_t need=1,count=0;char*out,*p;for(i=arr+1;i<nt&&t[i].start<t[arr].end;i=mj_skip(t,i,nt))if(t[i].parent==arr&&t[i].type==MJ_STRING){need+=(size_t)(t[i].end-t[i].start)+(count?2:0);count++;}out=malloc(need);if(!out)return NULL;p=out;*p=0;count=0;for(i=arr+1;i<nt&&t[i].start<t[arr].end;i=mj_skip(t,i,nt))if(t[i].parent==arr&&t[i].type==MJ_STRING){size_t n=(size_t)(t[i].end-t[i].start);if(count){*p++=',';*p++=' ';}memcpy(p,j+t[i].start,n);p+=n;count++;}*p=0;return out;}
static char*urljoin(const char*b,const char*p){size_t a=strlen(b),n=strlen(p);char*o=malloc(a+n+2);if(!o)return NULL;strcpy(o,b);if(a&&b[a-1]=='/'&&*p=='/')p++;else if((!a||b[a-1]!='/')&&*p!='/')strcat(o,"/");strcat(o,p);return o;}
static char*auth(const romm_client_t*c){char*o;size_t n;if(!c->token||!*c->token)return NULL;n=strlen(c->token)+8;o=malloc(n);if(o){strcpy(o,"Bearer ");strcat(o,c->token);}return o;}
int romm_client_init(romm_client_t*c,const char*b,const char*t,romm_transport_t x){if(!c||!b||!x.get)return ROMM_ERR_ARGUMENT;memset(c,0,sizeof(*c));c->base_url=dupstr(b);c->token=dupstr(t?t:"");c->transport=x;if(!c->base_url||!c->token){romm_client_destroy(c);return ROMM_ERR_MEMORY;}return 0;}
void romm_client_destroy(romm_client_t*c){if(c){free(c->base_url);free(c->token);memset(c,0,sizeof(*c));}}
int romm_get_json(romm_client_t*c,const char*p,char**out){char*u,*a;romm_http_response_t r={0};int rc;if(!c||!p||!out)return ROMM_ERR_ARGUMENT;*out=NULL;u=urljoin(c->base_url,p);a=auth(c);if(!u){free(a);return ROMM_ERR_MEMORY;}rc=c->transport.get(c->transport.userdata,u,a,&r);free(u);free(a);if(rc)return ROMM_ERR_TRANSPORT;if(r.status<200||r.status>=300){if(c->transport.free_response)c->transport.free_response(c->transport.userdata,&r);return ROMM_ERR_HTTP;}*out=dupstr(r.body?r.body:"");if(c->transport.free_response)c->transport.free_response(c->transport.userdata,&r);return*out?0:ROMM_ERR_MEMORY;}
static void pfree(romm_platform_t*p){free(p->slug);free(p->name);free(p->display_name);free(p->category);free(p->family_name);memset(p,0,sizeof(*p));}
static int pobj(const char*j,mj_token_t*t,int obj,int nt,romm_platform_t*p){int i=obj+1;memset(p,0,sizeof(*p));p->generation=-1;while(i<nt&&t[i].start<t[obj].end){int v;if(t[i].parent!=obj){i++;continue;}v=i+1;if(v>=nt)break;if(mj_eq(j,&t[i],"id"))p->id=tlong(j,&t[v]);else if(mj_eq(j,&t[i],"rom_count"))p->rom_count=tlong(j,&t[v]);else if(mj_eq(j,&t[i],"generation")&&strncmp(j+t[v].start,"null",4))p->generation=tlong(j,&t[v]);else if(mj_eq(j,&t[i],"fs_size_bytes"))p->fs_size_bytes=tull(j,&t[v]);else if(mj_eq(j,&t[i],"is_identified"))p->is_identified=tbool(j,&t[v]);else if(mj_eq(j,&t[i],"missing_from_fs"))p->missing_from_fs=tbool(j,&t[v]);else if(mj_eq(j,&t[i],"slug")&&t[v].type==MJ_STRING)p->slug=tokstr(j,&t[v]);else if(mj_eq(j,&t[i],"name")&&t[v].type==MJ_STRING)p->name=tokstr(j,&t[v]);else if(mj_eq(j,&t[i],"display_name")&&t[v].type==MJ_STRING)p->display_name=tokstr(j,&t[v]);else if(mj_eq(j,&t[i],"category")&&t[v].type==MJ_STRING)p->category=tokstr(j,&t[v]);else if(mj_eq(j,&t[i],"family_name")&&t[v].type==MJ_STRING)p->family_name=tokstr(j,&t[v]);i=mj_skip(t,v,nt);}if(!p->slug)p->slug=dupstr("");if(!p->name)p->name=dupstr("");if(!p->display_name)p->display_name=dupstr(p->name);if(!p->category)p->category=dupstr("");if(!p->family_name)p->family_name=dupstr("");if(!p->slug||!p->name||!p->display_name||!p->category||!p->family_name){pfree(p);return ROMM_ERR_MEMORY;}return 0;}
int romm_platforms(romm_client_t*c,romm_platform_list_t*out){char*j=NULL;mj_token_t*t=NULL;int nt,cap=4096,rc,i;size_t n=0;if(!c||!out)return ROMM_ERR_ARGUMENT;memset(out,0,sizeof(*out));rc=romm_get_json(c,"/api/platforms",&j);if(rc)return rc;for(;;){t=calloc(cap,sizeof(*t));if(!t){free(j);return ROMM_ERR_MEMORY;}nt=mj_parse(j,t,cap);if(nt!=-1)break;free(t);t=NULL;cap*=2;if(cap>262144){free(j);return ROMM_ERR_PARSE;}}if(nt<1||t[0].type!=MJ_ARRAY){free(t);free(j);return ROMM_ERR_PARSE;}for(i=1;i<nt;i=mj_skip(t,i,nt))if(t[i].parent==0&&t[i].type==MJ_OBJECT)n++;out->items=calloc(n,sizeof(*out->items));if(n&&!out->items){free(t);free(j);return ROMM_ERR_MEMORY;}for(i=1;i<nt;i=mj_skip(t,i,nt))if(t[i].parent==0&&t[i].type==MJ_OBJECT){rc=pobj(j,t,i,nt,&out->items[out->count]);if(rc){romm_platform_list_free(out);free(t);free(j);return rc;}out->count++;}free(t);free(j);return 0;}
void romm_platform_list_free(romm_platform_list_t*l){size_t i;if(!l)return;for(i=0;i<l->count;i++)pfree(&l->items[i]);free(l->items);memset(l,0,sizeof(*l));}

void romm_game_free(romm_game_t *g) {
    if(!g)return;
    free(g->name); free(g->fs_name); free(g->platform_display_name);
    free(g->summary); free(g->genres); free(g->developers); free(g->publishers);
    free(g->game_modes); free(g->regions); free(g->path_cover_small); free(g->path_cover_large);
    memset(g,0,sizeof(*g));
}

static int game_obj(const char*j,mj_token_t*t,int obj,int nt,romm_game_t*g) {
    int i=obj+1;
    memset(g,0,sizeof(*g));
    while(i<nt && t[i].start<t[obj].end) {
        int v;
        if(t[i].parent!=obj){i++;continue;}
        v=i+1; if(v>=nt)break;
        if(mj_eq(j,&t[i],"id")) g->id=tlong(j,&t[v]);
        else if(mj_eq(j,&t[i],"platform_id")) g->platform_id=tlong(j,&t[v]);
        else if(mj_eq(j,&t[i],"fs_size_bytes")) g->fs_size_bytes=tull(j,&t[v]);
        else if(mj_eq(j,&t[i],"name") && t[v].type==MJ_STRING) g->name=tokstr_unescape(j,&t[v]);
        else if(mj_eq(j,&t[i],"fs_name") && t[v].type==MJ_STRING) g->fs_name=tokstr_unescape(j,&t[v]);
        else if(mj_eq(j,&t[i],"platform_display_name") && t[v].type==MJ_STRING) g->platform_display_name=tokstr_unescape(j,&t[v]);
        else if(mj_eq(j,&t[i],"summary") && t[v].type==MJ_STRING) g->summary=tokstr_unescape(j,&t[v]);
        else if(mj_eq(j,&t[i],"regions") && t[v].type==MJ_ARRAY) g->regions=join_string_array(j,t,v,nt);
        else if(mj_eq(j,&t[i],"path_cover_small") && t[v].type==MJ_STRING) g->path_cover_small=tokstr_unescape(j,&t[v]);
        else if(mj_eq(j,&t[i],"path_cover_large") && t[v].type==MJ_STRING) g->path_cover_large=tokstr_unescape(j,&t[v]);
        else if(mj_eq(j,&t[i],"has_manual")) g->has_manual=tbool(j,&t[v]);
        else if(mj_eq(j,&t[i],"has_multiple_files")) g->has_multiple_files=tbool(j,&t[v]);
        else if(mj_eq(j,&t[i],"metadatum") && t[v].type==MJ_OBJECT) {
            int k=v+1;
            while(k<nt && t[k].start<t[v].end) {
                int mv;
                if(t[k].parent!=v){k++;continue;}
                mv=k+1;if(mv>=nt)break;
                if(mj_eq(j,&t[k],"genres")&&t[mv].type==MJ_ARRAY)g->genres=join_string_array(j,t,mv,nt);
                else if(mj_eq(j,&t[k],"developers")&&t[mv].type==MJ_ARRAY)g->developers=join_string_array(j,t,mv,nt);
                else if(mj_eq(j,&t[k],"publishers")&&t[mv].type==MJ_ARRAY)g->publishers=join_string_array(j,t,mv,nt);
                else if(mj_eq(j,&t[k],"game_modes")&&t[mv].type==MJ_ARRAY)g->game_modes=join_string_array(j,t,mv,nt);
                else if(mj_eq(j,&t[k],"first_release_date"))g->first_release_date=(long long)tull(j,&t[mv]);
                else if(mj_eq(j,&t[k],"average_rating"))g->average_rating=tdouble(j,&t[mv]);
                k=mj_skip(t,mv,nt);
            }
        }
        i=mj_skip(t,v,nt);
    }
    if(!g->name)g->name=dupstr("");
    if(!g->fs_name)g->fs_name=dupstr("");
    if(!g->platform_display_name)g->platform_display_name=dupstr("");
    if(!g->summary)g->summary=dupstr("");
    if(!g->genres)g->genres=dupstr("");
    if(!g->developers)g->developers=dupstr("");
    if(!g->publishers)g->publishers=dupstr("");
    if(!g->game_modes)g->game_modes=dupstr("");
    if(!g->regions)g->regions=dupstr("");
    if(!g->path_cover_small)g->path_cover_small=dupstr("");
    if(!g->path_cover_large)g->path_cover_large=dupstr("");
    if(!g->name||!g->fs_name||!g->platform_display_name||!g->summary||!g->genres||!g->developers||!g->publishers||!g->game_modes||!g->regions||!g->path_cover_small||!g->path_cover_large){romm_game_free(g);return ROMM_ERR_MEMORY;}
    return ROMM_OK;
}

static int parse_games_json(const char*j,romm_game_list_t*out) {
    mj_token_t*t=NULL; int nt,cap=8192,arr=-1,i,rc; size_t n=0;
    memset(out,0,sizeof(*out));
    for(;;) {
        t=calloc((size_t)cap,sizeof(*t));
        if(!t)return ROMM_ERR_MEMORY;
        nt=mj_parse(j,t,cap);
        if(nt!=-1)break;
        free(t); t=NULL; cap*=2;
        if(cap>1048576)return ROMM_ERR_PARSE;
    }
    if(nt<1){free(t);return ROMM_ERR_PARSE;}

    /* RomM /api/roms is paginated in current releases. Accept both
       a direct array and object responses containing an "items" array. */
    if(t[0].type==MJ_ARRAY) arr=0;
    else if(t[0].type==MJ_OBJECT) {
        for(i=1;i<nt;) {
            int v;
            if(t[i].parent!=0){i++;continue;}
            v=i+1; if(v>=nt)break;
            if(mj_eq(j,&t[i],"items") && t[v].type==MJ_ARRAY) arr=v;
            else if(mj_eq(j,&t[i],"roms") && t[v].type==MJ_ARRAY && arr<0) arr=v;
            else if(mj_eq(j,&t[i],"total")) out->total=tlong(j,&t[v]);
            i=mj_skip(t,v,nt);
        }
    }
    if(arr<0){free(t);return ROMM_ERR_PARSE;}

    for(i=arr+1;i<nt;) {
        if(t[i].parent==arr && t[i].type==MJ_OBJECT)n++;
        i=mj_skip(t,i,nt);
    }
    out->items=calloc(n,sizeof(*out->items));
    if(n&&!out->items){free(t);return ROMM_ERR_MEMORY;}
    for(i=arr+1;i<nt;) {
        if(t[i].parent==arr && t[i].type==MJ_OBJECT) {
            rc=game_obj(j,t,i,nt,&out->items[out->count]);
            if(rc){romm_game_list_free(out);free(t);return rc;}
            out->count++;
        }
        i=mj_skip(t,i,nt);
    }
    if(out->total==0)out->total=(long)out->count;
    free(t); return ROMM_OK;
}

int romm_games(romm_client_t*c,long platform_id,size_t limit,size_t offset,
               romm_game_list_t*out) {
    char path[256],*j=NULL; int rc;
    if(!c||!out||platform_id<=0)return ROMM_ERR_ARGUMENT;
    if(limit==0)limit=100;
    snprintf(path,sizeof(path),
             "/api/roms?platform_ids=%ld&limit=%lu&offset=%lu&with_char_index=false&with_filter_values=false&with_rom_id_index=false",
             platform_id,(unsigned long)limit,(unsigned long)offset);
    rc=romm_get_json(c,path,&j); if(rc)return rc;
    rc=parse_games_json(j,out); free(j); return rc;
}

static int contains_ci(const char *hay,const char *needle) {
    size_t i,n;
    if(!needle||!*needle)return 1;
    n=strlen(needle);
    for(;*hay;hay++) {
        for(i=0;i<n;i++) {
            unsigned char a=(unsigned char)hay[i],b=(unsigned char)needle[i];
            if(!a)return 0;
            if(a>='A'&&a<='Z')a=(unsigned char)(a-'A'+'a');
            if(b>='A'&&b<='Z')b=(unsigned char)(b-'A'+'a');
            if(a!=b)break;
        }
        if(i==n)return 1;
    }
    return 0;
}

int romm_search_games(romm_client_t*c,long platform_id,const char*text,
                      size_t limit,romm_game_list_t*out) {
    romm_game_list_t page; size_t offset=0,page_size=250,i; int rc;
    if(!c||!text||!out||platform_id<=0)return ROMM_ERR_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(limit==0)limit=50;
    /* Gateway index-search avoids rescanning every RomM page on each keypress.
       Ordinary RomM servers return 404; retain the original fallback. */
    {
        char path[512], *json=NULL;
        size_t k=0; const unsigned char *q=(const unsigned char *)text;
        int n=snprintf(path,sizeof(path),"/api/roms/index-search?platform_ids=%ld&limit=%lu&q=",platform_id,(unsigned long)limit);
        if(n>0 && (size_t)n<sizeof(path)) {
            k=(size_t)n;
            while(*q && k+4<sizeof(path)) {
                unsigned char ch=*q++;
                if((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='_') path[k++]=(char)ch;
                else {static const char hex[]="0123456789ABCDEF";path[k++]='%';path[k++]=hex[ch>>4];path[k++]=hex[ch&15];}
            }
            path[k]=0;
            if(!*q && romm_get_json(c,path,&json)==ROMM_OK) {
                rc=parse_games_json(json,out);free(json);return rc;
            }
            free(json);
        }
    }
    for(;;) {
        rc=romm_games(c,platform_id,page_size,offset,&page);
        if(rc){romm_game_list_free(out);return rc;}
        for(i=0;i<page.count && out->count<limit;i++) {
            if(contains_ci(page.items[i].name,text) || contains_ci(page.items[i].fs_name,text)) {
                romm_game_t *q=realloc(out->items,(out->count+1)*sizeof(*q));
                if(!q){romm_game_list_free(&page);romm_game_list_free(out);return ROMM_ERR_MEMORY;}
                out->items=q;
                out->items[out->count]=page.items[i];
                memset(&page.items[i],0,sizeof(page.items[i]));
                out->count++;
            }
        }
        {
            long total=page.total;
            size_t got=page.count;
            romm_game_list_free(&page);
            offset+=got;
            if(got==0 || (total>0 && offset>=(size_t)total) || out->count>=limit)break;
        }
    }
    out->total=(long)out->count;
    return ROMM_OK;
}

int romm_letter_games(romm_client_t*c,long pid,char letter,size_t limit,romm_game_list_t*out) {
    char path[180], term[2]; char *json=NULL;int rc;size_t i,j=0;
    if(!c||!out||pid<=0||letter<'A'||letter>'Z')return ROMM_ERR_ARGUMENT;
    if(!limit)limit=500;
    snprintf(path,sizeof(path),"/api/roms/index-search?platform_ids=%ld&limit=%lu&letter=%c",pid,(unsigned long)limit,letter);
    rc=romm_get_json(c,path,&json);
    if(rc==ROMM_OK){rc=parse_games_json(json,out);free(json);return rc;}
    free(json);
    term[0]=letter;term[1]=0;
    rc=romm_search_games(c,pid,term,limit,out);
    if(rc)return rc;
    for(i=0;i<out->count;i++) {
        const char *name=out->items[i].name;
        unsigned char ch=(unsigned char)(name&&*name?*name:0);
        if(ch>='a'&&ch<='z')ch=(unsigned char)(ch-'a'+'A');
        if(ch==letter) {if(j!=i){out->items[j]=out->items[i];memset(&out->items[i],0,sizeof(out->items[i]));}j++;}
        else romm_game_free(&out->items[i]);
    }
    out->count=j;out->total=(long)j;return ROMM_OK;
}

void romm_game_list_free(romm_game_list_t*l) {
    size_t i; if(!l)return;
    for(i=0;i<l->count;i++)romm_game_free(&l->items[i]);
    free(l->items); memset(l,0,sizeof(*l));
}


static int parse_game_json(const char *j, romm_game_t *out) {
    mj_token_t *t=NULL; int nt,cap=2048,rc;
    if(!j||!out)return ROMM_ERR_ARGUMENT;
    memset(out,0,sizeof(*out));
    for(;;) {
        t=calloc((size_t)cap,sizeof(*t));
        if(!t)return ROMM_ERR_MEMORY;
        nt=mj_parse(j,t,cap);
        if(nt!=-1)break;
        free(t); t=NULL; cap*=2;
        if(cap>262144)return ROMM_ERR_PARSE;
    }
    if(nt<1||t[0].type!=MJ_OBJECT){free(t);return ROMM_ERR_PARSE;}
    rc=game_obj(j,t,0,nt,out);
    free(t);
    return rc;
}

int romm_game_info(romm_client_t *c,long rom_id,romm_game_t *out) {
    char path[96],*j=NULL; int rc;
    if(!c||!out||rom_id<=0)return ROMM_ERR_ARGUMENT;
    snprintf(path,sizeof(path),"/api/roms/%ld",rom_id);
    rc=romm_get_json(c,path,&j);
    if(rc)return rc;
    rc=parse_game_json(j,out);
    free(j);
    return rc;
}

static int is_unreserved(unsigned char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
           c=='-'||c=='_'||c=='.'||c=='~';
}

static char *urlencode_segment(const char *s) {
    static const char hex[]="0123456789ABCDEF";
    size_t n=0,i; char *o,*p;
    if(!s)return NULL;
    for(i=0;s[i];i++)n+=is_unreserved((unsigned char)s[i])?1:3;
    o=malloc(n+1); if(!o)return NULL; p=o;
    for(i=0;s[i];i++) {
        unsigned char c=(unsigned char)s[i];
        if(is_unreserved(c))*p++=(char)c;
        else {*p++='%';*p++=hex[c>>4];*p++=hex[c&15];}
    }
    *p=0; return o;
}

int romm_download_rom(romm_client_t *c,long rom_id,const char *destination) {
    romm_game_t g; char *enc=NULL,*path=NULL; size_t n; int rc;
    if(!c||rom_id<=0||!destination||!*destination)return ROMM_ERR_ARGUMENT;
    rc=romm_game_info(c,rom_id,&g); if(rc)return rc;
    if(!g.fs_name||!*g.fs_name){romm_game_free(&g);return ROMM_ERR_PARSE;}
    enc=urlencode_segment(g.fs_name);
    if(!enc){romm_game_free(&g);return ROMM_ERR_MEMORY;}
    n=strlen(enc)+64; path=malloc(n);
    if(!path){free(enc);romm_game_free(&g);return ROMM_ERR_MEMORY;}
    snprintf(path,n,"/api/roms/%ld/content/%s",rom_id,enc);
    rc=romm_download_file(c,path,destination);
    free(path); free(enc); romm_game_free(&g);
    return rc;
}

int romm_download_file(romm_client_t*c,const char*p,const char*d){char*u,*a;int rc;if(!c||!p||!d||!c->transport.download)return ROMM_ERR_ARGUMENT;u=urljoin(c->base_url,p);a=auth(c);if(!u){free(a);return ROMM_ERR_MEMORY;}rc=c->transport.download(c->transport.userdata,u,a,d);free(u);free(a);return rc;}
const char*romm_strerror(int e){switch(e){case 0:return"OK";case -1:return"invalid argument";case -2:return"transport error";case -3:return"HTTP error";case -4:return"out of memory";case -5:return"JSON parse error";case -6:return"I/O error";default:return"unknown error";}}
