#include "fox_parse.h"
#include <stdarg.h>


#if 0
void show_stack(Engine *e)
{
    Value *v = e->stk_top - 1;

    while (v >= e->stk_base) {
        const Node *type = Value_type(*v);
        if (type == fs->cls_int) {
            fprintf(stderr, "type:int, value=%d\n", Value_int(*v));
        } else if (type == fs->cls_str) {
            Str s = Value_str(*v);
            fprintf(stderr, "type:str, value=%.*s", s.size, s.p);
        } else {
            const Str *s = type->name;
            fprintf(stderr, "type:%.*s\n", s->size, s->p);
        }
        v--;
    }
}
int show_code(OpCode *p)
{
    const Str *s;

    switch (p->type) {
    case OP_RETURN:
        fprintf(stderr, "return\n");
        return 1;
    case OP_RETURN_VAL:
        fprintf(stderr, "return val\n");
        return 1;
    case OP_RETURN_THRU:
        fprintf(stderr, "return this\n");
        return 1;
    case OP_POP:
        fprintf(stderr, "pop %d\n", p->s);
        return 1;
    case OP_LOCAL:
        fprintf(stderr, "local %d\n", p->s);
        return 1;
    case OP_LITERAL_P: {
        Value *v = p->op[1].p;
        if (v == NULL) {
            fprintf(stderr, "literal null (%d)\n", p->op[0].i);
        } else if (IS_CONST_U(*v)) {
            fprintf(stderr, "literal [CONST_U] (%d)\n", p->op[0].i);
        } else {
            const Str *type = Value_type(*v)->name;
            fprintf(stderr, "literal %.*s (%d)\n", type->size, type->p, p->op[0].i);
        }
        return 3;
    }
    case OP_LITERAL: {
        Value *v = (Value*) p->op;
        const Str *type = Value_type(*v)->name;
        fprintf(stderr, "literal %.*s (%d)\n", type->size, type->p, p->op[0].i);

        return 3;
    }
    case OP_FUNC: {
        Node *node = p->op[0].p;
        fprintf(stderr, "func %x (%p)\n", node->type, node);
        return 1;
    }
    case OP_CLASS: {
        const Node *klass = p->op[0].p;
        fprintf(stderr, "class %.*s\n", klass->name->size, klass->name->p);
        return 1;
    }
    case OP_MODULE:
        fprintf(stderr, "module %p\n", p->op[0].p);
        return 1;
    case OP_PUSH_CATCH:
        fprintf(stderr, "push_catch %d,%d\n", p->op[0].i, p->op[1].i);
        return 1;
    case OP_CATCH_JMP: {
        const Node *t = p->op[1].p;
        if (t == NULL) {
            fprintf(stderr, "catch_jmp null, %08x\n", p->op[0].i);
        } else {
            fprintf(stderr, "catch_jmp %.*s, %d\n", t->name->size, t->name->p, p->op[0].i);
        }
        return 2;
    }
    case OP_RANGE_NEW:
        fprintf(stderr, "range_new (%d)\n", p->op[0].i);
        return 1;
    case OP_CALL:
        fprintf(stderr, "call argc..%d (%d)\n", p->s, p->op[0].i);
        return 1;
    case OP_CALL_ITER:
        fprintf(stderr, "call_iter (%d)\n", p->op[0].i);
        return 1;
    case OP_CALL_IN:
        fprintf(stderr, "call_in (%d)\n", p->op[0].i);
        return 1;
    case OP_CALL_M:
        s = p->op[1].p;
        fprintf(stderr, "call_m argc=%d name=%.*s (%d)\n", p->s, s->size, s->p, p->op[0].i);
        return 1;
    case OP_CALL_INIT:
        s = p->op[1].p;
        fprintf(stderr, "call_init argc=%d name=%.*s (%d)\n", p->s, s->size, s->p, p->op[0].i);
        return 1;
    case OP_MEMB:
        s = p->op[1].p;
        fprintf(stderr, "memb name=%.*s (%d)\n", s->size, s->p, p->op[0].i);
        return 3;
    case OP_JMP:
        fprintf(stderr, "jmp %d\n", p->op[0].i);
        return 2;
    case OP_EQUAL:
        fprintf(stderr, "%s (%d)\n", (p->s == T_EQ ? "==" : "!="), p->op[0].i);
        return 1;
    case OP_NOT:
        fprintf(stderr, "not\n");
        return 2;
    case OP_JIF:
        fprintf(stderr, "jif %d\n", p->op[0].i);
        return 2;
    case OP_JIF_N:
        fprintf(stderr, "jif_n %d\n", p->op[0].i);
        return 2;
    case OP_IFP:
        fprintf(stderr, "ifp %d\n", p->op[0].i);
        return 2;
    case OP_IFP_N:
        fprintf(stderr, "ifp_n %d\n", p->op[0].i);
        return 2;
    case OP_NONE:
        fprintf(stderr, "none\n");
        return 0;
    default:
        fprintf(stderr, "unknown %d\n", p->type);
        return 0;
    }
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int call_dispose(Value v, RefNode *type)
{
    // sub -> superの順に実行
    while (type != fs->cls_obj) {
        RefNode *dispose = Hash_get_p(&type->u.c.h, fs->str_dtor);

        if (dispose != NULL && (dispose->type == NODE_FUNC || dispose->type == NODE_FUNC_N)) {
            RefHeader *rf = Value_ref_header(v);
            *fg->stk_top++ = v;
            rf->nref += 2;      // 再帰的にdisposeが発生しないように
            if (!call_function(dispose, 0)) {
                // デストラクタで例外を吐くことはできないので、即終了
                fatal_errorf("Cannot throw in %n:_dispose (%n)", type, Value_type(fg->error));
            }

            // 戻り値を破棄
            Value_pop();
            rf->nref--;    // dispose抑止区間終了
        }
        type = type->u.c.super;
    }
    return TRUE;
}
/**
 * デストラクタ呼び出し後に解放する
 */
void Value_release_ref(Value v)
{
    Ref *r = (Ref*)(uintptr_t)v;
    RefNode *type = r->rh.type;

    if (type != NULL) {
        call_dispose(v, type);
    }
    if (r->rh.nref == 0) {  // 参照カウンタが変化しているかもしれない
        int i;
        int n_memb = r->rh.n_memb;
        for (i = 0; i < n_memb; i++) {
            unref(r->v[i]);
        }
        // 弱参照を削除
        if (r->rh.weak_ref != NULL) {
            Ref *r2 = r->rh.weak_ref;
            r2->v[1] = VALUE_FALSE;
            r->rh.weak_ref = NULL;

            if (r2->rh.nref > 0 && --r2->rh.nref == 0) {
                free(r2);
            }
        }
        free(r);
        fv->heap_count--;
    }
}

void dispose_opcode(RefNode *func)
{
    OpCode *code = func->u.f.u.op;
    int pc = 0;

    if (code == NULL) {
        return;
    }

    for (;;) {
        OpCode *p = &code[pc];
        if (p->type == OP_NONE) {
            break;
        } else if (p->type == OP_LITERAL) {
            unref(p->op[0]);
            pc += 2;
        } else if (p->type > OP_SIZE_3) {
            pc += 3;
        } else if (p->type > OP_SIZE_2) {
            pc += 2;
        } else {
            pc += 1;
        }
    }
//  free(code);
    func->u.f.u.op = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * メンバ探索を行う
 * _missingの探索は行わない
 */
RefNode *search_member(Value v, RefNode *klass, RefStr *name)
{
    RefNode *memb = NULL;

    if (klass == fs->cls_class) {
        // ex) Int.parse()
        // コンストラクタか定数のみ
        RefNode *klass2 = Value_vp(v);
        memb = Hash_get_p(&klass2->u.c.h, name);
        if (memb != NULL) {
            if ((memb->type & NODEMASK_MEMBER_S) != 0) {
                return memb;
            } else {
                return NULL;
            }
        }
    } else if (klass == fs->cls_module) {
        // ex) module.func()
        RefNode *klass2 = Value_vp(v);
        memb = Hash_get_p(&klass2->u.c.h, name);
        if (memb != NULL) {
            return memb;
        }
    }

    memb = Hash_get_p(&klass->u.c.h, name);
    if (memb != NULL && (memb->type & NODEMASK_MEMBER) != 0) {
        return memb;
    } else {
        return NULL;
    }
}

/**
 * メンバ変数を取り出す
 * 見つからない場合は_missingを呼び出す
 */
int call_property(RefStr *name)
{
    Value *v = fg->stk_top - 1;
    RefNode *klass = Value_type(*v);
    RefNode *memb = search_member(*v, klass, name);

    if (memb != NULL) {
        switch (memb->type) {
        case NODE_CONST_U:
        case NODE_CONST_U_N: {
            *fg->stk_top++ = VALUE_NULL;
            if (!call_function(memb, 0)) {
                return FALSE;
            }
            fg->stk_top--;
            memb->type = NODE_CONST;
            memb->u.k.val = *fg->stk_top;
            *v = Value_cp(memb->u.k.val);
            break;
        }
        case NODE_CONST:
            unref(*v);
            *v = Value_cp(memb->u.k.val);
            break;
        case NODE_FUNC:
        case NODE_FUNC_N:
            if ((memb->opt & NODEOPT_PROPERTY) != 0){
                return call_function(memb, 0);
            } else {
                // 内部変数なしのFunctionオブジェクトを生成
                Ref *r = ref_new_n(fs->cls_fn, INDEX_FUNC_LOCAL);
                r->v[INDEX_FUNC_FN] = Value_cp(vp_Value(memb));
                r->v[INDEX_FUNC_THIS] = *v;
                r->v[INDEX_FUNC_N_LOCAL] = int32_Value(0);
                *v = vp_Value(r);
            }
            break;
        case NODE_NEW:
        case NODE_NEW_N:
        case NODE_CLASS:
            *v = vp_Value(memb);
            break;
        }
    } else {
        // 見つからなかった場合は __missingを呼び出す
        memb = Hash_get_p(&klass->u.c.h, fs->str_missing);
        if (memb != NULL) {
            if (memb->type != NODE_FUNC && memb->type != NODE_FUNC_N) {
                throw_errorf(fs->mod_lang, "TypeError", "__missing must be function but not");
                return FALSE;
            }
            Value_push("r", name);
            if (!call_function(memb, 1)) {
                return FALSE;
            }
            return TRUE;
        } else {
            throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, (klass == fs->cls_class ? Value_vp(*v) : klass), name);
            return FALSE;
        }
    }

    return TRUE;
}
/**
 * メンバ関数を呼び出す
 * raise_exception == FALSEの場合は、メンバが見つからなくても例外を投げない
 * (メンバ関数中で例外が発生した場合は伝える)
 */
int call_member_func(RefStr *name, int argc, int raise_exception)
{
    Value *v = fg->stk_top - argc - 1;
    RefNode *klass = Value_type(*v);
    RefNode *memb;

CLASS_NEW:
    if (klass == fs->cls_class) {
        // コンストラクタを探す
        RefNode *klass2 = Value_vp(*v);
        memb = Hash_get_p(&klass2->u.c.h, name);

        // 見つかれば実行
        if (memb != NULL) {
            switch (memb->type) {
            case NODE_NEW:
            case NODE_NEW_N:
                return call_function(memb, argc);
            default:
                // 見つからない
                break;
            }
        }
    }
    if (klass == fs->cls_module) {
        // メンバを探す
        RefNode *mod = Value_vp(*v);
        memb = Hash_get_p(&mod->u.c.h, name);
        if (memb == NULL) {
            if (raise_exception) {
                throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, mod, name);
                return FALSE;
            } else {
                return TRUE;
            }
        }
        // メンバがクラスだった場合、コンストラクタ呼び出しのお膳立てをする
        if (memb->type == NODE_CLASS) {
            klass = fs->cls_class;
            *v = vp_Value(memb);
            name = fs->str_new;
            goto CLASS_NEW;
        }
    } else {
        memb = Hash_get_p(&klass->u.c.h, name);
        if (memb == NULL) {
            if (name->c[0] != '_') {
                int prop = (name->c[name->size - 1] == '=');

                // _method_missing / _missing_setを探す
                memb = Hash_get_p(&klass->u.c.h, prop ? fs->str_missing_set : fs->str_method_missing);
                if (memb != NULL) {
                    // 引数の先頭に名前を挿入
                    RefStr *name2 = prop ? intern(name->c, name->size - 1) : name;
                    Value *arg_base = fg->stk_top - argc;
                    memmove(arg_base + 1, arg_base, argc * sizeof(Value));
                    *arg_base = (Value)(uintptr_t)name2;
                    fg->stk_top++;
                    argc++;
                }
            }
            if (memb == NULL) {
                if (raise_exception) {
                    throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, (klass == fs->cls_class ? Value_vp(*v) : klass), name);
                    return FALSE;
                } else {
                    return TRUE;
                }
            }
        }
    }

    switch (memb->type) {
    case NODE_FUNC:
    case NODE_FUNC_N:
        if ((memb->opt & NODEOPT_PROPERTY) != 0){
            throw_errorf(fs->mod_lang, "TypeError", "%r is property", name);
            return FALSE;
        }
        return call_function(memb, argc);
    case NODE_CLASS: {
        RefNode *construct = Hash_get_p(&memb->u.c.h, fs->str_new);
        if (construct != NULL) {
            return call_function(construct, argc);
        } else {
            throw_errorf(fs->mod_lang, "NameError", "%n has no constructor named %r", klass, name);
            return FALSE;
        }
    }
    default:
        throw_errorf(fs->mod_lang, "NameError", "%r is not a member function", name);
        return FALSE;
    }
}

