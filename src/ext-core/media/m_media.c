#define DEFINE_GLOBALS
#include "media.h"
#include "fox_io.h"
#include "compat.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

enum {
    SOUND_INIT_SIZE = 32768,
};

static RefNode *cls_fileio;
static RefNode *cls_timedelta;


static void throw_already_closed(void)
{
    fs->throw_errorf(mod_media, "SoundError", "Sound is already closed");
}
static int detect_file_type(RefStr **ret, Value v)
{
    char buf[MAGIC_MAX];
    int size = MAGIC_MAX;

    if (!fs->stream_read_data(v, NULL, buf, &size, TRUE, FALSE)) {
        return FALSE;
    }
    *ret = fs->mimetype_from_magic(buf, size);
    return TRUE;
}
// e : NULLの場合はファイル読み込みに失敗した場合も終了しない
static Hash *load_type_function(Mem *mem)
{
    static Hash mods;

    if (mods.entry == NULL) {
        fs->Hash_init(&mods, mem, 64);
        fs->load_aliases_file(&mods, "data" SEP_S "media-type.txt");
    }
    return &mods;
}

/**
 * 8bit  ->      0 -   255 or -1.0 - 1.0
 * 16bit -> -32768 - 32767 or -1.0 - 1.0
 */
static int value_to_audio_range(Value v, int width)
{
    int upper, lower;
    RefNode *type = fs->Value_type(v);

    if (width == 2) {
        upper = 32767;
        lower = -32768;
    } else {
        upper = 255;
        lower = 0;
    }
    if (type == fs->cls_int) {
        int err = FALSE;
        int64_t val = fs->Value_int64(v, &err);
        if (err || val < lower) {
            return lower;
        } else if (val > upper) {
            return upper;
        } else {
            return (int)val;
        }
    } else {
        double d = (fs->Value_float(v) * (upper - lower) + (upper + lower)) / 2.0;

        if (d < (double)lower) {
            return lower;
        } else if (d > (double)upper) {
            return upper;
        } else {
            return (int)d;
        }
    }
}

////////////////////////////////////////////////////////////////////////////

int Audio_set_size(RefAudio *snd, int size)
{
    int length = size * snd->width * snd->channels;
    if (length > fs->max_alloc) {
        fs->throw_error_select(THROW_MAX_ALLOC_OVER__INT, fs->max_alloc);
        return FALSE;
    }
    if (snd->alloc_size < size) {
        if (snd->alloc_size == 0) {
            snd->alloc_size = SOUND_INIT_SIZE;
        }
        while (snd->alloc_size < size) {
            snd->alloc_size *= 2;
        }
        length = snd->alloc_size * snd->width * snd->channels;
        snd->u.u8 = realloc(snd->u.u8, length);
    }
    snd->length = size;
    return TRUE;
}

