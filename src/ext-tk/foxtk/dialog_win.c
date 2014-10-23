#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"
#include <string.h>
#include <shlobj.h>



void alert_dialog(RefStr *msg, RefStr *title, WndHandle parent)
{
    wchar_t *wmsg = cstr_to_utf16(msg->c, msg->size);
    wchar_t *wtitle;

    if (title != NULL) {
        wtitle = cstr_to_utf16(title->c, title->size);
    } else {
        wtitle = NULL;
    }

    MessageBoxW(parent, wmsg, (wtitle != NULL ? wtitle : L""), MB_OK|MB_ICONINFORMATION);
    free(wmsg);
    free(wtitle);
}

int confirm_dialog(RefStr *msg, RefStr *title, WndHandle parent)
{
    int ret = FALSE;
    wchar_t *wmsg = cstr_to_utf16(msg->c, msg->size);
    wchar_t *wtitle;

    if (title != NULL) {
        wtitle = cstr_to_utf16(title->c, title->size);
    } else {
        wtitle = NULL;
    }

    ret = (MessageBoxW(parent, wmsg, (wtitle != NULL ? wtitle : L""), MB_YESNO|MB_ICONQUESTION) == IDYES);
    free(wmsg);
    free(wtitle);

    return ret;
}

/**
 * *.bmp;*.jpg -> "*.bmp;*.jpg", "*.bmp;*.jpg"
 * Image file:*.bmp;*.jpg -> "Image file", "*.bmp;*.jpg"
 */
static void split_filter_name(Str *name, Str *filter, Str src)
{
    int i;

    for (i = src.size - 1; i >= 0; i--) {
        if (src.p[i] == ':') {
            name->p = src.p;
            name->size = i;
            filter->p = src.p + i + 1;
            filter->size = src.size - i - 1;
            return;
        }
    }
    *name = src;
    *filter = src;
}

static wchar_t *make_filter_string(RefArray *r)
{
    int i;
    int pos = 0;
    int max = 256;
    wchar_t *buf = malloc(max * sizeof(wchar_t));

    for (i = 0; i < r->size; i++) {
        Str name, ft;
        int name_len, ft_len;
        wchar_t *wname, *wft;
        int next;

        split_filter_name(&name, &ft, fs->Value_str(r->p[i]));

        wname = cstr_to_utf16(name.p, name.size);
        wft = cstr_to_utf16(ft.p, ft.size);

        // 追加後サイズ計算
        name_len = wcslen(wname);
        ft_len = wcslen(wft);
        next = pos + name_len + ft_len + 8;
        if (next > max) {
            while (next > max) {
                max *= 2;
            }
            buf = realloc(buf, max * sizeof(wchar_t));
        }

        // 文字列を\0区切りで追加
        wcscpy(&buf[pos], wname);
        pos += name_len + 1;
        wcscpy(&buf[pos], wft);
        pos += ft_len + 1;
    }
    buf[pos++] = L'\0';
    buf[pos++] = L'\0';

    return buf;
}
static void array_add_file(RefArray *r, const wchar_t *p)
{
    Value *vf = fs->refarray_push(r);
    *vf = utf16_file_Value(p);
}
static wchar_t *concat_filename(const wchar_t *dir, const wchar_t *file)
{
    int dlen = wcslen(dir);
    int flen = wcslen(file);
    wchar_t *p = malloc((dlen + flen + 2) * sizeof(wchar_t));

    wcscpy(p, dir);
    if (dlen > 0 && dir[dlen - 1] != L'\\') {
        p[dlen] = L'\\';
        wcscpy(&p[dlen + 1], file);
    } else {
        wcscpy(&p[dlen], file);
    }
    return p;
}

int file_open_dialog(Value *vret, Str title, RefArray *filter, WndHandle parent, int type)
{
    enum {
        RETBUF_MAX = 32 * 1024,
    };
    int ret = FALSE;
    wchar_t *title_p = cstr_to_utf16(title.p, title.size);
    wchar_t *filter_p = NULL;
    wchar_t *wbuf = malloc(RETBUF_MAX);
    int offset = 0;

    memset(wbuf, 0, RETBUF_MAX);

    if (type == FILEOPEN_DIR) {
        BROWSEINFOW bi;
        LPMALLOC pm = NULL;

        if (SHGetMalloc(&pm) != E_FAIL) {
            ITEMIDLIST *id;

            memset(&bi, 0, sizeof(bi));
            bi.hwndOwner = parent;
            bi.ulFlags = BIF_RETURNONLYFSDIRS;
            bi.lpszTitle = title_p;

            id = SHBrowseForFolderW(&bi);
            if (id != NULL){
                SHGetPathFromIDListW(id, wbuf);
                pm->lpVtbl->Free(pm, id);
                pm->lpVtbl->Release(pm);
                ret = TRUE;
            }
        }
    } else {
        OPENFILENAMEW of;
        memset(&of, 0, sizeof(of));
        of.hwndOwner = parent;
        of.lpstrFile = wbuf;
        of.nMaxFile = RETBUF_MAX - 16;
        of.hInstance = GetModuleHandle(NULL);
        of.lStructSize = sizeof(of);
        of.Flags = OFN_NOCHANGEDIR | OFN_CREATEPROMPT;
        of.lpstrTitle = title_p;

        if (filter != NULL) {
            filter_p = make_filter_string(filter);
            of.lpstrFilter = filter_p;
        }

        switch (type) {
        case FILEOPEN_OPEN:
            of.Flags |= OFN_HIDEREADONLY;
            ret = (GetOpenFileNameW(&of) != 0);
            break;
        case FILEOPEN_OPEN_MULTI:
            of.Flags |= OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
            ret = (GetOpenFileNameW(&of) != 0);
            offset = of.nFileOffset;
            break;
        case FILEOPEN_SAVE:
            of.Flags |= OFN_OVERWRITEPROMPT | OFN_EXTENSIONDIFFERENT | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
            ret = (GetSaveFileNameW(&of) != 0);
            break;
        }
    }

    if (ret) {
        if (type == FILEOPEN_OPEN_MULTI) {
            RefArray *aret = fs->refarray_new(0);
            wchar_t *p = wbuf;
            int dlen = wcslen(p);
            *vret = vp_Value(aret);

            if (offset <= dlen) {
                array_add_file(aret, p);
            } else {
                wchar_t *dir_part = p;
                p = &p[offset];

                while (*p != L'\0'){
                    wchar_t *filename = concat_filename(dir_part, p);
                    array_add_file(aret, filename);
                    free(filename);
                    p += wcslen(p) + 1;
                }
            }
        } else {
            *vret = utf16_file_Value(wbuf);
        }
    }

    free(title_p);
    free(wbuf);
    free(filter_p);

    return ret;
}

