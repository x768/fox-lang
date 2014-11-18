#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"
#include <stdio.h>


HDC hDisplayDC;

typedef struct {
    RefHeader rh;
    void *handle;
} RefHandle;

void gui_initialize()
{
    hDisplayDC = CreateDCW(L"DISPLAY", NULL, NULL, NULL);
}
void gui_dispose()
{
    DeleteDC(hDisplayDC);
}
void strcat_wchar_to_char(char *dst, const wchar_t *src)
{
    while (*dst != '\0') {
        dst++;
    }
    while (*src != L'\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
}
char *get_toolkit_version()
{
    OSVERSIONINFOW ver;
    char *p = malloc(128);

    ver.dwOSVersionInfoSize = sizeof(ver);
    GetVersionExW(&ver);

    sprintf(p, "Windows %lu.%lu ", ver.dwMajorVersion, ver.dwMinorVersion);
    strcat_wchar_to_char(p, ver.szCSDVersion);

    return p;
}


wchar_t *cstr_to_utf16(const char *src, int src_size)
{
    int wlen;
    wchar_t *buf;

    if (src_size < 0) {
        src_size = strlen(src);
    }
    wlen = MultiByteToWideChar(CP_UTF8, 0, src, src_size, NULL, 0);
    buf = malloc((wlen + 2) * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, src, src_size, buf, wlen + 1);
    buf[wlen] = L'\0';
    buf[wlen+1] = L'\0';

    return buf;
}
Value utf16_str_Value(const wchar_t *src)
{
    Value v;
    int len = wcslen(src);
    int clen = WideCharToMultiByte(CP_UTF8, 0, src, len, NULL, 0, NULL, NULL);
    char *buf = malloc(clen + 1);
    WideCharToMultiByte(CP_UTF8, 0, src, len, buf, clen + 1, NULL, NULL);
    buf[clen] = '\0';

    v = fs->cstr_Value_conv(buf, -1, NULL);
    free(buf);
    return v;
}
Value utf16_file_Value(const wchar_t *src)
{
    Value v = utf16_str_Value(src);
    Value_ref_header(v)->type = fs->cls_file;
    return v;
}

HBITMAP duplicate_bitmap(HBITMAP hSrcBmp)
{
    BITMAP bitmap;
    HDC hSrcDC, hDstDC;
    HBITMAP hDstBmp;

    GetObject(hSrcBmp, sizeof(bitmap), &bitmap);
    hDstBmp = CreateCompatibleBitmap(hDisplayDC, bitmap.bmWidth, bitmap.bmHeight);

    hSrcDC = CreateCompatibleDC(hDisplayDC);
    SelectObject(hSrcDC, hSrcBmp);
    hDstDC = CreateCompatibleDC(hDisplayDC);
    SelectObject(hDstDC, hDstBmp);
    BitBlt(hDstDC, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hSrcDC, 0, 0, SRCCOPY);

    DeleteDC(hSrcDC);

    return hDstBmp;
}

void *Value_handle(Value v)
{
    if (Value_isref(v)) {
        RefHandle *rh = Value_vp(v);
        return (void*)rh->handle;
    } else {
        return NULL;
    }
}
Value handle_Value(void *handle)
{
    RefHandle *rh = fs->buf_new(NULL, sizeof(RefHandle));
    rh->handle = (WndHandle)handle;
    return vp_Value(rh);
}
