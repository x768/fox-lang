#ifndef GENML_H_INCLUDED
#define GENML_H_INCLUDED

#include "markup.h"


#ifdef DEFINE_GLOBALS
#define extern
#endif

RefNode *cls_genml;
RefNode *cls_gmlargs;
RefNode *cls_gmlargsiter;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


enum {
    GMLTYPE_NONE,
    GMLTYPE_ARG,
    GMLTYPE_TEXT,
    GMLTYPE_COMMAND,
    GMLTYPE_COMMAND_OPT,
};
enum {
    PUNCT_RAW = 1,
    PUNCT_HEADING = 2,
    PUNCT_LIST = 4,
    PUNCT_TABLE = 8,
    PUNCT_RANGE_OPEN = 16,
    PUNCT_RANGE_CLOSE = 32,
};

enum {
    INDEX_GENML_GML,
    INDEX_GENML_NUM,
};
enum {
    INDEX_GENMLARGSITER_ARG,
    INDEX_GENMLARGSITER_NODE,
    INDEX_GENMLARGSITER_NODELIST,
    INDEX_GENMLARGSITER_NUM,
};

typedef struct GenMLNode {
    int16_t type;
    int16_t opt;
    const char *name;
    struct GenMLNode *next;
    struct GenMLNode *child;
} GenMLNode;

typedef struct {
    uint32_t flags;
    RefStr *comment;

    RefStr *raw;
    RefStr *range;
    RefStr *heading_list_table;
} GenMLPunct;

typedef struct {
    RefHeader rh;

    Mem mem;
    GenMLPunct *punct[128];
    RefStr *toplevel;

    GenMLNode *root;
    GenMLNode *sub;
    PtrList *headings;
} GenML;

typedef struct {
    RefHeader rh;

    GenML *gm;
    GenMLNode *node;
    Value cvt;
} GenMLArgs;

GenML *GenML_new(RefMap *rm);
int parse_genml(GenML *gm, const char *src);
int make_genml_root(Value *vret, Value cvt, GenML *gm);
int make_genml_sub(Value cvt, GenML *gm, const char *name);
int make_genml_elem(Value *vret, Value cvt, GenML *gm, GenMLNode *node);

#endif /* GENML_H_INCLUDED */
