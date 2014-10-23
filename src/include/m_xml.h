#ifndef _M_XML_H_
#define _M_XML_H_

#include "fox.h"

enum {
    INDEX_ELEM_NAME,
    INDEX_ELEM_ATTR,
    INDEX_ELEM_CHILDREN,
    INDEX_ELEM_NUM,
};

typedef struct {
    int (*resolve_entity)(const char *p, const char *end);
} XMLStatic;

#endif /* _M_UNICODE_H_ */
