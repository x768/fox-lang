#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"
#include <string.h>


static void bmp_swap_rgb(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0 ; i < width; i++) {
        dst[i * 3 + 0] = src[i * 3 + 2];
        dst[i * 3 + 1] = src[i * 3 + 1];
        dst[i * 3 + 2] = src[i * 3 + 0];
    }
}
static void bmp_swap_rgba(uint8_t *dst, const uint8_t *src, int width)
{
    int i;
    for (i = 0 ; i < width; i++) {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }
}

int conv_graphics_to_image(Value *vret, GraphicsHandle img)
{
    int width, height;
    int pitch, has_alpha = FALSE;  // pitch:1行あたりのバイト数
    BITMAP bmp;
    BITMAPINFO bmi;
    int y;
    uint8_t *dst;

    RefImage *fi = fs->buf_new(cls_image, sizeof(RefImage));
    *vret = vp_Value(fi);

    GetObject(img, sizeof(bmp), &bmp);
    width = bmp.bmWidth;
    height = bmp.bmHeight;
    if (width > MAX_IMAGE_SIZE || height > MAX_IMAGE_SIZE) {
        fs->throw_errorf(mod_gui, "GuiError", "Graphics size too large (max:%d)", MAX_IMAGE_SIZE);
        return FALSE;
    }
    pitch = width * 3;
    pitch = (pitch + 3) & ~3;

    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  // 負の数の場合、並びが上から下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    dst = malloc(pitch * height);

    GetDIBits(hDisplayDC, img, 0, height, dst, &bmi, DIB_RGB_COLORS);

    // BGR -> RGB
    for (y = 0; y < height; y++) {
        int x;
        uint8_t *b1 = dst + y * pitch;

        for (x = 0; x < width; x++) {
            uint8_t *p = b1 + x * 3;
            uint8_t tmp = p[2];
            p[2] = p[0];
            p[0] = tmp;
        }
    }
    fi->data = dst;

    if (has_alpha) {
        fi->bands = BAND_RGBA;
    } else {
        fi->bands = BAND_RGB;
    }
    fi->width = width;
    fi->height = height;
    fi->pitch = pitch;

    return TRUE;
}

static HGLOBAL conv_image_to_hglobal(RefImage *image)
{
    int total_size = 0;
    int off_bits = 0;
    int bit_count;
    int pitch = image->pitch;
    int pitch_align;
    HGLOBAL hGlobal;
    BITMAPINFOHEADER *bi;
    uint8_t *buf;

    if (image->bands == BAND_L) {
        bit_count = 8;
        pitch_align = image->width;
    } else if (image->bands == BAND_P) {
        bit_count = 8;
        pitch_align = image->width;
    } else if (image->bands == BAND_RGB) {
        bit_count = 24;
        pitch_align = image->width * 3;
    } else if (image->bands == BAND_RGBA) {
        bit_count = 32;
        pitch_align = image->width * 4;
    } else {
        fs->throw_errorf(mod_image, "ImageError", "Not supported type");
        return NULL;
    }
    pitch_align = (pitch_align + 3) & ~3;
    off_bits = sizeof(BITMAPINFOHEADER) + (bit_count == 8 ? PALETTE_NUM * 4 : 0);
    total_size = off_bits + pitch_align * image->height;

    hGlobal = GlobalAlloc(GHND | GMEM_SHARE, total_size);

    // ヘッダ
    bi = (BITMAPINFOHEADER*)GlobalLock(hGlobal);
    memset(bi, 0, sizeof(BITMAPINFOHEADER));
    bi->biSize = sizeof(BITMAPINFOHEADER);
    bi->biWidth = image->width;
    bi->biHeight = image->height;
    bi->biPlanes = 1;
    bi->biBitCount = bit_count;
    bi->biCompression = BI_RGB;

    buf = ((uint8_t*)bi) + sizeof(BITMAPINFOHEADER);

    // パレット
    if (bit_count == 8) {
        int i;
        if (image->palette != NULL) {
            const uint32_t *palette = image->palette;
            for (i = 0; i < PALETTE_NUM; i++) {
                uint32_t c = palette[i];
                buf[i * 4 + 0] = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
                buf[i * 4 + 1] = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
                buf[i * 4 + 2] = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
                buf[i * 4 + 3] = 0;
            }
        } else {
            // グレイスケール画像
            for (i = 0; i < PALETTE_NUM; i++) {
                buf[i * 4 + 0] = i;
                buf[i * 4 + 1] = i;
                buf[i * 4 + 2] = i;
                buf[i * 4 + 3] = 0;
            }
        }
        buf += PALETTE_NUM * 4;
    }

    // 画像情報
    {
        int y;
        int width = image->width;
        int height = image->height;

        for (y = 0; y < height; y++) {
            uint8_t *src = (uint8_t*)(image->data + (height - y - 1) * pitch);
            switch (bit_count) {
            case 8:
                memcpy(buf, src, pitch);
                break;
            case 24:
                bmp_swap_rgb(buf, src, width);
                break;
            case 32:
                bmp_swap_rgba(buf, src, width);
                break;
            }
            buf += pitch_align;
        }
    }

    GlobalUnlock(hGlobal);
    return hGlobal;
}

