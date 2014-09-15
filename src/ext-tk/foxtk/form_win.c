#include "gui_compat.h"
#include "gui.h"
#include "gui_win.h"


static const wchar_t * const FORM_CLASS = L"FoxGuiWindow";
static const wchar_t * const IMAGE_PANE_CLASS = L"FoxImagePane";


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
			*vf = utf16_file_Value(wbuf);
		}
		DragFinish(hDrop);

		evt = event_object_new(sender_r);
		event_object_add(evt, "files", vp_Value(afile));
		fs->Value_dec(vp_Value(afile));

		event_object_add(evt, "x", int32_Value(pt.x));
		event_object_add(evt, "y", int32_Value(pt.y));

		if (!invoke_event_handler(&result, *eh, evt)) {
			//
		}
		fs->Value_dec(evt);
	}
	return TRUE;
}
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE:
		return 0;
	case WM_DESTROY: {
		Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		widget_handler_destroy(r);
		fs->Value_dec(r->v[INDEX_WIDGET_HANDLE]);
		r->v[INDEX_WIDGET_HANDLE] = VALUE_NULL;

		root_window_count--;
		if (root_window_count <= 0) {
			PostQuitMessage(0);
		}
		return 0;
	}
	case WM_CLOSE: {
		Ref *sender_r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		Value *eh = get_event_handler(sender_r, fs->intern("closed", -1));
		int result = TRUE;

		if (eh != NULL) {
			Value evt = event_object_new(sender_r);
			if (!invoke_event_handler(&result, *eh, evt)) {
				//
			}
			fs->Value_dec(evt);
		}
		if (fg->error != VALUE_NULL) {
			PostQuitMessage(0);
		}
		if (!result) {
			return 0;
		}
		break;
	}
	case WM_SIZE: {
		if (!IsIconic(hWnd)){
			RECT rc;
			HWND hChild = GetDlgItem(hWnd, ID_CHILD_PANE);
			GetClientRect(hWnd, &rc);
			MoveWindow(hChild, 0, 0, rc.right, rc.bottom, TRUE);
		}
		break;
	}
	case WM_COMMAND: {
		Ref *sender_r = get_menu_object(LOWORD(wParam));
		if (sender_r != NULL) {
			Value evt = event_object_new(sender_r);
			invoke_event(evt, sender_r->v[INDEX_MENUITEM_FN]);
			fs->Value_dec(evt);
		}
		return 0;
	}
	case WM_DROPFILES:
		wndproc_drop_event((Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA), (HDROP)wParam);
		if (fg->error != VALUE_NULL) {
			PostQuitMessage(0);
		}
		return 0;
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static int register_class()
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