/**
 * superのコンストラクタを呼び出す
 * オブジェクト生成は行わない
 */
static int call_super_constructor(RefStr *name, int argc)
{
    Value v = fg->stk_top[-argc - 1];
    RefHeader *rh = Value_ref_header(v);
    RefNode *base_klass = Value_type(v);
    RefNode *klass = base_klass->u.c.super;
    RefNode *fn;

    if (klass == fs->cls_obj) {
        if (argc > 0) {
            throw_errorf(fs->mod_lang, "ArgumentError", "0 arguments excepted (%d given)", argc);
            return FALSE;
        }
        return TRUE;
    }

    fn = Hash_get_p(&klass->u.c.h, name);
    if (fn == NULL || (fn->type != NODE_NEW && fn->type != NODE_NEW_N)) {
        throw_errorf(fs->mod_lang, "NameError", "%n has no constructor named %r", klass, name);
        return FALSE;
    }

    rh->type = klass;
    if (!call_function(fn, argc)) {
        rh->type = base_klass;
        return FALSE;
    }
    rh->type = base_klass;

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////////////////////

int invoke_code(RefNode *func, int pc)
{
    OpCode *code = func->u.f.u.op;
    OpCode *p;

    fv->n_callfunc++;
    if (fv->n_callfunc > MAX_CALLFUNC_NUM || fg->stk_top + func->u.f.max_stack > fg->stk_max) {
        while (fg->stk_top > fg->stk_base + 1) {
            Value_pop();
        }
        if (fv->n_callfunc > MAX_CALLFUNC_NUM) {
            throw_errorf(fs->mod_lang, "StackOverflowError", "Too many function calls (%d)", MAX_CALLFUNC_NUM);
        } else {
            throw_errorf(fs->mod_lang, "StackOverflowError", "Stack overflow");
        }
        add_stack_trace(NULL, func, func->defined_line);
        fv->n_callfunc--;
        return FALSE;
    }

NORMAL:
    for (;;) {
        p = &code[pc];

        switch (p->type) {
        case OP_INIT_GENR: {  // Generatorインスタンスを返す
            Ref *r = ref_new_n(fv->cls_generator, func->u.f.max_stack + INDEX_GENERATOR_LOCAL);
            Value v = vp_Value(r);

            r->v[INDEX_GENERATOR_PC] = int32_Value(1);
            r->v[INDEX_GENERATOR_NSTACK] = int32_Value(fg->stk_top - fg->stk_base);
            r->v[INDEX_GENERATOR_FUNC] = Value_cp(vp_Value(func));
            memcpy(&r->v[INDEX_GENERATOR_LOCAL], fg->stk_base, (fg->stk_top - fg->stk_base) * sizeof(Value));
            fg->stk_top = fg->stk_base + 1;
            *fg->stk_base = v;
            fv->n_callfunc--;

            return TRUE;
        }
        case OP_YIELD_VAL: { // yield value
            // スタックの値を退避
            Ref *r = Value_ref(fg->stk_base[-1]);
            fg->stk_top--;
            r->v[INDEX_GENERATOR_PC] = int32_Value(pc + 1);
            r->v[INDEX_GENERATOR_NSTACK] = int32_Value(fg->stk_top - fg->stk_base);
            memcpy(&r->v[INDEX_GENERATOR_LOCAL], fg->stk_base, (fg->stk_top - fg->stk_base) * sizeof(Value));
            *fg->stk_base = *fg->stk_top;
            fg->stk_top = fg->stk_base + 1;
            fv->n_callfunc--;
            return TRUE;
        }
        case OP_RETURN: // return
        case OP_RETURN_VAL: // return value
            unref(*fg->stk_base);

            if (p->type == OP_RETURN_VAL) {
                fg->stk_top--;
                *fg->stk_base = *fg->stk_top;
            } else {
                *fg->stk_base = VALUE_NULL;
            }
            // fall through
        case OP_RETURN_THRU: // thisをそのまま返す
            while (fg->stk_top > fg->stk_base + 1) {
                Value *v = fg->stk_top - 1;

                if (Value_isref(*v)) {
                    RefHeader *r = Value_ref_header(*v);
                    if (r->nref > 0 && --r->nref == 0) {
                        Value_release_ref(*v);
                    }
                }
                fg->stk_top = v;
            }
            fv->n_callfunc--;
            return TRUE;
        case OP_SUSPEND:
            return TRUE;
        case OP_THROW: {
            Value v;
            RefNode *type;

            if (fg->error != VALUE_NULL) {
                unref(fg->error);
                fg->error = VALUE_NULL;
            }
            v = fg->stk_top[-1];

            type = Value_type(v);
            if (!is_subclass(type, fs->cls_error) && !is_subclass(type, fs->cls_uncaught)) {
                throw_errorf(fs->mod_lang, "TypeError", "Error or UncaughtError required but %n", type);
                goto THROW;
            }
            fg->error = v;
            fg->stk_top--;
            goto THROW;
        }

        case OP_END:
            // catch/finally節から戻る
            fv->n_callfunc--;
            return TRUE;
        case OP_FUNC:
        case OP_CLASS:
        case OP_MODULE:
        case OP_LITERAL:   // リテラル
            *fg->stk_top++ = Value_cp(p->op[0]);
            pc += 2;
            break;
        case OP_LITERAL_P: { // 定数
            RefNode *node = Value_vp(p->op[1]);
            if (node->type != NODE_CONST) {
                *fg->stk_top++ = VALUE_NULL;
                if (!call_function(node, 0)) {
                    goto THROW;
                }
                fg->stk_top--;
                node->type = NODE_CONST;
                node->u.k.val = *fg->stk_top;
                if (Value_isref(node->u.k.val)) {
                    RefHeader *rh = Value_vp(node->u.k.val);
                    // メンバ変数かデストラクタを持っているオブジェクトだけを追加
                    if (rh->n_memb > 0 || Hash_get_p(&rh->type->u.c.h, fs->str_dtor) != NULL) {
                        PtrList_add_p(&fv->const_refs, rh, &fg->st_mem);
                    }
                }
            }
            *fg->stk_top++ = Value_cp(node->u.k.val);
            pc += 3;
            break;
        }
        case OP_NEW_REF: {
            Value *v = fg->stk_base;
            RefHeader *rh = Value_ref_header(*v);
            if (rh->type == fs->cls_fn || rh->type == fs->cls_class) {
                *v = vp_Value(ref_new(Value_vp(p->op[0])));
            }
            pc += 2;
            break;
        }
        case OP_NEW_FN: {
            Ref *r = ref_new_n(fs->cls_fn, INDEX_FUNC_LOCAL + p->s);

            r->v[INDEX_FUNC_THIS] = Value_cp(fg->stk_base[0]);
            r->v[INDEX_FUNC_FN] = Value_cp(p->op[0]);
            r->v[INDEX_FUNC_N_LOCAL] = int32_Value(p->s);
            *fg->stk_top++ = vp_Value(r);
            pc += 2;
            break;
        }
        case OP_LOCAL_FN: {  // ローカル変数をstktopの関数オブジェクトにコピー
            Value v = fg->stk_top[-1];
            Ref *r = Value_ref(v);
            int idx = (intptr_t)p->op[0];
            r->v[INDEX_FUNC_LOCAL + idx] = Value_cp(fg->stk_base[p->s]);
            pc += 2;
            break;
        }
        case OP_PUSH_CATCH:
            *fg->stk_top++ = (p->op[1] << 32) | (p->op[0] << 2) | 3ULL;
            pc += 3;
            break;

        case OP_RANGE_NEW: {
            Value *v = fg->stk_top;
            v[0] = v[-1];
            v[-1] = v[-2];
            v[-2] = VALUE_NULL;
            fg->stk_top++;
            if (p->s) {
                *fg->stk_top++ = VALUE_TRUE;
                if (!call_function(fv->func_range_new, 3)) {
                    goto THROW;
                }
            } else {
                if (!call_function(fv->func_range_new, 2)) {
                    goto THROW;
                }
            }

            pc += 2;
            break;
        }
        case OP_DUP: {
            int i;
            int n = p->s;
            Value *v = fg->stk_top;

            memcpy(v, v - n, sizeof(Value) * n);
            for (i = 0; i < n; i++) {
                Value v2 = v[i];
                if (Value_isref(v2)) {
                    RefHeader *rh = Value_ref_header(v2);
                    if (rh->nref > 0) {
                        rh->nref++;
                    }
                }
            }
            fg->stk_top += n;

            pc += 1;
            break;
        }

        case OP_GET_LOCAL:  // ローカル変数・引数をstktopに
        case OP_GET_LOCAL_V:
            *fg->stk_top++ = Value_cp(fg->stk_base[p->s]);
            pc += 1;
            break;
        case OP_SET_LOCAL: {
            Value *v = fg->stk_base + p->s;
            unref(*v);
            fg->stk_top--;
            *v = *fg->stk_top;
            pc += 1;
            break;
        }
        case OP_GET_PROP:  // プロパティ取得
            if (!call_property(Value_vp(p->op[1]))) {
                goto THROW;
            }
            pc += 3;
            break;
        case OP_GET_FIELD: {
            RefNode *klass = Value_vp(p->op[0]);
            Ref *r = Value_ref(*fg->stk_base);
            *fg->stk_top++ = Value_cp(r->v[p->s + klass->u.c.n_offset]);
            pc += 2;
            break;
        }
        case OP_SET_FIELD: {
            RefNode *klass = Value_vp(p->op[0]);
            Ref *r = Value_ref(*fg->stk_base);
            Value *v = &r->v[p->s + klass->u.c.n_offset];
            unref(*v);
            fg->stk_top--;
            *v = *fg->stk_top;
            pc += 2;
            break;
        }
        case OP_CALL_M:    // メソッド呼び出し
            if (!call_member_func(Value_vp(p->op[1]), p->s, TRUE)) {
                goto THROW;
            }
            pc += 3;
            break;
        case OP_CALL_M_POP:  // メソッド呼び出し
            if (!call_member_func(Value_vp(p->op[1]), p->s, TRUE)) {
                goto THROW;
            }
            Value_pop();
            pc += 3;
            break;
        case OP_CALL_NEXT: {  // for文でnext呼び出し
            Value *v = fg->stk_top;
            if (!call_member_func(fs->str_next, 0, TRUE)) {
                if (Value_type(fg->error) == fs->cls_stopiter) {
                    // throw StopIterationでjmp
                    unref(fg->error);
                    fg->error = VALUE_NULL;
                    pc = (int)p->op[1];

                    // pop
                    v -= 1;
                    while (fg->stk_top > v) {
                        Value_pop();
                    }
                    break;
                }
                goto THROW;
            }
            pc += 3;
            break;
        }
        case OP_CALL_ITER:
            if (!call_member_func(fs->str_iterator, 0, FALSE)) {
                goto THROW;
            }
            pc += 2;
            break;
        case OP_CALL_INIT: // スタック上のオブジェクトに対してコンストラクタを呼ぶ
            if (!call_super_constructor(Value_vp(p->op[1]), p->s)) {
                goto THROW;
            }
            // 戻り値を除去
            Value_pop();
            pc += 3;
            break;
        case OP_CALL_IN: {
            Value *v = fg->stk_top - 2;
            RefNode *type = Value_type(v[1]);
            RefNode *fn_in = Hash_get_p(&type->u.c.h, fs->symbol_stock[T_IN]);
            if (fn_in == NULL || (fn_in->type != NODE_FUNC && fn_in->type != NODE_FUNC_N)) {
                throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, fs->symbol_stock[T_IN]);
                goto THROW;
            }

            // 左右を入れ替える
            {
                Value tmp = v[0];
                v[0] = v[1];
                v[1] = tmp;
            }

            // right.__in(left)
            if (!call_function(fn_in, 1)) {
                goto THROW;
            }
            pc += 2;
            break;
        }

        case OP_CALL:
        case OP_CALL_POP: { // 関数呼び出し
            Value v = fg->stk_top[-p->s - 1];
            if (Value_isref(v)) {
                RefHeader *rh = Value_ref_header(v);
                if (rh->n_memb > 0) {
                    if (!call_function_obj(p->s)) {
                        goto THROW;
                    }
                } else {
                    RefNode *r = Value_vp(v);
                    if (r->type == NODE_CLASS) {
                        if (!call_member_func(fs->str_new, p->s, TRUE)) {
                            goto THROW;
                        }
                    } else if (r->type == NODE_MODULE) {
                        if (!call_member_func(fs->str_toplevel, p->s, TRUE)) {
                            goto THROW;
                        }
                    } else if (r->type == NODE_FUNC || r->type == NODE_FUNC_N) {
                        if (!call_function(r, p->s)) {
                            goto THROW;
                        }
                    } else {
                        if (!call_member_func(fs->symbol_stock[T_LP], p->s, TRUE)) {
                            goto THROW;
                        }
                    }
                }
            } else {
                if (!call_member_func(fs->symbol_stock[T_LP], p->s, TRUE)) {
                    goto THROW;
                }
            }
            if (p->type == OP_CALL_POP) {
                Value_pop();
            }
            pc += 2;
            break;
        }

        case OP_EQUAL:
        case OP_EQUAL2: {
            Value v1 = fg->stk_top[-2];
            Value v2 = fg->stk_top[-1];
            int neq = (p->s == T_NEQ);
            int result;

            if (v1 == v2) {
                result = !neq;
                unref(v1);
                unref(v2);
                fg->stk_top--;
            } else {
                RefNode *type = Value_type(v1);
                RefNode *v2_type = Value_type(v2);

                if (type == v2_type) {
                    if (Value_isint(v1) && Value_isint(v2)) {
                        // もしequalなら、最初のifが成立しているため
                        result = neq;
                        fg->stk_top--;
                    } else if (type == fs->cls_str || type == fs->cls_bytes) {
                        RefStr *r1 = Value_vp(v1);
                        RefStr *r2 = Value_vp(v2);
                        if (r1->size == r2->size && memcmp(r1->c, r2->c, r1->size) == 0) {
                            result = !neq;
                        } else {
                            result = neq;
                        }
                        fg->stk_top--;
                    } else {
                        RefNode *fn_eq = Hash_get_p(&type->u.c.h, fs->symbol_stock[T_EQ]);
                        if (fn_eq == NULL) {
                            throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, fs->symbol_stock[T_EQ]);
                            goto THROW;
                        }
                        if (!call_function(fn_eq, 1)) {
                            goto THROW;
                        }
                        v1 = fg->stk_top[-1];

                        if (Value_bool(v1)) {
                            // 参照オブジェクトの可能性がある
                            unref(v1);
                            result = !neq;
                        } else {
                            // false or null
                            result = neq;
                        }
                    }
                } else if (p->type == OP_EQUAL2) {
                    // switchの比較
                    result = neq;
                    fg->stk_top--;
                } else {
                    throw_errorf(fs->mod_lang, "TypeError",
                                "Compare operator given different type values (%n and %n)", type, v2_type);
                    goto THROW;
                }
            }
            fg->stk_top[-1] = bool_Value(result);
            pc += 2;
            break;
        }
        case OP_CMP: { // stktopと0を大小比較
            Value v1 = fg->stk_top[-2];
            Value v2 = fg->stk_top[-1];
            RefNode *type = Value_type(v1);
            RefNode *v2_type = Value_type(v2);
            int32_t cmp;

            if (type == v2_type) {
                if (Value_isint(v1) && Value_isint(v2)) {
                    if (Value_integral(v1) < Value_integral(v2)) {
                        cmp = -1;
                    } else if (Value_integral(v1) > Value_integral(v2)) {
                        cmp = 1;
                    } else {
                        cmp = 0;
                    }
                    fg->stk_top--;
                } else if (type == fs->cls_str || type == fs->cls_bytes) {
                    RefStr *r1 = Value_vp(v1);
                    RefStr *r2 = Value_vp(v2);
                    int len = (r1->size < r2->size ? r1->size : r2->size);
                    cmp = memcmp(r1->c, r2->c, len);
                    if (cmp == 0) {
                        cmp = r1->size - r2->size;
                    }
                    fg->stk_top--;
                } else {
                    RefNode *fn_cmp = Hash_get_p(&type->u.c.h, fs->symbol_stock[T_CMP]);
                    if (fn_cmp == NULL) {
                        throw_error_select(THROW_NO_MEMBER_EXISTS__NODE_REFSTR, type, fs->symbol_stock[T_CMP]);
                        goto THROW;
                    }
                    if (!call_function(fn_cmp, 1)) {
                        goto THROW;
                    }
                    v1 = fg->stk_top[-1];

                    if (Value_isint(v1)) {
                        cmp = Value_integral(v1);
                    } else if (Value_type(v1) == fs->cls_int) {
                        RefInt *ri = Value_vp(v1);
                        if (ri->bi.sign) {
                            cmp = -1;
                        } else {
                            cmp = 1;
                        }
                        unref(v1);
                    } else {
                        throw_errorf(fs->mod_lang, "TypeError", "op_cmp return value must be 'Int' but %n", type);
                        goto THROW;
                    }
                }
            } else {
                throw_errorf(fs->mod_lang, "TypeError",
                            "Compare operator given difference type values (%n and %n)", type, v2_type);
                goto THROW;
            }
            switch (p->s) {
            case T_LT:
                fg->stk_top[-1] = bool_Value(cmp < 0);
                break;
            case T_LE:
                fg->stk_top[-1] = bool_Value(cmp <= 0);
                break;
            case T_GT:
                fg->stk_top[-1] = bool_Value(cmp > 0);
                break;
            case T_GE:
                fg->stk_top[-1] = bool_Value(cmp >= 0);
                break;
            case T_CMP:
                if (cmp < 0) {
                    cmp = -1;
                } else if (cmp > 0) {
                    cmp = 1;
                }
                fg->stk_top[-1] = int32_Value(cmp);
                break;
            }
            pc += 2;
            break;
        }
        case OP_NOT: {
            Value *v = fg->stk_top - 1;
            if (Value_bool(*v)) {
                unref(*v);
                *v = VALUE_FALSE;
            } else {
                *v = VALUE_TRUE;
            }
            pc += 1;
            break;
        }
        case OP_JMP: // 無条件ジャンプ
            pc = (int)p->op[0];
            break;
        case OP_POP_IF_J: { // stk_topを取り出して真ならjmp
            Value v = fg->stk_top[-1];
            if (Value_bool(v)) {
                unref(v);
                pc = (int)p->op[0];
            } else {
                pc += 2;
            }
            fg->stk_top--;
            break;
        }
        case OP_POP_IFN_J: { // stk_topを取り出して偽ならjmp
            Value v = fg->stk_top[-1];
            if (Value_bool(v)) {
                unref(v);
                pc += 2;
            } else {
                pc = (int)p->op[0];
            }
            fg->stk_top--;
            break;
        }
        case OP_IF_J_POP: { // 真ならjmp,偽ならpop
            Value v = fg->stk_top[-1];
            if (Value_bool(v)) {
                pc = (int)p->op[0];
            } else {
                fg->stk_top--;
                pc += 2;
            }
            break;
        }
        case OP_IF_POP_J: { // 真ならpop,偽ならjmp
            Value v = fg->stk_top[-1];
            if (Value_bool(v)) {
                unref(v);
                fg->stk_top--;
                pc += 2;
            } else {
                pc = (int)p->op[0];
            }
            break;
        }
        case OP_IFN_POPJ: { // 偽ならpop,jmp
            Value v = fg->stk_top[-1];
            if (Value_bool(v)) {
                pc += 2;
            } else {
                fg->stk_top--;
                pc = (int)p->op[0];
            }
            break;
        }
        case OP_IFNULL_J: { // nullならjmp
            Value v = fg->stk_top[-1];
            if (v == VALUE_NULL) {
                pc = (int)p->op[0];
            } else {
                pc += 2;
            }
            break;
        }
        case OP_CATCH_JMP:
            if (is_subclass(Value_type(fg->error), Value_vp(p->op[1]))) {
                // 例外オブジェクトをセット
                *fg->stk_top++ = fg->error;
                fg->error = VALUE_NULL;
                pc += 3;
            } else if (p->op[0] > 0) {
                // 次のcatch節
                pc = (int)p->op[0];
            } else {
                fg->stk_top--;
                // 例外を上に伝える
                fv->n_callfunc--;
                return FALSE;
            }
            break;

        case OP_POP: {
            Value *vp;
            Value *v2 = fg->stk_top - p->s;
            for (vp = fg->stk_top - 1; vp >= v2; vp--) {
                Value v = *vp;
                if (Value_isref(v)) {
                    RefHeader *r = Value_ref_header(v);
                    if (r->nref > 0 && --r->nref == 0) {
                        Value_release_ref(v);
                    }
                }
            }
            fg->stk_top = v2;
            pc += 2;
            break;
        }

        default: {
            RefNode *defm = func->defined_module;
            fatal_errorf("%n (pc=%d:type=%d) : unknown opcode", defm, pc, p->type);
            break;
        }
        }
    }

