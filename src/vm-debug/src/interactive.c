#include "fox_parse.h"
#include <readline/readline.h>
#include <readline/history.h>


#define SHELL_PROMPT ">>> "
#define SHELL_PROMPT_CONTINUE "... "

// 1つの文が完了しているか調べる
// 継続: return TRUE
static int input_buffer_add(StrBuf *buf, RefNode *module, const char *line)
{
    Tok tk;
    char *cbuf;
    int result = FALSE;
    int n_nest = 0;

    if (buf->size > 0) {
        StrBuf_add_c(buf, '\n');
    }

    StrBuf_add(buf, line, strlen(line));
    cbuf = str_dup_p(buf->p, buf->size, NULL);
    Tok_init(&tk, module, cbuf);
    tk.is_test = TRUE;

    for (;;) {
        Tok_next_test(&tk);

        switch (tk.v.type) {
        case T_EOF:
            // 行が継続する場合
            if (tk.p_nest_n > 0) {
                result = TRUE;
            } else if (n_nest > 0) {
                result = TRUE;
            }
            goto BREAK2;
        case T_ERR:
            goto BREAK2;
        case T_WANTS_NEXT:
            result = TRUE;
            goto BREAK2;
        case T_LP:
        case T_LP_C:
        case T_LC:
        case T_LB:
        case T_LB_C:
            n_nest++;
            break;
        case T_RP:
        case T_RC:
        case T_RB:
            if (n_nest == 0) {
                goto BREAK2;
            }
            n_nest--;
            break;
        default:
            if (fg->error != VALUE_NULL) {
                goto BREAK2;
            }
            break;
        }
    }

BREAK2:
    free(cbuf);
    return result;
}

static void print_cio(const char *msg)
{
    stream_write_data(fg->v_cio, msg, -1);
}

static void print_error(void)
{
    StrBuf sb;
    StrBuf_init(&sb, 0);
    fox_error_dump(&sb, -1, FALSE);
    stream_write_data(fg->v_cio, sb.p, sb.size);
    StrBuf_close(&sb);

    Value_dec(fg->error);
    fg->error = VALUE_NULL;
}

//////////////////////////////////////////////////////////////////////////////////

void fox_intaractive(RefNode *module)
{
    StrBuf buf;
    char *input;
    Block *block;   // toplevel
    int input_continue = FALSE;


    if (fv->startup == NULL) {
        fv->startup = module;
    }
    module->u.m.src_path = "[intaractive]";
    StrBuf_init(&buf, 0);

    block = Block_new(NULL, NULL);
    block->offset = 1; // 暗黙のthis

    fg->stk_base = fg->stk;
    fg->stk[0] = VALUE_NULL;
    fg->stk_top = fg->stk + 1;

    while ((input = readline(input_continue ? SHELL_PROMPT_CONTINUE : SHELL_PROMPT)) != NULL) {
        if (!input_continue && input[0] == '.') {
            if (strcmp(input, ".q") == 0 || strcmp(input, ".quit") == 0 || strcmp(input, ".exit") == 0) {
                return;
            } else if (strcmp(input, ".v") == 0 || strcmp(input, ".version") == 0) {
                print_foxinfo();
                stream_flush_sub(fg->v_cio);
            } else if (strcmp(input, ".h") == 0 || strcmp(input, ".help") == 0) {
                print_cio("Ctrl+D        Exit this program\n");
                print_cio(".exit         Exit this program\n");
                print_cio(".quit (.q)    Exit this program\n");
                print_cio(".help (.h)    Show commands and operations\n");
                print_cio(".version (.v) Show fox and plugins' version\n");
                stream_flush_sub(fg->v_cio);
            } else {
                print_cio("Unknown command. Type '.h' to show help.\n");
                stream_flush_sub(fg->v_cio);
            }
        } else {
            Value *stk_prev = fg->stk_top;
            input_continue = input_buffer_add(&buf, module, input);
            if (!input_continue && fg->error == VALUE_NULL) {
                RefNode *toplevel;

                fox_init_compile(TRUE);
                Hash_init(&module->u.m.unresolved, &fv->cmp_mem, 32);
                toplevel = init_toplevel(module);
                block->func = toplevel;
                block->import = &toplevel->defined_module->u.m.import;

                // コンパイル
                StrBuf_add_c(&buf, '\0');
                if (set_module_eval(module, buf.p, toplevel, block)) {
                    fox_link();
                    buf.size = 0;

                    // 実行
                    if (fg->error == VALUE_NULL) {
                        if (!invoke_code(toplevel, 0)) {
                        }
                    }
                    dispose_opcode(toplevel);
                    toplevel->u.f.u.op = NULL;
                } else {
                    Mem_close(&fv->cmp_mem);
                    buf.size = 0;
                }
                stream_flush_sub(fg->v_cio);
            }
            if (fg->error != VALUE_NULL) {
                if (fg->stk_base > fg->stk) {
                    fg->stk_base = fg->stk;
                }
                while (fg->stk_top > stk_prev) {
                    Value_dec(fg->stk_top[-1]);
                    fg->stk_top--;
                }
                print_error();
                stream_flush_sub(fg->v_cio);
                Value_dec(fg->error);
                fg->error = VALUE_NULL;
                buf.size = 0;
            }
        }
        add_history(input);
        free(input);
    }

    // Ctrl+D
    print_cio(".quit\n");
}

//////////////////////////////////////////////////////////////////////////////////

void fox_debugger(RefNode *module)
{
    char *input;

    while ((input = readline("foxdbg> ")) != NULL) {
        char *cmd = str_dup_p(input, -1, NULL);
        char *param = cmd;
        while (isalnumu_fox(*param)) {
            param++;
        }
        if (*param != '\0') {
            *param++ = '\0';
        }

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0) {
            return;
        } else if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0) {
            print_cio("[Ctrl+D]    Exit this program\n");
            print_cio("exit        Exit this program\n");
            print_cio("quit (q)    Exit this program\n");
            print_cio("help (h)    Show commands and operations\n");
            stream_flush_sub(fg->v_cio);
        } else {
            print_cio("Unknown command. Type 'help' or 'h' to show help.\n");
            stream_flush_sub(fg->v_cio);
        }

        add_history(input);
        free(input);
        free(cmd);
    }

    // Ctrl+D
    print_cio("quit\n");
}

