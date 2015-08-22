#include "m_process.h"
#include "compat.h"
#include <windows.h>
#include <stdio.h>

enum {
    PIPE_TIMEOUT_MSEC = 1000,
};

static HANDLE dup_handle(HANDLE h)
{
    HANDLE ret;

    DuplicateHandle(
        GetCurrentProcess(), h, GetCurrentProcess(), &ret,
        0, FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(h);

    return ret;
}
static void init_pipe(STARTUPINFOW *si, int flags, HANDLE *p_in, HANDLE *p_out, HANDLE *p_err, HANDLE *p_cin, HANDLE *p_cout, HANDLE *p_cerr)
{
    SECURITY_ATTRIBUTES sa;
    HANDLE hIn, hOut, hErr;
    HANDLE cin = INVALID_HANDLE_VALUE, cout = INVALID_HANDLE_VALUE, cerr = INVALID_HANDLE_VALUE;
    HANDLE in = INVALID_HANDLE_VALUE, out = INVALID_HANDLE_VALUE, err = INVALID_HANDLE_VALUE;

    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    CreatePipe(&cin, &hIn, &sa, 0);
    CreatePipe(&hOut, &cout, &sa, 0);
    CreatePipe(&hErr, &cerr, &sa, 0);

    in  = dup_handle(hIn);
    out = dup_handle(hOut);
    err = dup_handle(hErr);

    *p_in = in;
    *p_out = out;
    *p_err = err;
    *p_cin = cin;
    *p_cout = cout;
    *p_cerr = cerr;

    si->hStdError = cerr;
    si->hStdOutput = cout;
    si->hStdInput = cin;
}
static wchar_t *make_cmd_string(char **argv)
{
    int first = TRUE;
    wchar_t *ret;
    StrBuf sb;
    fs->StrBuf_init(&sb, 0);

    for (; *argv != NULL; argv++) {
        const char *arg = *argv;
        int quot = FALSE;
        int i;

        if (first) {
            first = FALSE;
        } else {
            fs->StrBuf_add_c(&sb, ' ');
        }
        // スペースがある場合または長さ0の場合、""で囲む
        if (arg[0] == '\0') {
            quot = TRUE;
        } else {
            for (i = 0; arg[i] != '\0'; i++) {
                if (arg[i] == ' ') {
                    quot = TRUE;
                    break;
                }
            }
        }

        if (quot) {
            fs->StrBuf_add_c(&sb, '"');
        }
        for (i = 0; arg[i] != '\0'; i++) {
            if (arg[i] == '"') {
                // '"' => '\"'
                fs->StrBuf_add(&sb, "\\\"", 2);
            } else {
                fs->StrBuf_add_c(&sb, arg[i]);
            }
        }
        if (quot) {
            fs->StrBuf_add_c(&sb, '"');
        }
    }

    ret = cstr_to_utf16(sb.p, sb.size);
    StrBuf_close(&sb);
    return ret;
}

int process_new_sub(RefProcessHandle *ph, int create_pipe, const char *path, char **argv, int flags)
{
    HANDLE pipe_in = INVALID_HANDLE_VALUE;
    HANDLE pipe_out = INVALID_HANDLE_VALUE;
    HANDLE pipe_err = INVALID_HANDLE_VALUE;
    HANDLE pipe_cin = INVALID_HANDLE_VALUE;
    HANDLE pipe_cout = INVALID_HANDLE_VALUE;
    HANDLE pipe_cerr = INVALID_HANDLE_VALUE;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (create_pipe) {
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = SW_HIDE;
        init_pipe(&si, flags, &pipe_in, &pipe_out, &pipe_err, &pipe_cin, &pipe_cout, &pipe_cerr);
    } else {
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_SHOW;
    }
    {
        wchar_t *wpath = cstr_to_utf16(path, -1);
        wchar_t *cmd = make_cmd_string(argv);
        if (CreateProcessW(wpath, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi) == 0) {
            return FALSE;
        }
        free(cmd);
        free(wpath);
    }
    ph->valid = TRUE;
    {
        PROCESS_INFORMATION *p = malloc(sizeof(pi));
        memcpy(p, &pi, sizeof(pi));
        ph->process_info = p;
    }

    if (create_pipe) {
        ph->fd_in = (intptr_t)pipe_out;  // 子プロセスがstdoutなので、親側は入力
        ph->fd_out = (intptr_t)pipe_in;  // 上の逆
        ph->fd_err = (intptr_t)pipe_err;
        ph->p_cin = (intptr_t)pipe_cin;
        ph->p_cout = (intptr_t)pipe_cout;
        ph->p_cerr = (intptr_t)pipe_cerr;
    }

    return TRUE;
}
int process_wait_sub(RefProcessHandle *ph)
{
    PROCESS_INFORMATION *pi = ph->process_info;

    if (pi == NULL) {
        return TRUE;
    }
    for (;;) {
        DWORD ret = 0;

        if (GetExitCodeProcess(pi->hProcess, &ret) == 0 || ret != STILL_ACTIVE) {
            ph->exit_status = ret;
            ph->valid = FALSE;
            return TRUE;
        }
        WaitForSingleObject(pi->hProcess, INFINITE);
    }
}

int is_process_terminated(RefProcessHandle *ph)
{
    PROCESS_INFORMATION *pi = ph->process_info;

    if (pi != NULL) {
        DWORD ret = 0;
        if (GetExitCodeProcess(pi->hProcess, &ret) == 0 || ret != STILL_ACTIVE) {
            ph->exit_status = ret;
            return TRUE;
        }
    }
    return FALSE;
}

int read_pipe(RefProcessHandle *ph, char *data, int data_size)
{
    PROCESS_INFORMATION *pi = ph->process_info;
    HANDLE h_in = (HANDLE)ph->fd_in;
    HANDLE h_err = (HANDLE)ph->fd_err;

    for (;;) {
        DWORD ret;
        DWORD data_read;

        if (GetExitCodeProcess(pi->hProcess, &ret) == 0 || ret != STILL_ACTIVE) {
            return -1;
        }
        WaitForSingleObject(pi->hProcess, PIPE_TIMEOUT_MSEC);

        if (PeekNamedPipe(h_in, NULL, 0, NULL, &data_read, NULL) && data_read > 0){
            if (ReadFile(h_in, data, data_size, &data_read, NULL)){
                return data_read;
            }
        }
        if (PeekNamedPipe(h_err, NULL, 0, NULL, &data_read, NULL) && data_read > 0){
            if (ReadFile(h_err, data, data_size, &data_read, NULL)){
                return data_read;
            }
        }
    }
}
void write_pipe(RefProcessHandle *ph, const char *data, int data_size)
{
    PROCESS_INFORMATION *pi = ph->process_info;
    HANDLE h_out = (HANDLE)ph->fd_out;
    DWORD size_write;

    if (h_out != INVALID_HANDLE_VALUE) {
        WaitForInputIdle(pi->hProcess, INFINITE);
        WriteFile(h_out, data, data_size, &size_write, NULL);
    }
}
void process_close_sub(RefProcessHandle *ph)
{
    PROCESS_INFORMATION *pi = ph->process_info;
    if (pi != NULL) {
        CloseHandle(pi->hThread);
        CloseHandle(pi->hProcess);
        free(pi);
        ph->process_info = NULL;
    }
}
void pipeio_close_sub(RefProcessHandle *ph)
{
    process_close_sub(ph);

    if (ph->p_cin != -1) {
        CloseHandle((HANDLE)ph->p_cin);
        ph->p_cin = -1;
    }
    if (ph->p_cout != -1) {
        CloseHandle((HANDLE)ph->p_cout);
        ph->p_cout = -1;
    }
    if (ph->p_cerr != -1) {
        CloseHandle((HANDLE)ph->p_cerr);
        ph->p_cerr = -1;
    }
    if (ph->fd_err != -1) {
        CloseHandle((HANDLE)ph->fd_err);
        ph->fd_err = -1;
    }
}

RefCharset *get_local_codepage()
{
    char buf[32];
    RefCharset *cs;

    sprintf(buf, "CP%d", GetACP());
    cs = fs->get_charset_from_name(buf, -1);
    if (cs == NULL) {
        cs = fs->cs_utf8;
    }
    return cs;
}