THROW:
    // 例外を発生させる可能性のある命令は必ず行番号を持っている
    if (p != NULL) {
        add_stack_trace(NULL, func, (int)p->op[0]);
    }
    // スタック巻上げ
    while (fg->stk_top > fg->stk_base) {
        Value *v = fg->stk_top - 1;

        switch (*v & 3ULL) {
        case 0ULL:
            if (*v != VALUE_NULL){
                RefHeader *r = Value_ref_header(*v);
                if (r->nref > 0 && --r->nref == 0) {
                    Value_release_ref(*v);
                }
            }
            break;
        case 3ULL: {
            int catch_p = ((uint32_t)*v) >> 2;
            if (catch_p > 0 && fg->error != VALUE_NULL) {
                pc = catch_p;
                *v &= 0xFFFFffff00000003ULL;
                goto NORMAL;
            }
            break;
        }
        }
        fg->stk_top = v;
    }
    fv->n_callfunc--;
    return FALSE;
}

static int invoke_native(RefNode *node)
{
    Value vret = VALUE_NULL;
    NativeFunc function = node->u.f.u.fn;
    int ret = function(&vret, fg->stk_base, node);
    Value *v;
    Value *base = fg->stk_base;

    // 引数を除去
    fv->n_callfunc++;
    for (v = fg->stk_top - 1; v >= base; v--) {
        if (Value_isref(*v)) {
            RefHeader *r = Value_ref_header(*v);
            if (r->nref > 0 && --r->nref == 0) {
                Value_release_ref(*v);
            }
        }
    }
    if (!ret) {
        add_stack_trace(NULL, node, 0);
    }
    *base = vret;
    fv->n_callfunc--;
    fg->stk_top = base + 1;

    return ret;
}

