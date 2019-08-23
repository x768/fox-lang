#include "genml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int invoke_func(Value *vret, Value cvt, GenML *gm, const char *name, GenMLNode *node, int opt)
{
    RefStr *rs_name = fs->intern(name, -1);
    GenMLArgs *arg = fs->buf_new(cls_gmlargs, sizeof(GenMLArgs));

    arg->gm = gm;
    arg->node = node;
    arg->cvt = cvt;

    if (opt > 0) {
        fs->Value_push("vrd", cvt, arg, opt);
        if (!fs->call_member_func(rs_name, 2, TRUE)) {
            arg->node = NULL;
            fs->unref(vp_Value(arg));
            return FALSE;
        }
    } else {
        fs->Value_push("vr", cvt, arg);
        if (!fs->call_member_func(rs_name, 1, TRUE)) {
            arg->node = NULL;
            fs->unref(vp_Value(arg));
            return FALSE;
        }
    }
    arg->node = NULL;
    fs->unref(vp_Value(arg));

    fg->stk_top--;
    *vret = *fg->stk_top;
    return TRUE;
}

int make_genml_elem(Value *vret, Value cvt, GenML *gm, GenMLNode *node)
{
    switch (node->type) {
    case GMLTYPE_COMMAND:
    case GMLTYPE_COMMAND_OPT: {
        int opt = 0;
        if (node->type == GMLTYPE_COMMAND_OPT) {
            opt = node->opt;
        }
        if (!invoke_func(vret, cvt, gm, node->name, node->child, opt)) {
            return FALSE;
        }
        break;
    }
    case GMLTYPE_TEXT:
        *vret = fs->cstr_Value(fs->cls_str, node->name, -1);
        break;
    case GMLTYPE_ARG: {
        RefArray *ra = fs->refarray_new(0);
        GenMLNode *arg = node->child;
        while (arg != NULL) {
            Value v = VALUE_NULL;
            if (!make_genml_elem(&v, cvt, gm, arg)) {
                fs->unref(vp_Value(ra));
                return FALSE;
            }
            if (v != VALUE_NULL) {
                *fs->refarray_push(ra) = v;
            }
            arg = arg->next;
        }
        *vret = vp_Value(ra);
        break;
    }
    case GMLTYPE_NONE:
        *vret = VALUE_NULL;
        break;
    default:
        fs->fatal_errorf("GenML:make_genml_cmd unknown node type (%d)", node->type);
        return FALSE;
    }
    return TRUE;
}

int make_genml_root(Value *vret, Value cvt, GenML *gm)
{
    return invoke_func(vret, cvt, gm, "root", gm->root, 0);
}
int make_genml_sub(Value cvt, GenML *gm, const char *name)
{
    GenMLNode *node = gm->sub;
    int found = FALSE;

    while (node != NULL) {
        if (strcmp(node->name, name) == 0) {
            GenMLNode *arg;
            found = TRUE;
            for (arg = node->child; arg != NULL; arg = arg->next) {
                Value v;
                if (!make_genml_elem(&v, cvt, gm, arg)) {
                    return FALSE;
                }
                fs->unref(v);
            }
        }
        node = node->next;
    }
    if (!found) {
        fs->throw_errorf(mod_markup, "GenMLError", "%s not found", name);
        return FALSE;
    }

    return TRUE;
}
