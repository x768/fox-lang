#include "fox.h"
#include "compat.h"
#include <windows.h>
#include <ctype.h>


char *utf16to8(const wchar_t *src)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, src, -1, NULL, 0, NULL, NULL);
	char *dst = malloc(len + 1);
	WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, len + 1, NULL, NULL);
	dst[len] = '\0';
	return dst;
}
wchar_t *cstr_to_utf16(const char *src, int src_size)
{
	int len;
	wchar_t *dst;

	if (src_size < 0) {
		src_size = strlen(src);
	}
	len = MultiByteToWideChar(CP_UTF8, 0, src, src_size, NULL, 0);
	dst = malloc((len + 1) * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, src, src_size, dst, len + 1);
	dst[len] = L'\0';
	return dst;
}
wchar_t *filename_to_utf16(const char *src, const wchar_t *opt)
{
	wchar_t *dst;
	int unc = (isalpha(src[0]) && src[1] == ':');
	int pos = (unc ? 4 : 0);
	int len = MultiByteToWideChar(CP_UTF8, 0, src, strlen(src), NULL, 0);
	int len2 = (opt != NULL ? wcslen(opt) : 0);

	dst = malloc((pos + len + len2 + 1) * sizeof(wchar_t));
	if (unc) {
		wcscpy(dst, L"\\\\?\\");
	}
	MultiByteToWideChar(CP_UTF8, 0, src, -1, &dst[pos], len + 1);
	if (opt != NULL) {
		wcscpy(&dst[pos + len], opt);
	} else {
		dst[pos + len] = L'\0';
	}
	return dst;
}

FileHandle open_fox(const char *fname, int mode, int dummy)
{
	FileHandle fd;
	DWORD dwDesiredAccess;
	DWORD dwShareMode;
	DWORD dwCreationDisposition;
	wchar_t *wtmp;

	switch (mode) {
	case O_RDONLY:
		dwDesiredAccess = GENERIC_READ;
		dwShareMode = FILE_SHARE_READ|FILE_SHARE_WRITE;
		dwCreationDisposition = OPEN_EXISTING;
		break;
	case O_CREAT|O_WRONLY|O_TRUNC:
		dwDesiredAccess = GENERIC_WRITE;
		dwShareMode = 0;
		dwCreationDisposition = CREATE_ALWAYS;
		break;
	case O_CREAT|O_WRONLY|O_APPEND:
		dwDesiredAccess = GENERIC_WRITE;
		dwShareMode = 0;
		dwCreationDisposition = OPEN_ALWAYS;
		break;
	default:
		return -1;
	}
	wtmp = filename_to_utf16(fname, NULL);
	fd = (FileHandle)CreateFileW(wtmp, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);

	if (mode == (O_CREAT|O_WRONLY|O_APPEND)) {
		SetFilePointer((HANDLE)fd, 0, NULL, FILE_END);
	}
	free(wtmp);

	return fd;
}
int close_fox(FileHandle fd)
{
	CloseHandle((HANDLE)fd);
	return 0;
}
int read_fox(FileHandle fd, char *buf, int size)
{
	DWORD read_size;
	if (ReadFile((HANDLE)fd, buf, size, &read_size, NULL) != 0) {
		return read_size;
	} else {
		return -1;
	}
}
int write_fox(FileHandle fd, const char *buf, int size)
{
	DWORD write_size;
	if (WriteFile((HANDLE)fd, buf, size, &write_size, NULL) != 0) {
		return write_size;
	} else {
		return -1;
	}
}
int64_t seek_fox(FileHandle fd, int64_t offset, int whence)
{
	LARGE_INTEGER i64;
	i64.QuadPart = offset;
	SetFilePointerEx((HANDLE)fd, i64, &i64, whence);
	return i64.QuadPart;
}

