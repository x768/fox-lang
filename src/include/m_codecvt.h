#ifndef M_CODECVT_H_INCLUDED
#define M_CODECVT_H_INCLUDED

#include "fox.h"


enum {
    CODECVT_OK,
    CODECVT_OUTBUF,
    CODECVT_INVALID,
};

enum {
    FCHARSET_NONE,

	FCHARSET_ASCII,

    FCHARSET_UTF8,
	FCHARSET_UTF8_LOOSE,
	FCHARSET_CESU8,

    FCHARSET_SINGLE_BYTE,
    FCHARSET_EUC,
    FCHARSET_SHIFTJIS,
    FCHARSET_BIG5,
    FCHARSET_GB18030,

    FCHARSET_ASCII_COMPAT,

    FCHARSET_ISO_2022,

	FCHARSET_UTF16LE,
	FCHARSET_UTF16BE,
	FCHARSET_UTF32LE,
	FCHARSET_UTF32BE,
};
enum {
    FCONV_LINEEND_ASIS,
    FCONV_LINEEND_LF,
    FCONV_LINEEND_CRLF,
    FCONV_LINEEND_CR,
};
enum {
    FCONV_MAX_CHAR_LENGTH = 16,
};
enum {
    FCONV_OK,
    FCONV_ERROR,
    FCONV_INPUT_REQUIRED,
    FCONV_OUTPUT_REQUIRED,
};

typedef struct FCharset FCharset;

typedef struct {
    FCharset *cs;
    int to_utf8;
    const char *alter;
    int status;
    int error_charcode;

    const char *src;
    const char *src_end;
    int src_terminate;
    char *dst;
    const char *dst_end;

    // private
    void *inner_state;

    char src_tmp_buf[FCONV_MAX_CHAR_LENGTH * 2];
    char *src_tmp;
    char *src_tmp_end;
    int src_tmp_last;

    char dst_tmp_buf[FCONV_MAX_CHAR_LENGTH];
    char *dst_tmp;
} FConv;

typedef struct {
    FCharset *(*RefCharset_get_fcharset)(RefCharset *rc, int throw_error);

    void (*FConv_init)(FConv *fc, FCharset *cs, int to_utf8, const char *alter);
    int (*FConv_conv_strbuf)(FConv *fc, StrBuf *sb, const char *src, int src_len, int throw_error);
    int (*FConv_next)(FConv *fc, int throw_error);
    void (*FConv_set_src)(FConv *fc, const char *src, int src_len, int terminate);
    void (*FConv_set_dst)(FConv *fc, char *dst, int dst_len);

    Value (*cstr_Value_conv)(const char *p, int size, RefCharset *cs);
} CodeCVTStatic;

#endif /* M_CODECVT_H_INCLUDED */