GraphicsHandle conv_image_to_graphics(RefImage *img)
{
    GraphicsHandle handle = NULL;
    int width = img->width;
    int height = img->height;
    int src_pitch = img->pitch;
    const uint8_t *src_buf = img->data;
    uint8_t *palette = NULL;
    int x, y;
    BITMAPINFO bi;
    uint8_t *dst_buf;
    int dst_pitch = (width * 3 + 3) & ~3;

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = height;

    handle = CreateDIBSection(hDisplayDC, &bi, DIB_RGB_COLORS, (void**)&dst_buf, NULL, 0);
    if (handle == NULL) {
        goto FINALLY;
    }

    if (img->bands == BAND_P) {
        palette = malloc(3 * PALETTE_NUM);
        for (x = 0; x < PALETTE_NUM; x++) {
            uint32_t c = img->palette[x];
            palette[x * 3 + 0] = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
            palette[x * 3 + 1] = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
            palette[x * 3 + 2] = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
        }
    }

    for (y = 0; y < height; y++) {
        const uint8_t *src_line = src_buf + src_pitch * y;
        uint8_t *dst_line = dst_buf + dst_pitch * (height - y - 1);
        switch (img->bands) {
        case BAND_RGBA:
            for (x = 0; x < width; x++) {
                dst_line[x * 3 + 0] = src_line[x * 4 + 2];
                dst_line[x * 3 + 1] = src_line[x * 4 + 1];
                dst_line[x * 3 + 2] = src_line[x * 4 + 0];
            }
            break;
        case BAND_RGB:
            for (x = 0; x < width; x++) {
                dst_line[x * 3 + 0] = src_line[x * 3 + 2];
                dst_line[x * 3 + 1] = src_line[x * 3 + 1];
                dst_line[x * 3 + 2] = src_line[x * 3 + 0];
            }
            break;
        case BAND_P:
            for (x = 0; x < width; x++) {
                int c = src_line[x] * 3;
                dst_line[x * 3 + 0] = palette[c + 0];
                dst_line[x * 3 + 1] = palette[c + 1];
                dst_line[x * 3 + 2] = palette[c + 2];
            }
            break;
        case BAND_LA:
            for (x = 0; x < width; x++) {
                uint8_t c = src_line[x * 2];
                dst_line[x * 3 + 0] = c;
                dst_line[x * 3 + 1] = c;
                dst_line[x * 3 + 2] = c;
            }
            break;
        case BAND_L:
            for (x = 0; x < width; x++) {
                uint8_t c = src_line[x];
                dst_line[x * 3 + 0] = c;
                dst_line[x * 3 + 1] = c;
                dst_line[x * 3 + 2] = c;
            }
            break;
        }
    }
FINALLY:

    if (palette != NULL) {
        free(palette);
    }
    if (handle == NULL) {
        fs->throw_errorf(mod_gui, "GuiError", "CreateDIBSection failed");
        return NULL;
    }

    return handle;
}

////////////////////////////////////////////////////////////////////////////

