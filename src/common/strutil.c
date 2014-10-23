#include "fox.h"
#include <string.h>

const char *fox_ctype_flags = 
    "\x40\x40\x40\x40\x40\x40\x40\x40\x40\xc0\xc0\x40\x40\x40\x40\x40"
    "\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40\x40"
    "\xc0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x32\x32\x32\x32\x32\x32\x32\x32\x32\x32\x00\x00\x00\x00\x00\x00"
    "\x00\x27\x27\x27\x27\x27\x27\x07\x07\x07\x07\x07\x07\x07\x07\x07"
    "\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x07\x00\x00\x00\x00\x03"
    "\x00\x2b\x2b\x2b\x2b\x2b\x2b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
    "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    ;


int str_eq(const char *s_p, int s_size, const char *t_p, int t_size)
{
    if (s_size < 0) {
        s_size = strlen(s_p);
    }
    if (t_size < 0) {
        t_size = strlen(t_p);
    }
    return s_size == t_size && memcmp(s_p, t_p, s_size) == 0;
}
int str_eqi(const char *s_p, int s_size, const char *t_p, int t_size)
{
    int i;

    if (s_size < 0) {
        s_size = strlen(s_p);
    }
    if (t_size < 0) {
        t_size = strlen(t_p);
    }
    if (s_size != t_size) {
        return FALSE;
    }

    for (i = 0; i < s_size; i++) {
        int c1 = s_p[i];
        int c2 = t_p[i];
        if (tolower_fox(c1) != tolower_fox(c2)) {
            return FALSE;
        }
    }
    return TRUE;
}

int stricmp_fox(const char *s1, const char *s2)
{
    while (*s1 != '\0') {
        int c1 = tolower_fox(*s1);
        int c2 = tolower_fox(*s2);
        if (c1 != c2) {
            return c1 - c2;
        }
    }
    return (*s2 != '\0' ? -1 : 0);
}
int str_has0(const char *p, int size)
{
    int i;
    for (i = 0; i < size; i++) {
        if (p[i] == '\0') {
            return TRUE;
        }
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////////////

Str Str_new(const char *p, int len)
{
    Str s;
    s.p = p;
    if (len < 0) {
        if (p != NULL) {
            len = strlen(p);
        } else {
            len = 0;
        }
    }
    s.size = len;
    return s;
}
int Str_eq_p(Str s, const char *p)
{
    int len = strlen(p);
    return s.size == len && memcmp(s.p, p, len) == 0;
}


////////////////////////////////////////////////////////////////////////////

int refstr_eq(RefStr *r1, RefStr *r2)
{
    return r1->size == r2->size && memcmp(r1->c, r2->c, r1->size) == 0;
}
int refstr_cmp(RefStr *r1, RefStr *r2)
{
    int len = r1->size < r2->size ? r1->size : r2->size;
    int cmp = memcmp(r1->c, r2->c, len);
    if (cmp == 0) {
        cmp = r1->size - r2->size;
    }
    return cmp;
}
