#include "fox.h"
#include "compat.h"
#include <windows.h>
#include <ctype.h>


#if 0
int utf8_to_utf16(wchar_t *dst_p, const char *src, int len)
{
    const wchar_t *dst = dst_p;
    const unsigned char *p = (const unsigned char*)ptr;
    const unsigned char *end;

    if (len < 0) {
        len = strlen(src) + 1;
    }
    end = p + len;

    if (dst_p != NULL) {
        while (p < end) {
            int c = *p;
            int code;

            if ((c & 0x80) == 0) {
                code = c & 0x7F;
                p += 1;
            } else if ((c & 0xE0) == 0xC0 && p + 1 < end) {
                code = ((c & 0x1F) << 6) | (p[1] & 0x3F);
                p += 2;
            } else if ((c & 0xF0) == 0xE0 && p + 2 < end) {
                code = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                p += 3;
            } else if ((c & 0xF8) == 0xF0 && p + 3 < end) {
                code = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12)  | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
                p += 4;
            } else {
                p++;
                while (p < end && (*p & 0xC0) == 0x80) {
                    p++;
                }
                code = 0xFFFD;
            }

            if (code >= 0x10000) {
                code -= 0x10000;
                *dst++ = code / 0x400 + SURROGATE_U_BEGIN;
                *dst++ = code % 0x400 + SURROGATE_L_BEGIN;
            } else {
                *dst++ = code;
            }
        }
    } else {
        while (p < end) {
            int c = *p;

            dst++;
            if ((c & 0x80) == 0) {
                p += 1;
            } else if ((c & 0xE0) == 0xC0 && p + 1 < end) {
                p += 2;
            } else if ((c & 0xF0) == 0xE0 && p + 2 < end) {
                p += 3;
            } else if ((c & 0xF8) == 0xF0 && p + 3 < end) {
                p += 4;
                if (((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) >= 0x10000) {
                    dst++;
                }
            } else {
                p++;
                while (p < end && (*p & 0xC0) == 0x80) {
                    p++;
                }
            }
        }
    }
    return dst - dst_p;
}
#endif

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

/////////////////////////////////////////////////////////////////////////////////////

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

    if (fd != -1 && mode == (O_CREAT|O_WRONLY|O_APPEND)) {
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

/////////////////////////////////////////////////////////////////////////////////////

DIR *opendir_fox(const char *dname)
{
    DIR *d = malloc(sizeof(DIR) + sizeof(WIN32_FIND_DATAW));
    wchar_t *wtmp = filename_to_utf16(dname, L"\\*");
    d->hDir = FindFirstFileW(wtmp, (WIN32_FIND_DATAW*)d->wfd);
    free(wtmp);

    if (d->hDir != INVALID_HANDLE_VALUE) {
        WIN32_FIND_DATAW *p = (WIN32_FIND_DATAW*)d->wfd;
        d->first = TRUE;
        WideCharToMultiByte(CP_UTF8, 0, p->cFileName, -1, d->ent.d_name, sizeof(d->ent.d_name), NULL, NULL);
        if ((p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            d->ent.d_type = DT_DIR;
        } else {
            d->ent.d_type = DT_REG;
        }
        return d;
    } else {
        free(d);
        return NULL;
    }
}
struct dirent *readdir_fox(DIR *d)
{
    if (d->first) {
        d->first = FALSE;
        return &d->ent;
    } else if (FindNextFileW(d->hDir, (WIN32_FIND_DATAW*)d->wfd)) {
        WIN32_FIND_DATAW *p = (WIN32_FIND_DATAW*) &d->wfd;
        WideCharToMultiByte(CP_UTF8, 0, p->cFileName, -1, d->ent.d_name, sizeof(d->ent.d_name), NULL, NULL);
        if ((p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            d->ent.d_type = DT_DIR;
        } else {
            d->ent.d_type = DT_REG;
        }
        return &d->ent;
    } else {
        return NULL;
    }
}
void closedir_fox(DIR *d)
{
    FindClose((HANDLE)d->hDir);
    free(d);
}
