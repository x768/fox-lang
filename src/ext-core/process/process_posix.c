#include "m_process.h"
#include "compat.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int process_new_sub(RefProcessHandle *ph, int create_pipe, const char *path, char **argv, int flags)
{
    intptr_t pid = 0;
    intptr_t pipe_in = -1, pipe_out = -1;
    int p_in[2], p_out[2];

    if (create_pipe) {
        if ((flags & STREAM_WRITE) != 0) {
            if (pipe(p_in) == -1) {
                return FALSE;
            }
        }
        if ((flags & STREAM_READ) != 0) {
            if (pipe(p_out) == -1) {
                if ((flags & STREAM_WRITE) != 0) {
                    close(p_in[0]);
                    close(p_in[1]);
                }
                return FALSE;
            }
        }
    }

    pid = fork();
    if (pid == -1) {
        // failed
        if ((flags & STREAM_WRITE) != 0) {
            close(p_in[0]);
            close(p_in[1]);
        }
        if ((flags & STREAM_WRITE) != 0) {
            close(p_out[0]);
            close(p_out[1]);
        }
        return FALSE;
    }
    if (pid == 0) {
        if (create_pipe) {
            // 子プロセスの入力をパイプに接続
            if ((flags & STREAM_WRITE) != 0) {
                close(STDIN_FILENO);
                dup2(p_in[0], STDIN_FILENO);
                close(p_in[0]);
                close(p_in[1]);
            }
            // 子プロセスの出力をパイプに接続
            if ((flags & STREAM_READ) != 0) {
                close(STDOUT_FILENO);
                dup2(p_out[1], STDOUT_FILENO);
                close(p_out[1]);
                close(p_out[0]);
            }
        }
        execv(path, argv);
        exit(1);
    } else {
        if ((flags & STREAM_WRITE) != 0) {
            pipe_in = p_in[1];
            close(p_in[0]);
        }
        if ((flags & STREAM_READ) != 0) {
            pipe_out = p_out[0];
            close(p_out[1]);
        }
    }

    ph->valid = TRUE;
    ph->pid = pid;
    if (create_pipe) {
        ph->fd_in = pipe_out;  // 子プロセスがstdoutなので、親側は入力
        ph->fd_out = pipe_in;  // 上の逆
    }

    return TRUE;
}
void put_log(const char *msg, int msg_size)
{
    FileHandle fd = open_fox("/home/frog/Public/logs/log.txt", O_CREAT|O_WRONLY|O_APPEND, DEFAULT_PERMISSION);
    if (fd != -1) {
        if (msg_size < 0) {
            msg_size = strlen(msg);
        }
        write_fox(fd, msg, msg_size);
        close_fox(fd);
    }
}
int process_wait_sub(RefProcessHandle *ph)
{
    pid_t pid = ph->pid;
    if (pid == -1) {
        return TRUE;
    }
    for (;;) {
        int status;
        if (waitpid(pid, &status, WNOHANG) == -1) {
            return TRUE;
        }
        if (WIFEXITED(status)) {
            ph->exit_status = WEXITSTATUS(status);
            ph->valid = FALSE;
            return TRUE;
        }
    }
    return TRUE;
}

int is_process_terminated(RefProcessHandle *ph)
{
    pid_t pid = ph->pid;
    int status;

    if (!ph->valid || pid == -1) {
        return TRUE;
    }
    if (waitpid(pid, &status, WNOHANG) > 0) {
        if (WIFEXITED(status) || WIFSIGNALED(status)){
            ph->exit_status = WEXITSTATUS(status);
            ph->valid = FALSE;
            return TRUE;
        }
    }
    return FALSE;
}
int read_pipe(RefProcessHandle *ph, char *data, int data_size)
{
    intptr_t fd = ph->fd_in;
    int rd_size = 0;
    char ch;

    if (fd == -1 || ph->pid == -1) {
        return -1;
    }

    while (rd_size < data_size && read(fd, &ch, 1) > 0) {
        *data++ = ch;
        rd_size++;
    }
    if (rd_size == 0) {
        return -1;
    } else {
        return rd_size;
    }
}
void write_pipe(RefProcessHandle *ph, const char *data, int data_size)
{
    intptr_t fd = ph->fd_out;
    if (fd != -1) {
        write_fox(fd, data, data_size);
    }
}

void process_close_sub(RefProcessHandle *ph)
{
    pid_t pid = ph->pid;
    if (pid != -1) {
        kill(pid, SIGKILL);
        ph->pid = -1;
    }
}
void pipeio_close_sub(RefProcessHandle *ph)
{
    process_close_sub(ph);
}

RefCharset *get_local_codepage()
{
    return fs->cs_utf8;
}
