#ifndef GUI_WIN_H_
#define GUI_WIN_H_

#define ID_CHILD_PANE 100

extern HDC hDisplayDC;

wchar_t *cstr_to_utf16(const char *src, int src_size);
Value utf16_str_Value(const wchar_t *src);
Value utf16_file_Value(const wchar_t *src);
HBITMAP duplicate_bitmap(HBITMAP hSrcBmp);

#endif /* GUI_WIN_H_ */
