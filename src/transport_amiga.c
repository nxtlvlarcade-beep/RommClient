#include "libromm_amiga.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *SocketBase = NULL;

typedef struct { char host[256]; unsigned short port; char base[512]; } au_t;
static au_t au;

static int parse_http(const char *url, au_t *u) {
    const char *p,*slash,*colon; size_t n;
    if (!url || strncmp(url,"http://",7)) return -1;
    p=url+7; slash=strchr(p,'/'); if(!slash) slash=p+strlen(p);
    colon=NULL; { const char *q; for(q=p;q<slash;q++) if(*q==':') colon=q; }
    n=(size_t)((colon?colon:slash)-p); if(!n||n>=sizeof(u->host)) return -1;
    memcpy(u->host,p,n); u->host[n]=0; u->port=colon?(unsigned short)atoi(colon+1):80;
    if(!u->port) return -1;
    if(*slash) snprintf(u->base,sizeof(u->base),"%s",slash); else strcpy(u->base,"");
    return 0;
}
static int open_sock(void){ if(SocketBase)return 0; SocketBase=OpenLibrary("bsdsocket.library",4); return SocketBase?0:-1; }
static int connect_host(const au_t*u){ struct hostent*h; struct sockaddr_in a; int s;if(open_sock())return -1;h=gethostbyname((char*)u->host);if(!h)return -1;s=socket(AF_INET,SOCK_STREAM,0);if(s<0)return -1;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(u->port);memcpy(&a.sin_addr,h->h_addr,h->h_length);if(connect(s,(struct sockaddr*)&a,sizeof(a))<0){CloseSocket(s);return -1;}return s; }
static int send_all(int s,const char*b,size_t n){while(n){int k=send(s,(char*)b,(int)n,0);if(k<=0)return -1;b+=k;n-=(size_t)k;}return 0;}
static char *header_end(char*b,size_t n){size_t i;for(i=3;i<n;i++)if(b[i-3]=='\r'&&b[i-2]=='\n'&&b[i-1]=='\r'&&b[i]=='\n')return b+i+1;return NULL;}
static long status_code(const char*b){long x=0;if(sscanf(b,"HTTP/%*s %ld",&x)!=1)return 0;return x;}
static int request(const char*url,const char*auth,char**all,size_t*len,long*status){
    au_t u; int s,k; char req[1536],tmp[2048]; char*b=NULL; size_t used=0,cap=0; int qn;
    if(!all||!len||!status)return ROMM_ERR_ARGUMENT;
    *all=NULL; *len=0; *status=0;
    if(parse_http(url,&u))return ROMM_ERR_TRANSPORT;
    printf("[NET] host=%s port=%u path=%s\n",u.host,(unsigned)u.port,u.base[0]?u.base:"/"); fflush(stdout);
    s=connect_host(&u); if(s<0){printf("[NET] connect failed\n");fflush(stdout);return ROMM_ERR_TRANSPORT;}
    if(auth&&*auth)
        qn=snprintf(req,sizeof(req),"GET %s HTTP/1.0\r\nHost: %s\r\nAuthorization: %s\r\nAccept: application/json\r\nConnection: close\r\n\r\n",u.base[0]?u.base:"/",u.host,auth);
    else
        qn=snprintf(req,sizeof(req),"GET %s HTTP/1.0\r\nHost: %s\r\nAccept: application/json\r\nConnection: close\r\n\r\n",u.base[0]?u.base:"/",u.host);
    if(qn<0||(size_t)qn>=sizeof(req)){CloseSocket(s);return ROMM_ERR_ARGUMENT;}
    if(send_all(s,req,(size_t)qn)){CloseSocket(s);return ROMM_ERR_TRANSPORT;}
    printf("[NET] request sent\n"); fflush(stdout);
    while((k=recv(s,tmp,sizeof(tmp),0))>0){
        if(used+(size_t)k+1<used){free(b);CloseSocket(s);return ROMM_ERR_MEMORY;}
        if(used+(size_t)k+1>cap){
            size_t need=used+(size_t)k+1,nc=cap?cap:4096; char*nb;
            while(nc<need){if(nc>4194304UL/2){nc=need;break;}nc*=2;}
            if(nc>4194304UL){free(b);CloseSocket(s);return ROMM_ERR_MEMORY;}
            nb=(char*)realloc(b,nc); if(!nb){free(b);CloseSocket(s);return ROMM_ERR_MEMORY;} b=nb;cap=nc;
        }
        memcpy(b+used,tmp,(size_t)k); used+=(size_t)k;
    }
    CloseSocket(s);
    if(k<0){free(b);printf("[NET] recv failed\n");fflush(stdout);return ROMM_ERR_TRANSPORT;}
    if(!b)return ROMM_ERR_TRANSPORT;
    b[used]=0; *status=status_code(b); *all=b; *len=used;
    printf("[NET] received=%lu status=%ld\n",(unsigned long)used,*status); fflush(stdout);
    return ROMM_OK;
}
static int aget(void*ud,const char*url,const char*token,romm_http_response_t*out){char*b,*body;size_t n,hn;long st;int rc;(void)ud;memset(out,0,sizeof(*out));rc=request(url,token,&b,&n,&st);if(rc)return rc;body=header_end(b,n);if(!body){free(b);return ROMM_ERR_TRANSPORT;}hn=(size_t)(body-b);out->body_size=n-hn;out->body=(char*)malloc(out->body_size+1);if(!out->body){free(b);return ROMM_ERR_MEMORY;}memcpy(out->body,body,out->body_size);out->body[out->body_size]=0;out->status=st;free(b);return ROMM_OK;}
static int adownload(void*ud,const char*url,const char*token,const char*dest){char*b,*body;size_t n,hn;long st;FILE*f;int rc;(void)ud;rc=request(url,token,&b,&n,&st);if(rc)return rc;if(st<200||st>=300){free(b);return ROMM_ERR_HTTP;}body=header_end(b,n);if(!body){free(b);return ROMM_ERR_TRANSPORT;}hn=(size_t)(body-b);f=fopen(dest,"wb");if(!f){free(b);return ROMM_ERR_IO;}if(fwrite(body,1,n-hn,f)!=n-hn){fclose(f);remove(dest);free(b);return ROMM_ERR_IO;}fclose(f);free(b);return ROMM_OK;}
static void afree(void*ud,romm_http_response_t*r){(void)ud;if(r){free(r->body);memset(r,0,sizeof(*r));}}
romm_transport_t romm_amiga_transport(void){romm_transport_t t;t.get=aget;t.download=adownload;t.free_response=afree;t.userdata=&au;return t;}
void romm_amiga_transport_shutdown(void){if(SocketBase){CloseLibrary(SocketBase);SocketBase=NULL;}}
