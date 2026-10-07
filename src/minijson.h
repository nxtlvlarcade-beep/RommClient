#ifndef MINIJSON_H
#define MINIJSON_H
typedef enum { MJ_UNDEFINED=0,MJ_OBJECT,MJ_ARRAY,MJ_STRING,MJ_PRIMITIVE } mj_type_t;
typedef struct { mj_type_t type; int start,end,size,parent; } mj_token_t;
int mj_parse(const char*,mj_token_t*,int);
int mj_eq(const char*,const mj_token_t*,const char*);
int mj_skip(const mj_token_t*,int,int);
#endif
