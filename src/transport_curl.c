#include "libromm_curl.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct{char*d;size_t n;}mem_t;
static size_t wm(void*p,size_t s,size_t n,void*u){size_t z=s*n;mem_t*m=u;char*q=realloc(m->d,m->n+z+1);if(!q)return 0;m->d=q;memcpy(m->d+m->n,p,z);m->n+=z;m->d[m->n]=0;return z;}
static struct curl_slist*hdr(const char*a){struct curl_slist*h=NULL;char b[2048];if(a&&*a){snprintf(b,sizeof(b),"Authorization: %s",a);h=curl_slist_append(h,b);}return curl_slist_append(h,"Accept: application/json");}
static int get(void*u,const char*url,const char*a,romm_http_response_t*r){CURL*c;CURLcode x;struct curl_slist*h;mem_t m={0};(void)u;c=curl_easy_init();if(!c)return ROMM_ERR_TRANSPORT;h=hdr(a);curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,wm);curl_easy_setopt(c,CURLOPT_WRITEDATA,&m);x=curl_easy_perform(c);if(x==CURLE_OK)curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&r->status);curl_slist_free_all(h);curl_easy_cleanup(c);if(x!=CURLE_OK){free(m.d);return ROMM_ERR_TRANSPORT;}r->body=m.d;r->body_size=m.n;return 0;}
static size_t wf(void*p,size_t s,size_t n,void*u){return fwrite(p,s,n,(FILE*)u);}
static int dl(void*u,const char*url,const char*a,const char*d){CURL*c;CURLcode x;struct curl_slist*h;FILE*f;long st=0;(void)u;f=fopen(d,"wb");if(!f)return ROMM_ERR_IO;c=curl_easy_init();if(!c){fclose(f);return ROMM_ERR_TRANSPORT;}h=hdr(a);curl_easy_setopt(c,CURLOPT_URL,url);curl_easy_setopt(c,CURLOPT_HTTPHEADER,h);curl_easy_setopt(c,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(c,CURLOPT_WRITEFUNCTION,wf);curl_easy_setopt(c,CURLOPT_WRITEDATA,f);x=curl_easy_perform(c);if(x==CURLE_OK)curl_easy_getinfo(c,CURLINFO_RESPONSE_CODE,&st);curl_slist_free_all(h);curl_easy_cleanup(c);fclose(f);if(x!=CURLE_OK||st<200||st>=300){remove(d);return x!=CURLE_OK?ROMM_ERR_TRANSPORT:ROMM_ERR_HTTP;}return 0;}
static void fr(void*u,romm_http_response_t*r){(void)u;free(r->body);memset(r,0,sizeof(*r));}
romm_transport_t romm_curl_transport(void){romm_transport_t t={get,dl,fr,NULL};return t;}