static int button_proc_sub(Ref *sender_r, const char *type, int button, int x, int y, int dir)
{
	Value *eh = get_event_handler(sender_r, fs->intern(type, -1));

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
			fs->Value_dec(v_tmp);
		}

		event_object_add(evt, "x", int32_Value(x));
		event_object_add(evt, "y", int32_Value(y));

		if (button == -1) {
			event_object_add(evt, "dir", int32_Value(dir));
		}

		if (!invoke_event_handler(NULL, *eh, evt)) {
			//
		}

		fs->Value_dec(evt);
	}
	return TRUE;
}
static LRESULT CALLBACK WndProcBase(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (msg) {
	case WM_LBUTTONDOWN:
		button_proc_sub(r, "button_pressed", 1, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
		break;
	case WM_LBUTTONUP:
		button_proc_sub(r, "button_released", 1, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
		break;
	case WM_RBUTTONDOWN:
		button_proc_sub(r, "button_pressed", 3, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
		break;
	case WM_RBUTTONUP:
		button_proc_sub(r, "button_released", 3, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
		break;
	case WM_MBUTTONDOWN:
		button_proc_sub(r, "button_pressed", 2, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
		break;
	case WM_MBUTTONUP:
		button_proc_sub(r, "button_released", 2, (short)LOWORD(lParam), (short)HIWORD(lParam), 0);
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
	}
	return CallWindowProc(Value_ptr(r->v[INDEX_WBASE_CALLBACK]), hWnd, msg, wParam, lParam);
}

/**
 * Widget共通のイベントハンドラを登録する
 */
void connect_widget_events(WndHandle window)
{
	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProcBase);
}

// v : FormのサブクラスでValue_new_ref済みの値を渡す
void create_form_window(Value *v, WndHandle parent, int *size)
{
	Ref *r = Value_vp(*v);
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
	r->v[INDEX_WIDGET_HANDLE] = handle_Value(window);

	// Windowに関連付けたため、+1する
	fs->Value_inc(*v);
	root_window_count++;
	SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)r);
	r->v[INDEX_WBASE_CALLBACK] = ptr_Value((void*)GetWindowLongPtr(window, GWLP_WNDPROC));
	connect_widget_events(window);

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
	WndHandle window = Value_handle(r->v[INDEX_WIDGET_HANDLE]);
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
	*vret = utf16_str_Value(wbuf);
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

////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	GraphicsHandle g;
	int x, y;
	int width, height;
} ImagePaneData;

static void set_scrollbar(HWND hWnd, ImagePaneData *ipd, int code, int vert)
{
	RECT rc;
	GetClientRect(hWnd, &rc);

	switch (code){
	case SB_LINEUP:
		if (vert) {
			if (ipd->y > 0) {
				ipd->y--;
			}
		} else {
			if (ipd->x > 0) {
				ipd->x--;
			}
		}
		return;
	case SB_LINEDOWN:
		if (vert) {
			if (ipd->y < ipd->height - rc.bottom) {
				ipd->y++;
			}
		} else {
			if (ipd->x < ipd->width - rc.right) {
				ipd->x++;
			}
		}
		return;
	case SB_PAGEUP:
		if (vert) {
			ipd->y -= rc.bottom;
			if (ipd->y < 0) {
				ipd->y = 0;
			}
		} else {
			ipd->x -= rc.right;
			if (ipd->x < 0) {
				ipd->x = 0;
			}
		}
		return;
	case SB_PAGEDOWN:
		if (vert) {
			ipd->y += rc.bottom;
			if (ipd->y > ipd->height - rc.bottom) {
				ipd->y = ipd->height - rc.bottom;
			}
		} else {
			ipd->x += rc.right;
			if (ipd->x > ipd->width - rc.right) {
				ipd->x = ipd->width - rc.right;
			}
		}
		return;
	case SB_TOP:
		if (vert) {
			ipd->y = 0;
		} else {
			ipd->x = 0;
		}
		break;
	case SB_BOTTOM:
		if (vert) {
			ipd->y = ipd->height - rc.bottom;
		} else {
			ipd->x = ipd->width - rc.right;
		}
		break;
	case SB_THUMBTRACK: {
		SCROLLINFO si;
		si.cbSize = sizeof(si);
		si.fMask = SIF_TRACKPOS;
		if (GetScrollInfo(hWnd, (vert ? SB_VERT : SB_HORZ), &si) == 0) {
			return;
		}
		if (vert) {
			ipd->y = si.nTrackPos;
		} else {
			ipd->x = si.nTrackPos;
		}
	} break;
	default:
		break;
	}
}
static void update_scrollbar(HWND hWnd, ImagePaneData *ipd)
{
	SCROLLINFO sc;
	memset(&sc, 0, sizeof(sc));
	sc.cbSize = sizeof(SCROLLINFO);
	sc.fMask = SIF_ALL;

	if (ipd->g != NULL) {
		RECT rc;
		GetWindowRect(hWnd, &rc);
		if (ipd->width <= rc.right - rc.left && ipd->height <= rc.bottom - rc.top) {
			goto NO_SCROLL_BARS;
		}

		GetClientRect(hWnd, &rc);
		if (ipd->width > rc.right) {
			if (ipd->x > ipd->width - rc.right) {
				ipd->x = ipd->width - rc.right;
			}
		} else {
			ipd->x = 0;
		}
		if (ipd->height > rc.bottom) {
			if (ipd->y > ipd->height - rc.bottom) {
				ipd->y = ipd->height - rc.bottom;
			}
		} else {
			ipd->y = 0;
		}
		sc.nMax = ipd->width;
		sc.nPage = rc.right;
		sc.nPos = ipd->x;
		SetScrollInfo(hWnd, SB_HORZ, &sc, TRUE);

		sc.nMax = ipd->height;
		sc.nPage = rc.bottom;
		sc.nPos = ipd->y;
		SetScrollInfo(hWnd, SB_VERT, &sc, TRUE);
	} else {
		goto NO_SCROLL_BARS;
	}
	return;

NO_SCROLL_BARS:
	sc.nMax = 1;
	sc.nPage = 2;
	SetScrollInfo(hWnd, SB_HORZ, &sc, TRUE);
	SetScrollInfo(hWnd, SB_VERT, &sc, TRUE);
}
static void draw_image_pane(HDC hDC, HDC hImgDC, const RECT *rc, ImagePaneData *ipd, HBRUSH hBrush)
{
	int x1, y1;
	int x2, y2;
	int width, height;

	if (ipd->width < rc->right) {
		x1 = (rc->right - ipd->width) / 2;
	} else {
		x1 = -ipd->x;
	}
	if (ipd->height < rc->bottom) {
		y1 = (rc->bottom - ipd->height) / 2;
	} else {
		y1 = -ipd->y;
	}
	if (ipd->width > rc->right - x1) {
		width = rc->right - x1;
	} else {
		width = ipd->width;
	}
	if (ipd->height > rc->bottom - y1) {
		height = rc->bottom - y1;
	} else {
		height = ipd->height;
	}
	if (x1 < 0) {
		x2 = -x1;
		x1 = 0;
	} else {
		x2 = 0;
	}
	if (y1 < 0) {
		y2 = -y1;
		y1 = 0;
	} else {
		y2 = 0;
	}

	if (x1 > 0) {
		RECT r;
		r.left = 0;
		r.right = x1;
		r.top = 0;
		r.bottom = rc->bottom;
		FillRect(hDC, &r, hBrush);
	}
	if (width < rc->right) {
		RECT r;
		r.left = x1 + width;
		r.right = rc->right;
		r.top = 0;
		r.bottom = rc->bottom;
		FillRect(hDC, &r, hBrush);
	}
	if (y1 > 0) {
		RECT r;
		r.left = x1;
		r.right = x1 + width;
		r.top = 0;
		r.bottom = y1;
		FillRect(hDC, &r, hBrush);
	}
	if (height < rc->bottom) {
		RECT r;
		r.left = x1;
		r.right = x1 + width;
		r.top = y1 + height;
		r.bottom = rc->bottom;
		FillRect(hDC, &r, hBrush);
	}
	BitBlt(hDC, x1, y1, width, height, hImgDC, x2, y2, SRCCOPY);
}
static LRESULT CALLBACK ImagePaneProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CREATE: {
		CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
		Ref *r = (Ref*)cs->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)r);
		return 0;
	}
	case WM_DESTROY: {
		Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		ImagePaneData *ipd = Value_ptr(r->v[INDEX_WIDGET_GENERAL_USE]);
		if (ipd->g != NULL) {
			DeleteObject(ipd->g);
			ipd->g = NULL;
		}
		break;
	}
	case WM_HSCROLL: {
		Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		ImagePaneData *ipd = Value_ptr(r->v[INDEX_WIDGET_GENERAL_USE]);
		set_scrollbar(hWnd, ipd, LOWORD(wParam), FALSE);
		update_scrollbar(hWnd, ipd);
		InvalidateRect(hWnd, NULL, FALSE);
		return 0;
	}
	case WM_VSCROLL: {
		Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		ImagePaneData *ipd = Value_ptr(r->v[INDEX_WIDGET_GENERAL_USE]);
		set_scrollbar(hWnd, ipd, LOWORD(wParam), TRUE);
		update_scrollbar(hWnd, ipd);
		InvalidateRect(hWnd, NULL, FALSE);
		return 0;
	}
	case WM_SIZE:
		if (!IsIconic(hWnd)){
			Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		ImagePaneData *ipd = Value_ptr(r->v[INDEX_WIDGET_GENERAL_USE]);
			update_scrollbar(hWnd, ipd);
			InvalidateRect(hWnd, NULL, FALSE);
			return 0;
		}
		break;
	case WM_PAINT: {
		Ref *r = (Ref*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
		ImagePaneData *ipd = Value_ptr(r->v[INDEX_WIDGET_GENERAL_USE]);

		PAINTSTRUCT ps;
		RECT rc;
		HDC hDC = BeginPaint(hWnd, &ps);
		HBRUSH hBrush = GetSysColorBrush(COLOR_3DFACE);

		GetClientRect(hWnd, &rc);
		if (ipd->g != NULL) {
			HDC hImgDC = CreateCompatibleDC(hDC);
			SelectObject(hImgDC, ipd->g);
			draw_image_pane(hDC, hImgDC, &rc, ipd, hBrush);
			DeleteDC(hImgDC);
		} else {
			FillRect(hDC, &rc, hBrush);
		}
		EndPaint(hWnd, &ps);
		return 0;
	}
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void init_image_pane_class()
{
	static int initialized = FALSE;

	if (!initialized) {
		WNDCLASSEXW wc;

		wc.cbSize        = sizeof(wc);
		wc.style         = 0;
		wc.lpfnWndProc   = ImagePaneProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = GetModuleHandle(NULL);
		wc.hIcon         = NULL;
		wc.hIconSm       = NULL;
		wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = NULL;
		wc.lpszMenuName  = NULL;
		wc.lpszClassName = IMAGE_PANE_CLASS;
		RegisterClassExW(&wc);

		initialized = TRUE;
	}
}

void create_image_pane_window(Value *v, WndHandle parent)
{
	Ref *r = Value_vp(*v);
	HWND window = NULL;
	ImagePaneData *ipd;
	init_image_pane_class();

	ipd = malloc(sizeof(ImagePaneData));
	memset(ipd, 0, sizeof(ImagePaneData));
	r->v[INDEX_WIDGET_GENERAL_USE] = ptr_Value(ipd);

	window = CreateWindowExW(
		0,
		IMAGE_PANE_CLASS,
		L"",
		WS_CHILD|WS_CLIPSIBLINGS|WS_VISIBLE|WS_HSCROLL|WS_VSCROLL,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		parent,
		(HMENU)ID_CHILD_PANE,
		GetModuleHandle(NULL),
		r);

	ShowWindow(window, SW_SHOW);

	r->v[INDEX_WIDGET_HANDLE] = handle_Value(window);
	fs->Value_inc(*v);
}
int image_pane_set_image_sub(WndHandle window, RefImage *img)
{
	GraphicsHandle handle = conv_image_to_graphics(img);
	if (handle == NULL) {
		return FALSE;
	}
	{
		Ref *r = (Ref*)GetWindowLongPtr(window, GWLP_USERDATA);
		ImagePaneData *ipd = Value_ptr(r->v[INDEX_WIDGET_GENERAL_USE]);
		if (ipd->g != NULL) {
			DeleteObject(ipd->g);
		}
		ipd->g = handle;
		ipd->x = 0;
		ipd->y = 0;
		ipd->width = img->width;
		ipd->height = img->height;
		update_scrollbar(window, ipd);
	}
	InvalidateRect(window, NULL, FALSE);
	return TRUE;
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
int window_message_loop(int *loop)
{
	MSG msg;

	if (GetMessageW(&msg, NULL, 0, 0) <= 0) {
		*loop = FALSE;
	} else {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
		*loop = TRUE;
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

static RefGuiHash timer_entry;

static void CALLBACK timeout_callback(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	Value *sender_v = GuiHash_get_p(&timer_entry, (const void*)id);

	if (sender_v != NULL) {
		Ref *sender_r = Value_vp(*sender_v);
		Value fn = sender_r->v[INDEX_TIMER_FN];
		Value *stk_top = fg->stk_top;
		int ret_code;
		int result = TRUE;

		fs->Value_push("v", fn);
		*fg->stk_top++ = event_object_new(sender_r);

		ret_code = fs->call_function_obj(1);
		if (fg->error != VALUE_NULL) {
			PostQuitMessage(0);
		}
		if (ret_code) {
			Value vr = fg->stk_top[-1];
			if (vr == VALUE_FALSE) {
				result = FALSE;
			}
		} else {
			result = FALSE;
		}
		// エラーの場合、stk_topが不定
		while (fg->stk_top > stk_top) {
			fs->Value_pop();
		}
		if (!result) {
			sender_r->v[INDEX_TIMER_ID] = VALUE_NULL;

			// カウンタを1減らす
			fs->Value_dec(vp_Value(sender_r));
			KillTimer(NULL, id);

			root_window_count--;
			if (root_window_count <= 0) {
				PostQuitMessage(0);
			}
		}
	} else {
		KillTimer(NULL, id);
	}
}

void native_timer_new(int millisec, Ref *r)
{
	UINT_PTR id = SetTimer(NULL, 0, millisec, timeout_callback);
	Value *v_tmp = GuiHash_add_p(&timer_entry, (const void*)id);

	*v_tmp = fs->ref_cp_Value(&r->rh);

	r->v[INDEX_TIMER_ID] = int32_Value(id);
	r->rh.nref++;
	// イベントループ監視対象なので1増やす
	root_window_count++;
}
void native_timer_remove(Ref *r)
{
	if (r->v[INDEX_TIMER_ID] != VALUE_NULL) {
		// TODO: INDEX_TIMER_IDは64bit必要
		uintptr_t timer_id = Value_integral(r->v[INDEX_TIMER_ID]);

		KillTimer(NULL, timer_id);
		// カウンタを減らす
		GuiHash_del_p(&timer_entry,(const void*)timer_id);
		r->v[INDEX_TIMER_ID] = VALUE_NULL;

		root_window_count--;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
	HANDLE file;
} FileMonitor;

int native_filemonitor_new(Ref *r, const char *path)
{
	return TRUE;
}
void native_filemonitor_remove(Ref *r)
{
	FileMonitor *fm = Value_ptr(r->v[INDEX_FILEMONITOR_STRUCT]);

	if (fm != NULL) {
		//
		free(fm);
		r->v[INDEX_FILEMONITOR_STRUCT] = VALUE_NULL;
	}
}

void native_object_remove_all()
{
}
