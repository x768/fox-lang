#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"
#include <stdio.h>


static const wchar_t * const FORM_CLASS = L"FoxGuiWindow";


static int wndproc_drop_event(Ref *sender_r, HDROP hDrop)
{
    Value *eh = get_event_handler(sender_r, fs->intern("dropped", -1));

    if (eh != NULL) {
        wchar_t wbuf[MAX_PATH];
        int i;
        int n = DragQueryFileW(hDrop, 0xffffffff, NULL, 0);
        Value evt;
        int result;
        POINT pt;

        RefArray *afile = fs->refarray_new(0);
        DragQueryPoint(hDrop, &pt);

        for (i = 0; i < n; i++) {
            Value *vf;
            DragQueryFileW(hDrop, i, wbuf, MAX_PATH);
            vf = fs->refarray_push(afile);
            *vf = fs->wstr_Value(fs->cls_file, wbuf, -1);
        }
        DragFinish(hDrop);

        evt = event_object_new(sender_r);
        event_object_add(evt, "files", vp_Value(afile));
        fs->unref(vp_Value(afile));

        event_object_add(evt, "x", int32_Value(pt.x));
        event_object_add(evt, "y", int32_Value(pt.y));

        if (!invoke_event_handler(&result, *eh, evt)) {
            //
        }
        fs->unref(evt);
    }
    return TRUE;
}
static int button_proc_sub(Ref *sender_r, const char *type, int button, int x, int y, int dir)
{
    int i;
    RefArray *ra = Value_vp(sender_r->v[INDEX_FORM_CHILDREN]);
    Value *eh;

    for (i = 0; i < ra->size; i++) {
        Ref *child = Value_ref(ra->p[i]);
        int left = Value_integral(child->v[INDEX_WIDGET_X]);
        int top = Value_integral(child->v[INDEX_WIDGET_Y]);
        int right = left + Value_integral(child->v[INDEX_WIDGET_W]);
        int bottom = top + Value_integral(child->v[INDEX_WIDGET_H]);
        if (x >= left && x < right && y >= top && y < bottom) {
            sender_r = child;
            break;
        }
    }
    eh = get_event_handler(sender_r, fs->intern(type, -1));

    if (eh != NULL) {
        const char *button_val = NULL;
        Value evt = event_object_new(sender_r);

        switch (button) {
        case 1:
            button_val = "left";
            break;
        case 2:
            button_val = "middle";
            break;
        case 3:
            button_val = "right";
            break;
        }

        if (button_val != NULL) {
            Value v_tmp = fs->cstr_Value(fs->cls_str, button_val, -1);
            event_object_add(evt, "button", v_tmp);
            fs->unref(v_tmp);
        }

        event_object_add(evt, "x", int32_Value(x));
        event_object_add(evt, "y", int32_Value(y));

        if (button == -1) {
            event_object_add(evt, "dir", int32_Value(dir));
        }

        if (!invoke_event_handler(NULL, *eh, evt)) {
            //
        }

        fs->unref(evt);
    }
    return TRUE;
}
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE:
        return 0;
    case WM_DESTROY:
        widget_handler_destroy(r);
        fs->unref(r->v[INDEX_FORM_HANDLE]);
        r->v[INDEX_FORM_HANDLE] = VALUE_NULL;
        return 0;
    case WM_CLOSE: {
        Value *eh = get_event_handler(r, fs->intern("closed", -1));
        int result = TRUE;

        if (eh != NULL) {
            Value evt = event_object_new(r);
            if (!invoke_event_handler(&result, *eh, evt)) {
                //
            }
            fs->unref(evt);
        }
        if (fg->error != VALUE_NULL) {
            PostQuitMessage(0);
        }
        if (!result) {
            return 0;
        }
        break;
    }
    case WM_SIZE:
        if (!IsIconic(hWnd)) {
        } else {
        }
        return 0;
    case WM_COMMAND: {
        Ref *sender_r = get_menu_object(LOWORD(wParam));
        if (sender_r != NULL) {
            Value evt = event_object_new(sender_r);
            invoke_event(evt, sender_r->v[INDEX_MENUITEM_FN]);
            fs->unref(evt);
        }
        return 0;
    }
    case WM_DROPFILES:
        wndproc_drop_event(r, (HDROP)wParam);
        if (fg->error != VALUE_NULL) {
            PostQuitMessage(0);
        }
        return 0;
    case WM_LBUTTONDOWN:
        button_proc_sub(r, "mouse_pressed", 1, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        break;
    case WM_LBUTTONUP:
        button_proc_sub(r, "mouse_released", 1, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        break;
    case WM_RBUTTONDOWN:
        button_proc_sub(r, "mouse_pressed", 3, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        break;
    case WM_RBUTTONUP:
        button_proc_sub(r, "mouse_released", 3, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        break;
    case WM_MBUTTONDOWN:
        button_proc_sub(r, "mouse_pressed", 2, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        break;
    case WM_MBUTTONUP:
        button_proc_sub(r, "mouse_released", 2, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        break;
    case WM_MOUSEWHEEL: {
        POINT pt;
        int dir = -(short)HIWORD(wParam) / 120;
        pt.x = (short)LOWORD(lParam);
        pt.y = (short)HIWORD(lParam);
        ScreenToClient(hWnd, &pt);
        button_proc_sub(r, "wheel_scrolled", -1, pt.x, pt.y, dir);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(hWnd, &ps);
        int i;
        RefArray *ra = Value_vp(r->v[INDEX_FORM_CHILDREN]);

        for (i = 0; i < ra->size; i++) {
            Ref *ref = Value_ref(ra->p[i]);
            GuiCallback *gc = Value_ptr(ref->v[INDEX_WIDGET_CALLBACK]);
            if (gc != NULL && gc->on_paint != NULL) {
                gc->on_paint(ref, hWnd, hDC);
            }
        }
        EndPaint(hWnd, &ps);
        break;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static int register_class(void)
{
    static int initialized = FALSE;

    if (!initialized) {
        WNDCLASSEXW wc;

        wc.cbSize        = sizeof(wc);
        wc.style         = 0;
        wc.lpfnWndProc   = WndProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = GetModuleHandle(NULL);
        wc.hIcon         = NULL;
        wc.hIconSm       = NULL;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = FORM_CLASS;

        if (RegisterClassExW(&wc) == 0) {
            return FALSE;
        }
        initialized = TRUE;
    }
    return TRUE;
}

void create_form_window(Ref *r, WndHandle parent, int *size)
{
    HWND window = NULL;
    DWORD style;

    register_class();
    if (size != NULL) {
        style = WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME;
    } else {
        style = WS_OVERLAPPEDWINDOW;
    }
    window = CreateWindowW(
        FORM_CLASS,
        L"",
        style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        parent,
        NULL,
        GetModuleHandle(NULL),
        NULL);

    r->v[INDEX_FORM_ALPHA] = int32_Value(255);
    r->v[INDEX_FORM_HANDLE] = handle_Value(window);

    SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)r);

    if (size != NULL) {
        RECT rc;
        rc.left = 0;
        rc.top = 0;
        rc.right = size[0];
        rc.bottom = size[1];
        AdjustWindowRect(&rc, style, FALSE);
        SetWindowPos(window, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER|SWP_NOMOVE);
    }
}
void window_destroy(WndHandle window)
{
    DestroyWindow(window);
}

void window_set_drop_handler(WndHandle window)
{
    DragAcceptFiles(window, TRUE);
}

double window_get_opacity(Ref *r)
{
    return (double)Value_integral(r->v[INDEX_FORM_ALPHA]) / 255.0;
}
void window_set_opacity(Ref *r, double opacity)
{
    WndHandle window = Value_handle(r->v[INDEX_FORM_HANDLE]);
    int iop = (int)(opacity * 255.0);
    if (iop == 255) {
        if (Value_integral(r->v[INDEX_FORM_ALPHA]) < 255) {
            SetWindowLongPtr(window, GWL_EXSTYLE, GetWindowLongPtr(window, GWL_EXSTYLE) & ~WS_EX_LAYERED);
        }
    } else {
        if (Value_integral(r->v[INDEX_FORM_ALPHA]) == 255) {
            SetWindowLongPtr(window, GWL_EXSTYLE, GetWindowLongPtr(window, GWL_EXSTYLE) | WS_EX_LAYERED);
        }
        SetLayeredWindowAttributes(window, 0, iop, LWA_ALPHA);
    }
    r->v[INDEX_FORM_ALPHA] = int32_Value(iop);
}
void window_get_title(Value *vret, WndHandle window)
{
    int len = GetWindowTextLengthW(window);
    wchar_t *wbuf = malloc((len + 1) * sizeof(wchar_t));
    GetWindowTextW(window, wbuf, len + 1);
    *vret = fs->wstr_Value(fs->cls_str, wbuf, -1);
    free(wbuf);
}
void window_set_title(WndHandle window, const char *s)
{
    wchar_t *title_p = cstr_to_utf16(s, -1);
    SetWindowTextW(window, title_p);
    free(title_p);
}
void window_move(WndHandle window, int x, int y)
{
    if (x < 0) {
        x += GetSystemMetrics(SM_CXSCREEN);
    }
    if (y < 0) {
        y += GetSystemMetrics(SM_CYSCREEN);
    }
    SetWindowPos(window, NULL, x, y, 0, 0, SWP_NOZORDER|SWP_NOSIZE);
}
// TODO
void window_move_center(WndHandle window)
{
}
void window_set_client_size(WndHandle window, int width, int height)
{
    // クライアント領域のサイズを設定
    RECT rc;
    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    AdjustWindowRect(&rc, GetWindowLongPtr(window, GWL_STYLE), FALSE);
    SetWindowPos(window, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER|SWP_NOMOVE);
}

int window_get_visible(WndHandle window)
{
    return (GetWindowLongPtr(window, GWL_STYLE) & WS_VISIBLE) != 0;
}
void window_set_visible(WndHandle window, int show)
{
    ShowWindow(window, (show ? SW_SHOW : SW_HIDE));
}
void window_set_keep_above(WndHandle window, int above)
{
    SetWindowPos(window, (above ? HWND_TOPMOST : HWND_NOTOPMOST), 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
}

//////////////////////////////////////////////////////////////////////////////////////////

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
static HICON image_to_icon(RefImage *image, uint8_t **pbuf)
{
    enum {
        PIX_PER_METER = 2835,
    };
    uint8_t *buf;
    BITMAPINFOHEADER *hdr;

    int total_size = 0;
    int off_bits = 0;
    int bit_count;
    int pitch = image->pitch;
    int pitch_align = (pitch + 3) & ~3;
    int mask_pitch = (pitch + 7) / 8;

    switch (image->bands) {
    case BAND_L:
        bit_count = 8;
        break;
    case BAND_P:
        bit_count = 8;
        break;
    case BAND_RGB:
        bit_count = 24;
        break;
    case BAND_RGBA:
        bit_count = 32;
        break;
    default:
        fs->throw_errorf(mod_image, "ImageError", "Not supported type");
        return NULL;
    }

    off_bits = sizeof(BITMAPINFOHEADER) + (bit_count == 8 ? PALETTE_NUM * 4 : 0);
    total_size = off_bits + (pitch_align + mask_pitch) * image->height * 2;

    buf = malloc(total_size);
    memset(buf, 0, total_size);
    hdr = (BITMAPINFOHEADER*)buf;

    hdr->biSize = sizeof(BITMAPINFOHEADER);
    hdr->biWidth = image->width;
    hdr->biHeight = image->height * 2;
    hdr->biPlanes = 1;
    hdr->biBitCount = bit_count;
    hdr->biCompression = BI_RGB;
    hdr->biSizeImage = (pitch_align + mask_pitch) * image->height;
    hdr->biXPelsPerMeter = PIX_PER_METER;
    hdr->biYPelsPerMeter = PIX_PER_METER;
    hdr->biClrUsed = (bit_count == 8 ? 256 : 0);
    hdr->biClrImportant = 0;

    // パレット
    if (bit_count == 8) {
        int i;
        uint8_t *pal = buf + sizeof(BITMAPINFOHEADER);

        if (image->palette != NULL) {
            const uint32_t *palette = image->palette;
            for (i = 0; i < PALETTE_NUM; i++) {
                uint32_t c = palette[i];
                pal[i * 4 + 0] = (c & COLOR_B_MASK) >> COLOR_B_SHIFT;
                pal[i * 4 + 1] = (c & COLOR_G_MASK) >> COLOR_G_SHIFT;
                pal[i * 4 + 2] = (c & COLOR_R_MASK) >> COLOR_R_SHIFT;
                pal[i * 4 + 3] = 0;
            }
        } else {
            // グレイスケール画像
            for (i = 0; i < PALETTE_NUM; i++) {
                pal[i * 4 + 0] = i;
                pal[i * 4 + 1] = i;
                pal[i * 4 + 2] = i;
                pal[i * 4 + 3] = 0;
            }
        }
    }

    // 画像情報
    {
        int y;
        int width = image->width;
        int height = image->height;

        for (y = 0; y < height; y++) {
            const uint8_t *src = (uint8_t*)(image->data + (height - y - 1) * pitch);
            uint8_t *dst = &buf[off_bits + pitch_align * y];

            switch (bit_count) {
            case 8:
                memcpy(dst, src, pitch);
                break;
            case 24:
                bmp_swap_rgb(dst, src, width);
                break;
            case 32:
                bmp_swap_rgba(dst, src, width);
                break;
            }
        }
    }

    *pbuf = buf;
    return CreateIconFromResourceEx((BYTE*)buf, total_size,
            TRUE, 0x00030000, image->width, image->height, LR_DEFAULTCOLOR);
}

int window_set_icon(WndHandle window, Ref *r, RefImage *icon)
{
    uint8_t *buf = NULL;
    HICON img = image_to_icon(icon, &buf);
    HICON old_icon;
    if (img == NULL) {
        fs->throw_errorf(mod_gui, "GuiError", "CreateIconFromResourceEx failed");
        return FALSE;
    }

    SetClassLongPtr(window, GCLP_HICON, (LONG_PTR)img);
    old_icon = (HICON)SendMessageW(window, WM_SETICON, ICON_SMALL, (LPARAM)img);
    if (Value_bool(r->v[INDEX_FORM_NEW_ICON])) {
        DestroyIcon(old_icon);
    } else {
        r->v[INDEX_FORM_NEW_ICON] = VALUE_TRUE;
    }
    free(buf);
    return TRUE;
}

int window_message_loop()
{
    MSG msg;

    if (MsgWaitForMultipleObjectsEx(0, NULL, INFINITE, QS_ALLEVENTS|QS_ALLINPUT|QS_ALLPOSTMESSAGE, 0) == 0xFFFFFFFF) {
        fs->throw_errorf(fs->mod_lang, "InternalError", "MsgWaitForMultipleObjectsEx failed");
        return FALSE;
    }
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return TRUE;
}
