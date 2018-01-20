#include "fconv.h"
#include "util.h"
#include <string.h>
#include <stdio.h>




void FConv_init(FConv *fc, FCharset *cs, int to_utf8, const char *alter)
{
    memset(fc, 0, sizeof(FConv));
    fc->cs = cs;
    fc->to_utf8 = to_utf8;
    fc->alter = alter;
    fc->status = FCONV_OK;

    fc->src_tmp = fc->src_tmp_buf;
    fc->src_tmp_end = fc->src_tmp_buf;
    fc->src_tmp_last = 0;
    fc->dst_tmp = fc->dst_tmp_buf;
}
void FConv_set_src(FConv *fc, const char *src, int src_len, int terminate)
{
    if (terminate && src_len < FCONV_MAX_CHAR_LENGTH) {
        // TODO: assert
    }
    fc->src = src;
    fc->src_end = src + src_len;
    fc->src_terminate = terminate;
}
void FConv_set_dst(FConv *fc, char *dst, int dst_len)
{
    if (dst_len < FCONV_MAX_CHAR_LENGTH) {
        //TODO: assert;
    }
    fc->dst = dst;
    fc->dst_end = fc->dst + dst_len;
}

static int utf8_codepoint(const char *p, int *code)
{
    int c = *p;

    if ((c & 0x80) == 0) {
        *code = c & 0x7F;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        *code = ((c & 0x1F) << 6) | (p[1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        *code = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        return 3;
    } else {
        *code = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12)  | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        return 4;
    }
}

static void output_alter_string(char **pdst, const char *fmt, int code, FCharset *cs)
{
    char *dst = *pdst;
    while (*fmt != '\0') {
        switch (*fmt) {
        case 'd':
            sprintf(dst, "%d", code);
            dst += strlen(dst);
            break;
        case 'x':
            sprintf(dst, "%02X", code);
            dst += strlen(dst);
            break;
        case 'u':
            sprintf(dst, "%04X", code);
            dst += strlen(dst);
            break;
        default:
            *dst++ = *fmt;
            break;
        }
        fmt++;
    }
    if (cs != NULL) {
        FCharset_from_ascii_string(cs, *pdst, dst);
    }
    *pdst = dst;
}
void FConv_next(FConv *fc)
{
    for (;;) {
        const char *src_prev;

        // dst_tmpに残っている場合、すべて出力する
        if (fc->dst_tmp > fc->dst_tmp_buf) {
            int dst_last = fc->dst_end - fc->dst;
            int tmp_len = fc->dst_tmp - fc->dst_tmp_buf;
            if (dst_last < tmp_len) {
                if (dst_last > 0) {
                    memcpy(fc->dst, fc->dst_tmp_buf, dst_last);
                    fc->dst += dst_last;
                    memmove(fc->dst_tmp_buf, fc->dst_tmp_buf + dst_last, tmp_len - dst_last);
                    fc->dst_tmp -= dst_last;
                }
                fc->status = FCONV_OUTPUT_REQUIRED;
                return;
            } else {
                memcpy(fc->dst, fc->dst_tmp_buf, tmp_len);
                fc->dst += tmp_len;
                fc->dst_tmp = fc->dst_tmp_buf;
            }
        }

        if (fc->src_tmp_last > 0) {
            if ((fc->src_tmp_end - fc->src_tmp_buf) < FCONV_MAX_CHAR_LENGTH) {
                memcpy(fc->src_tmp_end, fc->src, FCONV_MAX_CHAR_LENGTH);
                fc->src_tmp_end += FCONV_MAX_CHAR_LENGTH;
            }
        } else if (fc->src_terminate) {
            if (fc->src >= fc->src_end) {
                fc->status = FCONV_OK;
                return;
            }
        } else {
            // srcが残り少なくなったらsrc_tmpに移す
            size_t src_last = fc->src_end - fc->src;
            if (src_last < FCONV_MAX_CHAR_LENGTH) {
                memcpy(fc->src_tmp_buf, fc->src, src_last);
                fc->src_tmp = fc->src_tmp_buf;
                fc->src_tmp_end = fc->src_tmp + src_last;
                fc->src_tmp_last = src_last;
                fc->src += src_last;
                fc->status = FCONV_INPUT_REQUIRED;
                return;
            }
        }
        {
            const char **psrc;
            const char *src_end;
            char **pdst;
            char *dst_dummy;

            if (fc->src_tmp_last > 0) {
                psrc = &fc->src_tmp;
                src_end = fc->src_tmp_end;
            } else {
                psrc = &fc->src;
                src_end = fc->src_end;
            }
            src_prev = *psrc;
            if (fc->dst == NULL) {
                dst_dummy = fc->dst_tmp_buf;
                pdst = &dst_dummy;
            } else if ((fc->dst_end - fc->dst) < FCONV_MAX_CHAR_LENGTH) {
                fc->dst_tmp = fc->dst_tmp_buf;
                pdst = &fc->dst_tmp;
            } else {
                pdst = &fc->dst;
            }
            if (fc->to_utf8) {
                if (!FCharset_to_utf8(fc->cs, psrc, src_end, pdst)) {
                    if (fc->alter != NULL) {
                        output_alter_string(pdst, fc->alter, **psrc, NULL);
                        (*psrc)++;
                    } else {
                        fc->status = FCONV_ERROR;
                        fc->error_charcode = **psrc & 0xFF;
                        return;
                    }
                }
            } else {
                if (!FCharset_from_utf8(fc->cs, psrc, pdst)) {
                    int code;
                    int n = utf8_codepoint(fc->src, &code);
                    if (fc->alter != NULL) {
                        output_alter_string(pdst, fc->alter, code, fc->cs);
                        (*psrc) += n;
                    } else {
                        fc->status = FCONV_ERROR;
                        fc->error_charcode = code;
                        return;
                    }
                }
            }
        }
        if (fc->src_tmp_last > 0) {
            int forward = fc->src_tmp - src_prev;
            fc->src_tmp_last -= forward;
            if (fc->src_tmp_last <= 0) {
                fc->src += -fc->src_tmp_last;
                fc->src_tmp_last = 0;
            }
        }
    }
}