static int audio_new(Value *vret, Value *v, RefNode *node)
{
    int channels = 1;
    int data_size;
    int samples = fs->Value_int64(v[1], NULL);
    int bits = fs->Value_int64(v[2], NULL);

    RefAudio *snd = fs->buf_new(cls_audio, sizeof(RefAudio));
    *vret = vp_Value(snd);

    if (samples < 1 && samples > 1000000) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Must be >=1 and <=1000000 (argument #1)");
        return FALSE;
    }
    if (bits != 8 && bits != 16) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Must be 8 or 16 (argument #2)");
        return FALSE;
    }
    if (fg->stk_top > v + 3) {
        channels = fs->Value_int64(v[3], NULL);
        if (channels != 1 && channels != 2) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Must be 1 or 2 (argument #3)");
            return FALSE;
        }
    }

    snd->samples = samples;
    snd->width = bits / 8;
    snd->channels = channels;
    snd->alloc_size = SOUND_INIT_SIZE;
    snd->length = 0;
    data_size = SOUND_INIT_SIZE * snd->width * snd->channels;
    snd->u.u8 = malloc(data_size);
    memset(snd->u.u8, 0, data_size);

    return TRUE;
}
// 無圧縮wavのロード
static int audio_load(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd;
    Value reader;

    if (!fs->value_to_streamio(&reader, v[1], FALSE, 0)) {
        return FALSE;
    }
    snd = fs->buf_new(cls_audio, sizeof(RefAudio));
    *vret = vp_Value(snd);
    if (!audio_load_wav(snd, reader, FUNC_INT(node))) {
        fs->Value_dec(reader);
        return FALSE;
    }
    fs->Value_dec(reader);

    return TRUE;
}
static int audio_close(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    free(snd->u.u8);
    snd->u.u8 = NULL;
    return TRUE;
}
static int audio_save(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    Value writer;

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }

    if (!fs->value_to_streamio(&writer, v[1], TRUE, 0)) {
        return FALSE;
    }

    if (!audio_save_wav(snd, writer)) {
        fs->Value_dec(writer);
        return FALSE;
    }
    fs->Value_dec(writer);

    return TRUE;
}
static int audio_marshal_read(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = fs->buf_new(cls_audio, sizeof(RefAudio));
    Value r = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];
    uint8_t i8;

    if (!fs->stream_read_uint8(r, &i8)) {
        return FALSE;
    }
    snd->samples = i8;

    if (!fs->stream_read_uint8(r, &i8)) {
        return FALSE;
    }
    snd->width = i8;

    if (!fs->stream_read_uint8(r, &i8)) {
        return FALSE;
    }
    snd->channels = i8;

    if (!fs->stream_read_uint8(r, &i8)) {
        return FALSE;
    }
    snd->length = i8;

    return TRUE;
}
static int audio_marshal_write(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    Value w = Value_ref(v[1])->v[INDEX_MARSHALDUMPER_SRC];

    if (!fs->stream_write_uint8(w, snd->samples)) {
        return FALSE;
    }
    if (!fs->stream_write_uint8(w, snd->width)) {
        return FALSE;
    }
    if (!fs->stream_write_uint8(w, snd->channels)) {
        return FALSE;
    }
    if (!fs->stream_write_uint8(w, snd->length)) {
        return FALSE;
    }

    return TRUE;
}

