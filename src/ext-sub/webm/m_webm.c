#define DEFINE_GLOBALS
#include "m_image.h"
#include "m_media.h"
#include <stdio.h>
#include <stdlib.h>

#include <vorbis/codec.h>
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>


typedef struct {
    int valid;

    Value dst;
    vpx_codec_ctx_t codec;
    vpx_codec_enc_cfg_t cfg;
} VPXCodec;


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_webm;
static RefNode *mod_image;
static RefNode *cls_webm_reader;
static RefNode *cls_webm_writer;


#if 0
static int get_map_bool(int *has_value, int *value, RefMap *v_map, const char *key)
{
    HashValueEntry *ret = fs->refmap_get_strkey(v_map, key, -1);

    if (ret != NULL) {
        RefNode *type = fs->Value_type(ret->val);
        if (type != fs->cls_bool) {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Bool required but %n", type);
            return FALSE;
        }
        *value = Value_bool(ret->val);
        *has_value = TRUE;
    } else {
        *has_value = FALSE;
    }
    return TRUE;
}
static int get_map_double(int *has_value, double *value, RefMap *v_map, const char *key, double lower, double upper)
{
    HashValueEntry *ret = fs->refmap_get_strkey(v_map, key, -1);

    if (ret != NULL) {
        RefNode *type = fs->Value_type(ret->val);
        double val;
        if (type != fs->cls_float) {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Float required but %n", type);
            return FALSE;
        }
        val = fs->Value_float(ret->val);
        if (val < lower) {
            val = lower;
        } else if (val > upper) {
            val = upper;
        }
        *value = val;
        *has_value = TRUE;
    } else {
        *has_value = FALSE;
    }
    return TRUE;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////


static int webmwriter_new(Value *vret, Value *v, RefNode *node)
{
    VPXCodec *vpx = fs->new_buf(cls_webm_writer, sizeof(VPXCodec));
    vpx_codec_err_t res = vpx_codec_enc_config_default(vpx_codec_vp8_cx(), &vpx->cfg, 0);

    *vret = vp_Value(vpx);
    if (res != 0) {
        goto ERROR_END;
    }
    
    return TRUE;

ERROR_END:
    fs->throw_errorf(mod_webm, "WebMError", "%s", vpx_codec_err_to_string(res));
    return FALSE;
}

static int webmwriter_close(Value *vret, Value *v, RefNode *node)
{
    VPXCodec *vpx = Value_vp(*v);

    if (vpx->valid) {
        fs->Value_dec(vpx->dst);
        vpx->dst = VALUE_NULL;
        vpx->valid = FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
}

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_webm_reader = fs->define_identifier(m, m, "WebMReader", NODE_CLASS, 0);
    cls_webm_writer = fs->define_identifier(m, m, "WebMWriter", NODE_CLASS, 0);


    cls = cls_webm_reader;
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_webm_writer;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, webmwriter_new, 1, 1, NULL, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, webmwriter_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, webmwriter_close, 0, 0, NULL);

    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "WebMError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_webm = m;

    mod_image = fs->get_module_by_name("image", -1, TRUE, FALSE);
    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    if (a_fs->revision != FOX_INTERFACE_REVISION) {
        return NULL;
    }
    if (buf == NULL) {
        buf = malloc(256);
        sprintf(buf, "Build at\t" __DATE__ "\nlibvorbis\t%s\nlibvpx\t%s\n",
                vorbis_version_string(), vpx_codec_version_str());
    }
    return buf;
}
