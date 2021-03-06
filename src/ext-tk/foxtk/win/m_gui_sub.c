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