static int audio_tostr(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    char cbuf[96];

    sprintf(cbuf, "Audio(%d, %dHz, %dbits, %s%s)",
        snd->length, snd->samples, snd->width * 8,
        (snd->channels == 1 ? "monoral" : "stereo"),
        (snd->u.u8 != NULL ? "" : ", closed"));

    *vret = fs->cstr_Value(fs->cls_str, cbuf, -1);

    return TRUE;
}
static int audio_hash(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    uint32_t ret = 0;
    int len;
    int i;

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    ret = snd->length;
    ret = ret * 31 + snd->width + snd->channels * 2;
    ret = ret * 31 + snd->samples;
    len = snd->length * snd->width * snd->channels;

    for (i = 0; i < len; i += 256) {
        ret = ret * 31 + snd->u.u8[i];
    }
    *vret = int32_Value(ret & INT32_MAX);

    return TRUE;
}
static int audio_eq(Value *vret, Value *v, RefNode *node)
{
    RefAudio *sn1 = Value_vp(*v);
    RefAudio *sn2 = Value_vp(*v);
    int len;

    if (sn1->u.u8 == NULL || sn2->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    *vret = VALUE_FALSE;

    if (sn1->length != sn2->length || sn1->width != sn2->width || sn1->channels != sn2->channels || sn1->samples != sn2->samples) {
        return TRUE;
    }

    len = sn1->length * sn1->width * sn1->channels;
    if (memcmp(sn1->u.u8, sn2->u.u8, len) != 0) {
        return TRUE;
    }

    *vret = VALUE_TRUE;
    return TRUE;
}
static int audio_size(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    *vret = int32_Value(snd->length);
    return TRUE;
}
static int audio_length(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    RefInt64 *delta = fs->buf_new(cls_timedelta, sizeof(RefInt64));
    *vret = vp_Value(delta);
    delta->u.i = (int64_t)snd->length * 1000 / snd->samples;
    return TRUE;
}
static int audio_samples(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    *vret = int32_Value(snd->samples);
    return TRUE;
}
static int audio_bits(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    *vret = int32_Value(snd->width * 8);
    return TRUE;
}
static int audio_channels(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    *vret = int32_Value(snd->channels);
    return TRUE;
}
static int audio_index(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    int64_t pos = fs->Value_int64(v[1], NULL);
    int64_t ch = fs->Value_int64(v[2], NULL);
    int f = FUNC_INT(node);

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (pos < 0) {
        pos += snd->length;
    }
    if (pos < 0 || pos >= snd->length) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid position");
        return FALSE;
    }
    if (ch < 0 || ch >= snd->channels) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid channel number (not more than %d)", snd->channels);
        return FALSE;
    }

    if (snd->channels == 2) {
        pos = pos * 2 + ch;
    }
    if (snd->width == 2) {
        // -32768 - 32767
        int16_t val = snd->u.i16[pos];
        if (f) {
            *vret = fs->float_Value(fs->cls_float, ((double)val) / 32768.0);
        } else {
            *vret = int32_Value(val);
        }
    } else {
        // 0 - 255
        uint8_t val = snd->u.u8[pos];
        if (f) {
            *vret = fs->float_Value(fs->cls_float, (((double)val) - 128.0) / 128.0);
        } else {
            *vret = int32_Value(val);
        }
    }

    return TRUE;
}
static int audio_index_set(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    int64_t pos = fs->Value_int64(v[1], NULL);
    int64_t ch = fs->Value_int64(v[2], NULL);
    int val;

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (pos < 0) {
        pos += snd->length;
    }
    if (pos < 0 || pos >= snd->length) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid position");
        return FALSE;
    }
    if (ch < 0 || ch >= snd->channels) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid channel number (not more than %d)", snd->channels);
        return FALSE;
    }

    if (snd->channels == 2) {
        pos = pos * 2 + ch;
    }
    val = value_to_audio_range(v[3], snd->width);
    if (snd->width == 2) {
        snd->u.i16[pos] = val;
    } else {
        snd->u.u8[pos] = val;
    }
    return TRUE;
}