/**
 * 引数の数が範囲外
 */
static void throw_argument_num_error(RefNode *node, int argc)
{
    const char *err_cls = "ArgumentError";

    if (node->u.f.arg_min == node->u.f.arg_max) {
        throw_errorf(fs->mod_lang, err_cls, "%d arguments excepted (%d given)", node->u.f.arg_min, argc);
    } else if (argc < node->u.f.arg_min) {
        throw_errorf(fs->mod_lang, err_cls, "%d or more arguments excepted (%d given)", node->u.f.arg_min, argc);
    } else {
        throw_errorf(fs->mod_lang, err_cls, "%d or less arguments excepted (%d given)", node->u.f.arg_max, argc);
    }
}

static int validate_arguments(RefNode *node, int argc)
{
    RefNode **vck;

    // 引数の数チェック
    if (argc < node->u.f.arg_min || (node->u.f.arg_max >= 0 && argc > node->u.f.arg_max)) {
        throw_argument_num_error(node, argc);
        add_stack_trace(node->defined_module, node, node->defined_line);
        return FALSE;
    }

    // 型チェック
    vck = node->u.f.arg_type;
    if (vck != NULL) {
        int ck_argc = (node->u.f.arg_max >= 0 ? argc : node->u.f.arg_min);
        Value *vstk = fg->stk_top - argc;

        int i;
        for (i = 0; i < ck_argc; i++) {
            RefNode *vt = vck[i];

            if (vt != NULL) {
                RefNode *type = Value_type(vstk[i]);
                if ((vt->opt & NODEOPT_ABSTRACT) != 0) {
                    while (type != fs->cls_obj) {
                        if (type == vt) {
                            break;
                        }
                        type = type->u.c.super;
                    }
                    if (type == fs->cls_obj && vt != fs->cls_obj) {
                        // type#castで変換を試みる
                        throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, vt, Value_type(vstk[i]), i + 1);
                        add_stack_trace(node->defined_module, node, node->defined_line);
                        return FALSE;
                    }
                } else {
                    // 継承しないので、1回しか探索しない
                    if (type != vt) {
                        throw_error_select(THROW_ARGMENT_TYPE__NODE_NODE_INT, vt, type, i + 1);
                        add_stack_trace(node->defined_module, node, node->defined_line);
                        return FALSE;
                    }
                }
            }
        }
    }
    return TRUE;
}

