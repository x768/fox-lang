#include "m_net.h"
#include <iphlpapi.h>


static int initialize_done;

int send_fox(int fd, const char *buf, int size)
{
    return send(fd, buf, size, 0);
}
int recv_fox(int fd, char *buf, int size)
{
    return recv(fd, buf, size, 0);
}

void initialize_winsock()
{
    if (!initialize_done) {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);

        initialize_done = TRUE;
    }
}
void shutdown_winsock()
{
    if (initialize_done) {
        WSACleanup();
    }
}
char *winsock_strerror()
{
    enum {
        ERRMSG_SIZE = 256,
    };
    static char *err_buf;

    wchar_t *wbuf = malloc(ERRMSG_SIZE * sizeof(wchar_t));
    int len;

    FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        GetLastError(),
        0,
        wbuf,
        ERRMSG_SIZE,
        NULL);

    if (err_buf == NULL) {
        err_buf = fs->Mem_get(&fg->st_mem, ERRMSG_SIZE * 3);
    }
    utf16_to_utf8(err_buf, wbuf, -1);
    free(wbuf);

    // 末尾の改行を削除
    len = strlen(err_buf);
    if (len >= 2 && err_buf[len - 2] == '\r' && err_buf[len - 1] == '\n') {
        err_buf[len - 2] = '\0';
    }

    return err_buf;
}

int getifaddrs_sub(RefArray *ra)
{
    ULONG buf_len = sizeof(IP_ADAPTER_ADDRESSES);
    IP_ADAPTER_ADDRESSES *ai = malloc(buf_len);
    IP_ADAPTER_ADDRESSES *p;
    DWORD ret = 0;

    if (ai == NULL) {
        goto MEMORY_ERROR;
    }
    ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, ai, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(ai);
        ai = malloc(buf_len);
        if (ai == NULL) {
            goto MEMORY_ERROR;
        }
        ret = GetAdaptersAddresses(AF_UNSPEC, 0, NULL, ai, &buf_len);
    }
    if (ret != NO_ERROR) {
        fs->throw_errorf(fs->mod_io, "SocketError", "GetAdaptersInfo");
        return FALSE;
    }

    for (p = ai; p != NULL; p = p->Next) {
        IP_ADAPTER_UNICAST_ADDRESS *ua;

        Value name;
        if (p->FriendlyName != NULL) {
            name = fs->wstr_Value(NULL, p->FriendlyName, -1);
        } else {
            name = fs->cstr_Value(NULL, p->AdapterName, -1);
        }
        for (ua = p->FirstUnicastAddress; ua != NULL; ua = ua->Next) {
            Value *v = fs->refarray_push(ra);
            Ref *r = fs->ref_new(cls_ifaddr);
            struct sockaddr *ifa = ua->Address.lpSockaddr;

            r->v[INDEX_IFADDR_NAME] = fs->Value_cp(name);
            r->v[INDEX_IFADDR_ADDR] = vp_Value(new_refsockaddr(ifa, FALSE));
            *v = vp_Value(r);
        }
        fs->unref(name);
    }
    free(ai);

    return TRUE;

MEMORY_ERROR:
    fs->throw_errorf(fs->mod_lang, "MemoryError", "Out of memory");
    return FALSE;
}
