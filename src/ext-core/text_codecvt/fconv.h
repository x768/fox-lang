#ifndef FCONV_H_INCLUDED
#define FCONV_H_INCLUDED

#include "m_codecvt.h"


extern const FoxStatic *fs;
extern FoxGlobal *fg;

FCharset *FCharset_new(RefCharset *rc);
int FCharset_from_utf8(FCharset *cs, const char **psrc, char **pdst);
int FCharset_to_utf8(FCharset *cs, const char **psrc, const char *src_end, char **pdst);
void FCharset_from_ascii_string(FCharset *cs, char *src, const char *end);
void FConv_throw_charset_error(FConv *fc);

void FConv_init(FConv *fc, FCharset *cs, int to_utf8, const char *alter);
void FConv_conv_strbuf(FConv *fc, StrBuf *sb, const char *src, int src_len, int throw_error);
int FConv_next(FConv *fc, int throw_error);
void FConv_set_src(FConv *fc, const char *src, int src_len, int terminate);
void FConv_set_dst(FConv *fc, char *dst, int dst_len);


#endif // FCONV_H_INCLUDED
