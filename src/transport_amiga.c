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
    if(*slash) { if(strlen(slash)>=sizeof(u->base)) return -1; strcpy(u->base,slash); }
    else strcpy(u->base,"");
    return 0;
}
static int open_sock(void){ if(SocketBase)return 0; SocketBase=OpenLibrary("bsdsocket.library",4); return SocketBase?0:-1; }
static int connect_host(const au_t*u){ struct hostent*h; struct sockaddr_in a; int s;if(open_sock())return -1;h=gethostbyname((char*)u->host);if(!h)return -1;s=socket(AF_INET,SOCK_STREAM,0);if(s<0)return -1;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons(u->port);memcpy(&a.sin_addr,h->h_addr,h->h_length);if(connect(s,(struct sockaddr*)&a,sizeof(a))<0){CloseSocket(s);return -1;}return s; }
static int send_all(int s,const char*b,size_t n){while(n){int want=n>32767U?32767:(int)n;int k=send(s,(char*)b,want,0);if(k<=0)return -1;b+=k;n-=(size_t)k;}return 0;}
static char *header_end(char*b,size_t n){size_t i;for(i=3;i<n;i++)if(b[i-3]=='\r'&&b[i-2]=='\n'&&b[i-1]=='\r'&&b[i]=='\n')return b+i+1;return NULL;}
static long status_code(const char*b){long x=0;if(sscanf(b,"HTTP/%*s %ld",&x)!=1)return 0;return x;}
static int make_request(char *req,size_t cap,const au_t*u,const char*auth){int n;if(auth&&*auth)n=snprintf(req,cap,"GET %s HTTP/1.0\r\nHost: %s\r\nAuthorization: %s\r\nAccept: application/json\r\nConnection: close\r\n\r\n",u->base[0]?u->base:"/",u->host,auth);else n=snprintf(req,cap,"GET %s HTTP/1.0\r\nHost: %s\r\nAccept: application/json\r\nConnection: close\r\n\r\n",u->base[0]?u->base:"/",u->host);if(n<0||(size_t)n>=cap)return -1;return n;}
static int grow(char **buf,size_t *cap,size_t need){size_t nc;char*nb;if(need<=*cap)return 0;nc=*cap?*cap:8192;while(nc<need){if(nc>((size_t)-1)/2){nc=need;break;}nc*=2;}if(nc<need)return -1;nb=(char*)realloc(*buf,nc);if(!nb)return -1;*buf=nb;*cap=nc;return 0;}

/* Metadata GET: one buffer only. After reception the HTTP headers are removed
   in-place, avoiding the old second ~response-sized allocation/copy. */
static int request_body(const char*url,const char*auth,char**out,size_t*outlen,long*status){
    au_t u; int s,k,qn,rc=ROMM_OK; char req[1536]; char *tmp=NULL,*b=NULL,*body; size_t used=0,cap=0,hn,need,last_report=0;
    if(!out||!outlen||!status)return ROMM_ERR_ARGUMENT;*out=NULL;*outlen=0;*status=0;
    if(parse_http(url,&u))return ROMM_ERR_TRANSPORT;
    tmp=(char*)malloc(2048);if(!tmp)return ROMM_ERR_MEMORY;
    printf("[NET] host=%s port=%u path=%s\n",u.host,(unsigned)u.port,u.base[0]?u.base:"/");fflush(stdout);
    s=connect_host(&u);if(s<0){printf("[NET] connect failed\n");fflush(stdout);free(tmp);return ROMM_ERR_TRANSPORT;}
    qn=make_request(req,sizeof(req),&u,auth);if(qn<0){CloseSocket(s);free(tmp);return ROMM_ERR_ARGUMENT;}
    if(send_all(s,req,(size_t)qn)){CloseSocket(s);free(tmp);return ROMM_ERR_TRANSPORT;}printf("[NET] request sent\n");fflush(stdout);
    for(;;){
        k=recv(s,tmp,2048,0);
        if(k==0)break;
        if(k<0){rc=ROMM_ERR_TRANSPORT;printf("[NET] recv failed\n");fflush(stdout);break;}
        if((size_t)k>((size_t)-1)-used-1){rc=ROMM_ERR_MEMORY;break;}
        need=used+(size_t)k+1;if(grow(&b,&cap,need)){rc=ROMM_ERR_MEMORY;break;}
        memcpy(b+used,tmp,(size_t)k);used+=(size_t)k;
        if(used-last_report>=65536U){printf("[NET] received %lu KB\n",(unsigned long)(used/1024U));fflush(stdout);last_report=used;}
    }
    CloseSocket(s);free(tmp);
    if(rc){free(b);return rc;}if(!b)return ROMM_ERR_TRANSPORT;
    b[used]=0;*status=status_code(b);body=header_end(b,used);if(!body){free(b);return ROMM_ERR_TRANSPORT;}
    hn=(size_t)(body-b);if(hn>used){free(b);return ROMM_ERR_TRANSPORT;}*outlen=used-hn;
    memmove(b,body,*outlen);b[*outlen]=0;*out=b;
    printf("[NET] body=%lu bytes status=%ld\n",(unsigned long)*outlen,*status);fflush(stdout);return ROMM_OK;
}