int call_function(RefNode *node, int argc)
{
    Value *stk_base = fg->stk_base;
    int ret;

    // 最初の1つはthis
    fg->stk_base = fg->stk_top - argc - 1;

    if (!validate_arguments(node, argc)) {
        fg->stk_base = stk_base;
        return FALSE;
    }

    if ((node->type & NODEMASK_FUNC) != 0) {
        int argc_max = node->u.f.arg_max;
        if (argc_max < 0) {
            // argc_minより多い引数はarrayにする
            int i;
            int argc_min = node->u.f.arg_min;
            int n = argc - argc_min;
            RefArray *arr = refarray_new(n);
            for (i = 0; i < n; i++) {
                arr->p[i] = fg->stk_base[argc_min + i + 1];
            }
            fg->stk_base[argc_min + 1] = vp_Value(arr);
            fg->stk_top = fg->stk_base + argc_min + 2;
        } else {
            // 引数が不足した分はnullで埋める
            while (argc < argc_max) {
                *fg->stk_top++ = VALUE_NULL;
                argc++;
            }
        }
        ret = invoke_code(node, 0);
    } else if ((node->type & NODEMASK_FUNC_N) != 0) {
        ret = invoke_native(node);
    } else {
        fatal_errorf("invoke_function : unknown node type : %d (%n)", node->type, node);
        return FALSE;
    }

    fg->stk_base = stk_base;
    return ret;
}

