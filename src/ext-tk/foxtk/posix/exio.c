#include "gui_compat.h"
#include "gui.h"
#include <stdlib.h>

static GFile *filename_to_gfile(RefStr *rs)
{
    // NULを含む場合エラー
    if (str_has0(rs->c, rs->size)) {
        fs->throw_errorf(fs->mod_io, "FileOpenError", "File path contains '\\0'");
        return NULL;
    }
    return g_file_new_for_path(rs->c);
}
static GFile *value_to_gfile(Value v)
{
    const RefNode *type = fs->Value_type(v);
    if (type == fs->cls_str || type == fs->cls_file) {
        return filename_to_gfile(Value_vp(v));
    } else if (type == fs->cls_uri) {
        RefStr *rs = Value_vp(v);
        GFile *gf = g_file_new_for_uri(rs->c);
        return gf;
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "Str, File or Uri required but %n", type);
        return NULL;
    }
}

int uri_to_file_sub(Value *dst, Value src)
{
    GFile *gf = value_to_gfile(src);
    char *pdst = g_file_get_path(gf);
    if (pdst != NULL) {
        *dst = fs->cstr_Value(fs->cls_file, pdst, -1);
        g_free(pdst);
        g_object_unref(G_OBJECT(gf));
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Cannot convert to File");
        g_object_unref(G_OBJECT(gf));
        return FALSE;
    }
    return TRUE;
}

int exio_trash_sub(Value vdst)
{
    GFile *dst = value_to_gfile(vdst);
    GError *error;
    int result = TRUE;

    if (dst == NULL) {
        return FALSE;
    }
    if (!g_file_trash(dst, NULL, &error)) {
        fs->throw_errorf(fs->mod_io, "IOError", "%s", error != NULL ? error->message : "An error occured");
        result = FALSE;
    }
    g_object_unref(G_OBJECT(dst));

    return result;
}