/* Downloads are streamed straight to disk. Buffers live on the heap so the
   classic AmigaDOS process stack does not need to be enlarged manually. */
static int download_stream(const char*url,const char*auth,const char*dest){
    au_t u; int s=-1,k,qn,rc=ROMM_OK; char req[1536]; char *tmp=NULL,*head=NULL,*body; size_t hused=0,hn,bn;long st;FILE*f=NULL;unsigned long total=0;
    if(!dest||parse_http(url,&u))return ROMM_ERR_ARGUMENT;
    tmp=(char*)malloc(8192);head=(char*)malloc(8192);if(!tmp||!head){free(tmp);free(head);return ROMM_ERR_MEMORY;}
    printf("[NET] download host=%s path=%s\n",u.host,u.base[0]?u.base:"/");fflush(stdout);
    s=connect_host(&u);if(s<0){rc=ROMM_ERR_TRANSPORT;goto done;}
    qn=make_request(req,sizeof(req),&u,auth);if(qn<0){rc=ROMM_ERR_ARGUMENT;goto done;}
    if(send_all(s,req,(size_t)qn)){rc=ROMM_ERR_TRANSPORT;goto done;}
    body=NULL;
    while(!body){k=recv(s,tmp,8192,0);if(k<=0){rc=ROMM_ERR_TRANSPORT;goto done;}if((size_t)k>8192U-hused-1U){rc=ROMM_ERR_TRANSPORT;goto done;}memcpy(head+hused,tmp,(size_t)k);hused+=(size_t)k;head[hused]=0;body=header_end(head,hused);}
    st=status_code(head);if(st<200||st>=300){rc=ROMM_ERR_HTTP;goto done;}hn=(size_t)(body-head);bn=hused-hn;
    f=fopen(dest,"wb");if(!f){rc=ROMM_ERR_IO;goto done;}
    if(bn&&fwrite(body,1,bn,f)!=bn){rc=ROMM_ERR_IO;goto fail_file;}total+=(unsigned long)bn;
    while((k=recv(s,tmp,8192,0))>0){if(fwrite(tmp,1,(size_t)k,f)!=(size_t)k){rc=ROMM_ERR_IO;goto fail_file;}total+=(unsigned long)k;}
    if(k<0){rc=ROMM_ERR_TRANSPORT;goto fail_file;}
    if(fclose(f)!=0){f=NULL;remove(dest);rc=ROMM_ERR_IO;goto done;}f=NULL;
    printf("[NET] downloaded=%lu bytes\n",total);fflush(stdout);goto done;
fail_file:
    if(f){fclose(f);f=NULL;}remove(dest);
done:
    if(s>=0)CloseSocket(s);free(tmp);free(head);return rc;
}
static int aget(void*ud,const char*url,const char*auth,romm_http_response_t*out){int rc;(void)ud;if(!out)return ROMM_ERR_ARGUMENT;memset(out,0,sizeof(*out));rc=request_body(url,auth,&out->body,&out->body_size,&out->status);return rc;}
static int adownload(void*ud,const char*url,const char*auth,const char*dest){(void)ud;return download_stream(url,auth,dest);}
static void afree(void*ud,romm_http_response_t*r){(void)ud;if(r){free(r->body);memset(r,0,sizeof(*r));}}
romm_transport_t romm_amiga_transport(void){romm_transport_t t;t.get=aget;t.download=adownload;t.free_response=afree;t.userdata=&au;return t;}
void romm_amiga_transport_shutdown(void){if(SocketBase){CloseLibrary(SocketBase);SocketBase=NULL;}}