int call_function_obj(int argc)
{
    Value *v = fg->stk_top - argc - 1;
    Ref *r = Value_ref(*v);

    if (r->rh.n_memb > 0) {
        RefNode *node = Value_vp(r->v[INDEX_FUNC_FN]);
        Value *stk_base = fg->stk_base;
        int n_memb = Value_integral(r->v[INDEX_FUNC_N_LOCAL]);
        int ret;

        // 両者が同じオブジェクトを指している場合を考慮
        *v = Value_cp(r->v[INDEX_FUNC_THIS]);

        // 最初の1つはthis
        fg->stk_base = fg->stk_top - argc - 1;

        if (!validate_arguments(node, argc)) {
            unref(vp_Value(r));
            fg->stk_base = stk_base;
            return FALSE;
        }

        if ((node->type & NODEMASK_FUNC) != 0) {
            int i;
            int argc_max = node->u.f.arg_max;
            // 不足している引数にnullを代入
            while (argc < argc_max) {
                *fg->stk_top++ = VALUE_NULL;
                argc++;
            }
            // 引数の後ろに内部変数を積む
            for (i = 0; i < n_memb; i++) {
                *fg->stk_top++ = Value_cp(r->v[INDEX_FUNC_LOCAL + i]);
            }
            unref(vp_Value(r));
            ret = invoke_code(node, 0);
        } else if ((node->type & NODEMASK_FUNC_N) != 0) {
            unref(vp_Value(r));
            ret = invoke_native(node);
        } else {
            fatal_errorf("invoke_function_obj : unknown node type : %d (%n)", node->type, node);
            return FALSE;
        }

        fg->stk_base = stk_base;
        return ret;
    } else {
        return call_function(Value_vp(*v), argc);
    }
}

