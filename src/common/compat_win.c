#include "fox.h"
#include "compat.h"
#include <windows.h>
#include <ctype.h>

// dst_size <= len * 3
// 出力は'\0'で終端する
// 入力エラーチェックは厳密ではないが、不正なシーケンスは出力しない
int utf8_to_utf16(wchar_t *dst_p, const char *src, int len)
{
    const uint8_t *p = (const uint8_t*)src;
    const uint8_t *end;

    if (len < 0) {
        len = strlen(src);
    }
    end = p + len;

    if (dst_p != NULL) {
        wchar_t *dst = dst_p;

        while (p < end) {
            int c = *p;

            if ((c & 0x80) == 0) {
                *dst++ = c & 0x7F;
                p += 1;
            } else if ((c & 0xE0) == 0xC0 && p + 1 < end) {
                *dst++ = ((c & 0x1F) << 6) | (p[1] & 0x3F);
                p += 2;
            } else if ((c & 0xF0) == 0xE0 && p + 2 < end) {
                int code = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                if (code >= SURROGATE_L_BEGIN && code < SURROGATE_END) {
                    code = 0xFFFD;
                }
                *dst++ = code;
                p += 3;
            } else if ((c & 0xF8) == 0xF0 && p + 3 < end) {
                int code = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12)  | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
                if (code >= 0x10000) {
                    code -= 0x10000;
                    *dst++ = (code >> 10) | SURROGATE_U_BEGIN;
                    *dst++ = (code & 0x3FF) | SURROGATE_L_BEGIN;
                } else {
                    *dst++ = code;
                }
                p += 4;
            } else {
                p++;
                while (p < end && (*p & 0xC0) == 0x80) {
                    p++;
                }
                *dst++ = 0xFFFD;
            }
        }
        *dst = L'\0';
        return (int)(dst - dst_p);
    } else {
        int dst = 0;
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
                if ((c & 0x07) != 0 || (p[1] & 0x30) != 0) {
                    dst++;
                }
            } else {
                p++;
                while (p < end && (*p & 0xC0) == 0x80) {
                    p++;
                }
            }
        }
        return dst;
    }
}
// dst_size <= len * 3
// 出力は'\0'で終端する
// 入力エラーチェックは厳密ではないが、不正なシーケンスは出力しない
int utf16_to_utf8(char *dst_p, const wchar_t *src, int len)
{
    enum {
        ALT_SIZE = 3,
    };
    const char *alt = "\xEF\xBF\xBD";
    const wchar_t *p = src;
    const wchar_t *end;

    if (len < 0) {
        len = wcslen(src);
    }
    end = p + len;

    if (dst_p != NULL) {
        char *dst = dst_p;

        while (p < end) {
            int c = *p++;

            if (c < 0x7F) {
                *dst++ = c;
            } else if (c < 0x7FF) {
                *dst++ = 0xC0 | (c >> 6);
                *dst++ = 0x80 | (c & 0x3F);
            } else if (c < SURROGATE_U_BEGIN) {
                *dst++ = 0xE0 | (c >> 12);
                *dst++ = 0x80 | ((c >> 6) & 0x3F);
                *dst++ = 0x80 | (c & 0x3F);
            } else if (c < SURROGATE_L_BEGIN) {
                if (p < end && *p >= SURROGATE_L_BEGIN && *p < SURROGATE_END) {
                    int code = (((c - SURROGATE_L_BEGIN) << 10) | (*p - SURROGATE_L_BEGIN)) + 0x10000;
                    *dst++ = 0xF0 | (code >> 18);
                    *dst++ = 0x80 | ((code >> 12) & 0x3F);
                    *dst++ = 0x80 | ((code >> 6) & 0x3F);
                    *dst++ = 0x80 | (code & 0x3F);
                    p++;
                } else {
                    memcpy(dst, alt, ALT_SIZE);
                    dst += ALT_SIZE;
                }
            } else if (c < SURROGATE_END) {
                memcpy(dst, alt, ALT_SIZE);
                dst += ALT_SIZE;
            } else {
                *dst++ = 0xE0 | (c >> 12);
                *dst++ = 0x80 | ((c >> 6) & 0x3F);
                *dst++ = 0x80 | (c & 0x3F);
            }
        }
        *dst = '\0';
        return (int)(dst - dst_p);
    } else {
        int dst = 0;

        while (p < end) {
            int c = *p++;

            if (c < 0x7F) {
                dst += 1;
            } else if (c < 0x7FF) {
                dst += 2;
            } else if (c < SURROGATE_U_BEGIN) {
                dst += 3;
            } else if (c < SURROGATE_L_BEGIN) {
                if (p < end && *p >= SURROGATE_L_BEGIN && *p < SURROGATE_END) {
                    dst += 4;
                    p++;
                } else {
                    dst += ALT_SIZE;
                }
            } else if (c < SURROGATE_END) {
                dst += ALT_SIZE;
            } else {
                dst += 3;
            }
        }
        return dst;
    }
}

char *utf16_to_cstr(const wchar_t *src, int src_size)
{
    int len;
    char *dst;

    if (src_size < 0) {
        src_size = wcslen(src);
    }
    len = utf16_to_utf8(NULL, src, src_size);
    dst = malloc(len + 1);
    utf16_to_utf8(dst, src, src_size);
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
    len = utf8_to_utf16(NULL, src, src_size);
    dst = malloc((len + 1) * sizeof(wchar_t));
    utf8_to_utf16(dst, src, src_size);
    dst[len] = L'\0';
    return dst;
}
wchar_t *filename_to_utf16(const char *src, const wchar_t *opt)
{
    wchar_t *dst;
    int unc = (isalpha(src[0]) && src[1] == ':');
    int pos = (unc ? 4 : 0);
    int len = utf8_to_utf16(NULL, src, -1);
    int len2 = (opt != NULL ? wcslen(opt) : 0);

    dst = malloc((pos + len + len2 + 1) * sizeof(wchar_t));
    if (unc) {
        wcscpy(dst, L"\\\\?\\");
    }
    utf8_to_utf16(&dst[pos], src, -1);
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

int64_t get_file_size(FileHandle fh)
{
    if (fh != -1) {
        HANDLE hFile = (HANDLE)fh;
        DWORD size_h;
        DWORD size = GetFileSize(hFile, &size_h);
        return ((uint64_t)size) | ((uint64_t)size_h << 32);
    } else {
        return -1;
    }
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
        utf16_to_utf8(d->ent.d_name, p->cFileName, -1);
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
        utf16_to_utf8(d->ent.d_name, p->cFileName, -1);
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