static int audio_sub(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    int64_t begin, end;

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (fg->stk_top > v + 1) {
        RefNode *type = fs->Value_type(v[1]);

        begin = fs->Value_int64(v[1], NULL);
        if (type == fs->cls_int) {
        } else if (type == cls_timedelta) {
            begin = begin * snd->samples / 1000;
        } else if (type == fs->cls_str) {
            RefStr *rs = Value_vp(v[1]);
            if (!fs->timedelta_parse_string(&begin, rs->c, rs->size)) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid time format");
                return FALSE;
            }
            begin = begin * snd->samples / 1000;
        } else {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Int, Str or TimeDelta expected but %n (argument #1)", type);
            return FALSE;
        }
        if (begin < 0) {
            begin += snd->length;
        }
        if (begin < 0) {
            begin = 0;
        } else if (begin > snd->length) {
            begin = snd->length;
        }
    } else {
        begin = 0;
    }
    if (fg->stk_top > v + 2) {
        RefNode *type = fs->Value_type(v[2]);

        end = fs->Value_int64(v[2], NULL);
        if (type == fs->cls_int) {
        } else if (type == cls_timedelta) {
            end = end * snd->samples / 1000;
        } else if (type == fs->cls_str) {
            RefStr *rs = Value_vp(v[2]);
            if (!fs->timedelta_parse_string(&end, rs->c, rs->size)) {
                fs->throw_errorf(fs->mod_lang, "ValueError", "Invalid time format");
                return FALSE;
            }
            end = end * snd->samples / 1000;
        } else {
            fs->throw_errorf(fs->mod_lang, "TypeError", "Int, Str or TimeDelta expected but %n (argument #2)", type);
            return FALSE;
        }
        if (end < 0) {
            end += snd->length;
        }
        if (end < begin) {
            end = begin;
        } else if (end > snd->length) {
            end = snd->length;
        }
    } else {
        end = snd->length;
    }
    {
        int size2 = end - begin;
        int n = snd->width * snd->channels;
        RefAudio *snd2 = fs->buf_new(cls_audio, sizeof(RefAudio));
        *vret = vp_Value(snd2);
        *snd2 = *snd;
        snd2->alloc_size = 0;
        snd2->u.u8 = NULL;
        Audio_set_size(snd2, size2);
        memcpy(snd2->u.u8, snd->u.u8 + (begin * n), size2 * n);
    }

    return TRUE;
}
static int audio_push(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    Value v1 = v[1];
    RefNode *v1_type = fs->Value_type(v1);
    int pos = snd->length;

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }

    snd->length++;
    if (snd->alloc_size < snd->length) {
        snd->alloc_size *= 2;
        snd->u.u8 = realloc(snd->u.u8, snd->alloc_size);
    }

    if (snd->channels == 2) {
        int val1, val2;
        Value v2 = v[2];
        RefNode *v2_type = fs->Value_type(v2);

        if (v1_type != v2_type) {
            fs->throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, v1_type, v2_type, 2);
            return FALSE;
        }
        val1 = value_to_audio_range(v1, snd->width);
        val2 = value_to_audio_range(v2, snd->width);
        if (snd->width == 2) {
            snd->u.i16[pos * 2 + 0] = val1;
            snd->u.i16[pos * 2 + 1] = val2;
        } else {
            snd->u.u8[pos * 2 + 0] = val1;
            snd->u.u8[pos * 2 + 1] = val2;
        }
    } else {
        int val1 = value_to_audio_range(v1, snd->width);
        if (snd->width == 2) {
            snd->u.i16[pos] = val1;
        } else {
            snd->u.u8[pos] = val1;
        }
    }

    return TRUE;
}
static int audio_write(Value *vret, Value *v, RefNode *node)
{
    RefAudio *snd = Value_vp(*v);
    RefBytesIO *mb = Value_vp(v[1]);
    const char *data = mb->buf.p;
    int data_size = mb->buf.size;
    int n = snd->width * snd->channels;

    if (snd->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (data_size % n != 0) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Data length must be %d * n", n);
        return FALSE;
    }
    if (snd->alloc_size < snd->length + data_size) {
        while (snd->alloc_size < snd->length + data_size) {
            snd->alloc_size *= 2;
        }
        snd->u.u8 = realloc(snd->u.u8, snd->alloc_size);
    }
    memcpy(snd->u.u8 + snd->length * n, data, data_size);
    snd->length += data_size / n;

    return 0;
}
static int audio_convert(Value *vret, Value *v, RefNode *node)
{
    RefAudio *src = Value_vp(*v);
    RefAudio *snd;
    int samples = fs->Value_int64(v[1], NULL);
    int bits = fs->Value_int64(v[2], NULL);
    int channels = 1;
    int data_size;

    if (src->u.u8 == NULL) {
        throw_already_closed();
        return FALSE;
    }
    if (samples < 1 && samples > 1000000) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Must be >=1 and <=1000000 (argument #1)");
        return FALSE;
    }
    if (bits != 8 && bits != 16) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Must be 8 or 16 (argument #2)");
        return FALSE;
    }
    if (fg->stk_top > v + 3) {
        channels = fs->Value_int64(v[3], NULL);
        if (channels != 1 && channels != 2) {
            fs->throw_errorf(fs->mod_lang, "ValueError", "Must be 1 or 2 (argument #3)");
            return FALSE;
        }
    }

    snd = fs->buf_new(cls_audio, sizeof(RefAudio));
    *vret = vp_Value(snd);
    snd->samples = samples;
    snd->width = bits / 8;
    snd->channels = channels;
    snd->alloc_size = SOUND_INIT_SIZE;
    snd->length = 0;
    data_size = SOUND_INIT_SIZE * snd->width * snd->channels;
    snd->u.u8 = malloc(data_size);
    memset(snd->u.u8, 0, data_size);

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static int mediareader_new(Value *vret, Value *v, RefNode *node)
{
    Value reader;
    RefStr *type = NULL;

    if (!fs->value_to_streamio(&reader, v[1], FALSE, 0)) {
        return FALSE;
    }
    if (fg->stk_top > v + 2) {
        type = Value_vp(v[2]);
    }

    if (type == NULL && !detect_file_type(&type, reader)) {
        fs->Value_dec(reader);
        return FALSE;
    }


    fs->Value_dec(reader);
    return TRUE;
}
static int mediareader_close(Value *vret, Value *v, RefNode *node)
{
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_mediareader = fs->define_identifier(m, m, "MediaReader", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_mediawriter = fs->define_identifier(m, m, "MediaWriter", NODE_CLASS, NODEOPT_ABSTRACT);
    cls_audio = fs->define_identifier(m, m, "Audio", NODE_CLASS, 0);


    cls = cls_audio;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, audio_new, 2, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "load", NODE_NEW_N, 0);
    fs->define_native_func_a(n, audio_load, 1, 1, (void*)FALSE, NULL);
    n = fs->define_identifier(m, cls, "load_info", NODE_NEW_N, 0);
    fs->define_native_func_a(n, audio_load, 1, 1, (void*)TRUE, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_read, NODE_NEW_N, 0);
    fs->define_native_func_a(n, audio_marshal_read, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_close, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, audio_hash, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->str_marshal_write, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_marshal_write, 1, 1, NULL, fs->cls_marshaldumper);

    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_eq, 1, 1, NULL, cls_audio);
    n = fs->define_identifier(m, cls, "save", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_save, 1, 1, NULL, NULL);
    n = fs->define_identifier(m, cls, "size", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, audio_size, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "length", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, audio_length, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "samples", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, audio_samples, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "bits", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, audio_bits, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "channels", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, audio_channels, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LB], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_index, 2, 2, (void*)FALSE, fs->cls_int, fs->cls_int);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_LET_B], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_index_set, 3, 3, (void*)FALSE, fs->cls_int, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "at", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_index, 2, 2, (void*)TRUE, fs->cls_int, fs->cls_int);
    n = fs->define_identifier(m, cls, "sub", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_sub, 1, 2, NULL, NULL, NULL);
    n = fs->define_identifier(m, cls, "dup", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_sub, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "push", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_push, 1, 2, NULL, fs->cls_number, fs->cls_number);
    n = fs->define_identifier(m, cls, "write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_write, 1, 1, NULL, fs->cls_bytesio);
    n = fs->define_identifier(m, cls, "convert", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, audio_convert, 2, 3, NULL, fs->cls_int, fs->cls_int, fs->cls_int);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_mediareader;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, mediareader_new, 1, 1, NULL, NULL, fs->cls_mimetype);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, mediareader_close, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_mediawriter;
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "AudioError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    RefNode *mod_time;

    fs = a_fs;
    fg = a_fg;
    mod_media = m;

    mod_time = fs->get_module_by_name("time", -1, FALSE, FALSE);
    cls_timedelta = fs->Hash_get(&mod_time->u.m.h, "TimeDelta", -1);
    cls_fileio = fs->Hash_get(&fs->mod_io->u.m.h, "FileIO", -1);

    define_class(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    static char *buf = NULL;

    fs = a_fs;
    if (buf == NULL) {
        Mem mem;
        Hash *hash;
        StrBuf sb;
        int i;
        int first = TRUE;

        fs->Mem_init(&mem, 256);
        hash = load_type_function(&mem);
        fs->StrBuf_init(&sb, 0);
        fs->StrBuf_add(&sb, "Build at\t" __DATE__ "\nSupported type\t", -1);

        for (i = 0; i < hash->entry_num; i++) {
            const HashEntry *he = hash->entry[i];
            for (; he != NULL; he = he->next) {
                if (first) {
                    first = FALSE;
                } else {
                    fs->StrBuf_add(&sb, ", ", 2);
                }
                fs->StrBuf_add_r(&sb, he->key);
            }
        }

        fs->StrBuf_add(&sb, "\n\0", 2);
        buf = sb.p;
        fs->Mem_close(&mem);
    }
    return buf;
}
