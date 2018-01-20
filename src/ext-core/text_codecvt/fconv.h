#ifndef FCONV_H_INCLUDED
#define FCONV_H_INCLUDED


enum {
	FCHARSET_ASCII,

    FCHARSET_UTF8,
	FCHARSET_UTF8_LOOSE,
	FCHARSET_CESU8,

	FCHARSET_UTF16LE,
	FCHARSET_UTF16BE,
	FCHARSET_UTF32LE,
	FCHARSET_UTF32BE,

    FCHARSET_SINGLE_BYTE,
    FCHARSET_EUC,
    FCHARSET_ISO_2022,

    FCHARSET_SHIFTJIS,
    FCHARSET_BIG5,
    FCHARSET_GB18030,
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


FCharset *FCharset_new(int type, int num, ...);
void FCharset_close(FCharset *cv);
int FCharset_from_utf8(FCharset *cs, const char **psrc, char **pdst);
int FCharset_to_utf8(FCharset *cs, const char **psrc, const char *src_end, char **pdst);
void FCharset_from_ascii_string(FCharset *cs, char *src, const char *end);

void FConv_init(FConv *fc, FCharset *cs, int to_utf8, const char *alter);
void FConv_next(FConv *fc);
void FConv_set_src(FConv *fc, const char *src, int src_len, int terminate);
void FConv_set_dst(FConv *fc, char *dst, int dst_len);


#endif // FCONV_H_INCLUDED