static void dispose_all_modules()
{
    int i;
    for (i = 0; i < fg->mod_root.entry_num; i++) {
        HashEntry *entry;
        for (entry = fg->mod_root.entry[i]; entry != NULL; entry = entry->next) {
            RefNode *m = entry->p;
            RefNode *fn = Hash_get_p(&m->u.m.h, fs->str_dtor);
            if (fn != NULL) {
                *fg->stk_top++ = VALUE_NULL;
                if (!call_function(fn, 0)) {
                    // デストラクタで例外を吐くことはできないので、即終了
                    fatal_errorf("Cannot throw in _dispose (%n)", m);
                }
                // 戻り値を除去
                Value_pop();
            }
        }
    }
}
int fox_run(RefNode *mod)
{
    RefNode *node = Hash_get_p(&mod->u.m.h, fs->str_toplevel);

    if (node != NULL && node->type == NODE_FUNC) {
        int result;
        PtrList *p;

        fg->stk_base = fg->stk;
        fg->stk[0] = VALUE_NULL;
        fg->stk_top = fg->stk + 1;

        result = invoke_code(node, 0);

        // 戻り値を除去
        fg->stk_base = fg->stk;
        unref(*fg->stk_base);

        // 定数オブジェクトを削除
        for (p = fv->const_refs; p != NULL; p = p->next) {
            unref(vp_Value(p->u.p));
        }

        dispose_all_modules();
        return result;
    } else {
        // cannot happen
        return FALSE;
    }
}
