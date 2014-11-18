#define DEFINE_GLOBALS
#include "fox_vm.h"
#include <string.h>
#include <stdio.h>


typedef struct {
    const char *srcfile;
} ArgumentInfo;


void print_foxinfo()
{
    StrBuf sb;

    StrBuf_init(&sb, 32);

    StrBuf_add(&sb, "foxw ", -1);
    StrBuf_printf(&sb, "%d.%d.%d", FOX_VERSION_MAJOR, FOX_VERSION_MINOR, FOX_VERSION_REVISION);
    StrBuf_printf(&sb, ", %dbit", (int)sizeof(void*) * 8);
#ifndef NO_DEBUG
    StrBuf_add(&sb, " (debug-build)", -1);
#endif
    show_error_message(sb.p, sb.size, TRUE);

    StrBuf_close(&sb);
}

static int parse_args(ArgumentInfo *ai, int argc, const char **argv)
{
    if (argc >= 2) {
        const char *srcfile = NULL;
        const char *arg_file = argv[1];
        Str src = Str_new(arg_file, -1);
        char *pwd;

        if (is_absolute_path(src)) {
            Str curdir_s = base_dir_with_sep(src.p, src.size);
            char *curdir_p = str_dup_p(curdir_s.p, curdir_s.size, NULL);
            set_current_directory(curdir_p);
            free(curdir_p);

            pwd = get_current_directory();
            srcfile = arg_file;
        } else {
            // 相対パス
            pwd = get_current_directory();
            srcfile = str_printf("%s" SEP_S "%s", pwd, arg_file);
        }
        {
            // カレントディレクトリの末尾に/を付ける
            int pwd_len = strlen(pwd);
            RefStr *rs = refstr_new_n(fs->cls_file, pwd_len + 1);
            memcpy(rs->c, pwd, pwd_len);
            if (rs->c[pwd_len - 1] != SEP_C) {
                rs->c[pwd_len] = SEP_C;
                rs->c[pwd_len + 1] = '\0';
            } else {
                rs->size = pwd_len;
                rs->c[pwd_len] = '\0';
            }
            fv->cur_dir = rs;
            free(pwd);
        }

        ai->srcfile = srcfile;
        fv->argc = argc - 2;
        fv->argv = argv + 2;
        return TRUE;
    } else {
        print_foxinfo();
        return FALSE;
    }
}
void print_last_error()
{
    if (fg->error != VALUE_NULL && strcmp(fv->err_dst, "null") != 0) {
        if (strcmp(fv->err_dst, "alert") == 0) {
            StrBuf sb;
            StrBuf_init(&sb, 0);
            fox_error_dump(&sb, -1, FALSE);
            show_error_message(sb.p, sb.size, FALSE);
            StrBuf_close(&sb);
        } else {
            fox_error_dump(NULL, -1, TRUE);
        }
    }
}

int main_fox(int argc, const char **argv)
{
    ArgumentInfo ai;
    RefNode *mod;
    RefStr *name_startup;
    int ret = FALSE;

    memset(&ai, 0, sizeof(ai));
    init_stdio();

    // デフォルト値
    fs->max_alloc = 32 * 1024 * 1024; // 32MB
    fs->max_stack = 32768;
    fv->err_dst = "alert";

    if (!parse_args(&ai, argc, argv)) {
        goto ERROR_END;
    }
    fox_init_compile(FALSE);

    if (ai.srcfile != NULL) {
        mod = get_module_by_file(ai.srcfile);
        if (mod != NULL) {
            mod->u.m.src_path = ai.srcfile;
        }
    } else {
        throw_errorf(fs->mod_lang, "ArgumentError", "Missing filename");
        goto ERROR_END;
    }

    // ソースを読み込んだ後で実行
    if (fs->cgi_mode) {
        throw_errorf(fs->mod_lang, "CompileError", "CGI mode is not supported");
        goto ERROR_END;
    }
    init_fox_stack();

    if (mod == NULL) {
        throw_error_select(THROW_CANNOT_OPEN_FILE__STR, Str_new(ai.srcfile, -1));
        goto ERROR_END;
    }
    if (fg->error != VALUE_NULL) {
        goto ERROR_END;
    }

    // 参照できない名前で登録
    name_startup = intern("[startup]", 9);
    mod->name = name_startup;
    Hash_add_p(&fg->mod_root, &fg->st_mem, name_startup, mod);

    if (!fox_link()) {
        goto ERROR_END;
    }
    if (!fox_run(mod)) {
        goto ERROR_END;
    }
    ret = TRUE;

ERROR_END:
    print_last_error();
    fox_close();
    return ret;
}

