#ifndef _M_XML_H_
#define _M_XML_H_

#include "fox.h"

enum {
    INDEX_ELEM_NAME,
    INDEX_ELEM_ATTR,
    INDEX_ELEM_CHILDREN,
    INDEX_ELEM_NUM,
};
enum {
    INDEX_DECL_NAME,
    INDEX_DECL_CONTENT,
    INDEX_DECL_NUM,
};

typedef struct {
    int (*resolve_entity)(const char *p, const char *end);
    int (*is_valid_elem_name)(const char *s_p, int s_size);
} XMLStatic;

#endif /* _M_UNICODE_H_ */
