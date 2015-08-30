#include "fox.h"
#include <string.h>

const char *fox_ctype_flags = 
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\xc0\xc0\x40\x40\x40\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
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

////////////////////////////////////////////////////////////////////////////

int refstr_eq(RefStr *r1, RefStr *r2)
{
    if (r1 == NULL || r2 == NULL) {
        return r1 == r2;
    }
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

////////////////////////////////////////////////////////////////////////////

uint16_t ptr_read_uint16(const char *p)
{
    return ((uint8_t)p[0] << 8) | ((uint8_t)p[1]);
}
uint32_t ptr_read_uint32(const char *p)
{
    return ((uint8_t)p[0] << 24) | ((uint8_t)p[1] << 16) | ((uint8_t)p[2] << 8) | ((uint8_t)p[3]);
}

void ptr_write_uint16(char *p, uint32_t val)
{
    p[0] = val >> 8;
    p[1] = val & 0xFF;
}
void ptr_write_uint32(char *p, uint32_t val)
{
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
}