int clipboard_clear()
{
    if (OpenClipboard(NULL) == 0) {
        return TRUE;
    }
    EmptyClipboard();
    CloseClipboard();
    return TRUE;
}

int clipboard_is_available(const RefNode *klass)
{
    if (klass == fs->cls_str) {
        return IsClipboardFormatAvailable(CF_UNICODETEXT);
    } else if (klass == fs->cls_file) {
        return IsClipboardFormatAvailable(CF_HDROP);
    } else if (klass == cls_image) {
        return IsClipboardFormatAvailable(CF_BITMAP);
    } else {
        return FALSE;
    }
}

static HGLOBAL cstr_to_hglobal(const char *src_p, int src_size)
{
    int wlen;
    HGLOBAL hGlobal;
    wchar_t *buf;

    if (src_size < 0) {
        src_size = strlen(src_p);
    }
    wlen = MultiByteToWideChar(CP_UTF8, 0, src_p, src_size, NULL, 0);
    hGlobal = GlobalAlloc(GHND, (wlen + 1) * sizeof(wchar_t));

    buf = (wchar_t*)GlobalLock(hGlobal);
    MultiByteToWideChar(CP_UTF8, 0, src_p, src_size, buf, wlen + 1);
    buf[wlen] = L'\0';
    GlobalUnlock(hGlobal);

    return hGlobal;
}

Value clipboard_get_text()
{
    if (IsClipboardFormatAvailable(CF_UNICODETEXT) && OpenClipboard(NULL) != 0) {
        Value v;
        wchar_t *wbuf;
        HGLOBAL hGlobal = GetClipboardData(CF_UNICODETEXT);

        if (hGlobal == NULL){
            CloseClipboard();
            return TRUE;
        }
        wbuf = (wchar_t*)GlobalLock(hGlobal);
        v = utf16_str_Value(wbuf);

        GlobalUnlock(hGlobal);
        CloseClipboard();
        return v;
    } else {
        return VALUE_NULL;
    }
}
void clipboard_set_text(const char *src_p, int src_size)
{
    if (OpenClipboard(NULL) != 0) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, cstr_to_hglobal(src_p, src_size));
        CloseClipboard();
    }
}

int clipboard_get_files(Value *v, int uri)
{
    if (IsClipboardFormatAvailable(CF_HDROP) && OpenClipboard(NULL) != 0) {
        int i;
        RefNode *fn_to_uri = fs->Hash_get(&fs->cls_file->u.c.h, "to_uri", 6);
        wchar_t wbuf[MAX_PATH];
        HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
        int n = DragQueryFileW(hDrop, 0xffffffff, NULL, 0);
        RefArray *afile = fs->refarray_new(0);
        *v = vp_Value(afile);

        for (i = 0; i < n; i++) {
            Value *vf;
            DragQueryFileW(hDrop, i, wbuf, MAX_PATH);
            vf = fs->refarray_push(afile);
            *vf = utf16_file_Value(wbuf);
            if (uri) {
                fs->Value_push("v", *vf);
                if (!fs->call_function(fn_to_uri, 0)) {
                    return FALSE;
                }
                fg->stk_top--;
                *vf = *fg->stk_top;
            }
        }
        DragFinish(hDrop);

        CloseClipboard();
    }
    return TRUE;
}
int clipboard_set_files(Value *v, int num)
{
    if (OpenClipboard(NULL) != 0) {
        CloseClipboard();
    }
    return TRUE;
}

int clipboard_get_image(Value *v)
{
    int ret = TRUE;

    if (IsClipboardFormatAvailable(CF_BITMAP) && OpenClipboard(NULL) != 0) {
        HBITMAP hBmp = GetClipboardData(CF_BITMAP);
        if (!conv_graphics_to_image(v, hBmp)) {
            ret = FALSE;
        }
        CloseClipboard();
    }
    return ret;
}

int clipboard_set_image(RefImage *img)
{
    if (OpenClipboard(NULL) != 0) {
        HGLOBAL hGlobal = conv_image_to_hglobal(img);

        if (hGlobal == NULL) {
            return FALSE;
        }
        EmptyClipboard();
        SetClipboardData(CF_DIB, hGlobal);
        CloseClipboard();
    }
    return TRUE;
}
