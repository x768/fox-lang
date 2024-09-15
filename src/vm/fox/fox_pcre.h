#ifndef FOX_PCRE_H_INCLUDED
#define FOX_PCRE_H_INCLUDED

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

pcre2_code *regex_compile(const char *s, int len, uint32_t options, int *errcode);
void throw_pcre_error(int errcode);

#endif /* FOX_PCRE_H_INCLUDED */
