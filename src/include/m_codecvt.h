#ifndef _M_CODECVT_H_
#define _M_CODECVT_H_

#include "fox.h"


enum {
    CODECVT_OK,
    CODECVT_OUTBUF,
    CODECVT_INVALID,
};

typedef struct {
    RefCharset *cs_from;
    RefCharset *cs_to;
    const char *trans;
    void *ic;
    const char *inbuf;
    size_t inbytesleft;
    char *outbuf;
    size_t outbytesleft;
} CodeCVT;


typedef struct {
    int (*CodeCVT_open)(CodeCVT *ic, RefCharset *from, RefCharset *to, const char *trans);
    void (*CodeCVT_close)(CodeCVT *ic);
    int (*CodeCVT_next)(CodeCVT *ic);
    int (*CodeCVT_conv)(CodeCVT *ic, StrBuf *dst, const char *src_p, int src_size, int from_uni, int raise_error);

    Value (*cstr_Value_conv)(const char *p, int size, RefCharset *cs);
} CodeCVTStatic;

#endif /* _M_CODECVT_H_ */
