#ifndef M_XML_H_INCLUDED
#define M_XML_H_INCLUDED

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

#endif /* M_XML_H_INCLUDED */
