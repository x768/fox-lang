#define DEFINE_GLOBALS
#include "m_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



static RefNode *mod_net;
static RefNode *cls_socketio;
static RefNode *cls_listener;
static RefNode *cls_ipaddr;
static RefNode *cls_ipaddrrange;
static RefNode *cls_macaddr;

///////////////////////////////////////////////////////////////////////////////////////////

static void throw_socket_error(int err)
{
    fs->throw_errorf(fs->mod_io, "SocketError", "%s", gai_strerror_fox(err));
}
static void *sockaddr_bytes(struct sockaddr *addr)
{
    switch (addr->sa_family) {
    case AF_INET: {
        struct sockaddr_in *addr_in = (struct sockaddr_in*)addr;
        return &addr_in->sin_addr;
    }
    case AF_INET6: {
        struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6*)addr;
        return &addr_in6->sin6_addr;
    }
    default:
        // unknown
        return NULL;
    }
}
static int sockaddr_bytes_count(struct sockaddr *addr)
{
    switch (addr->sa_family) {
    case AF_INET:
        return sizeof(((struct sockaddr_in*)NULL)->sin_addr);
    case AF_INET6:
        return sizeof(((struct sockaddr_in6*)NULL)->sin6_addr);
    default:
        // unknown
        return 0;
    }
}
static int get_mask_bit_count(const void *p, int size)
{
    // 左から探索して最初にビットが0になっている桁を探す
    int i = 0;
    const uint8_t *c = (const uint8_t*)p;

    for (i = 0; i < size; i++) {
        int ch = c[i];
        int j;
        for (j = 0; j < 8; j++) {
            if ((ch & (1 << (7 - j))) == 0) {
                return i * 8 + j;
            }
        }
    }
    return size * 8;
}
int sockaddr_get_bit_count(struct sockaddr *addr)
{
    void *p = sockaddr_bytes(addr);
    if (p != NULL) {
        return get_mask_bit_count(p, sockaddr_bytes_count(addr));
    } else {
        return 0;
    }
}

RefSockAddr *new_refsockaddr(struct sockaddr *sa, int is_range)
{
    int len;
    RefSockAddr *rsa;

    if (sa->sa_family == AF_INET) {
        len = sizeof(struct sockaddr_in);
    } else if (sa->sa_family == AF_INET6) {
        len = sizeof(struct sockaddr_in6);
    } else {
        return NULL;
    }

    rsa = fs->buf_new(is_range ? cls_ipaddrrange : cls_ipaddr, sizeof(RefSockAddr) + len);
    rsa->len = len;
    rsa->mask_bits = 0;
    memcpy(&rsa->addr, sa, len);
    return rsa;
}
static int getaddrinfo_sub(struct addrinfo **ai, const char *hostname, int *port, int flags)
{
    struct addrinfo hints;
    char *pserv = NULL;
    char serv[32];
    int ret;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // IPv4/IPv6両対応
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = flags;

    if (port != NULL) {
        sprintf(serv, "%d", *port);
        pserv = serv;
    }
    ret = getaddrinfo(hostname, pserv, &hints, ai);

    return ret;
}

static RefSockAddr *cstr_to_ipaddr(const char *hostname, int is_range)
{
    RefSockAddr *rsa = NULL;
    struct addrinfo *ai = NULL;
    int err = getaddrinfo_sub(&ai, hostname, NULL, AI_NUMERICHOST);

    if (err != 0) {
        throw_socket_error(err);
        return NULL;
    }
    if (ai != NULL) {
        rsa = new_refsockaddr(ai->ai_addr, is_range);
    } else {
        fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
        return NULL;
    }
    freeaddrinfo(ai);

    return rsa;
}

static int net_getaddrinfo(Value *vret, Value *v, RefNode *node)
{
    struct addrinfo *res = NULL;
    struct addrinfo *ai;
    RefArray *ra;
    RefStr *rs = Value_vp(v[1]);
    int err;

    if (str_has0(rs->c, rs->size)) {
        fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
        return FALSE;
    }

    err = getaddrinfo_sub(&res, rs->c, NULL, 0);
    if (err != 0) {
        throw_socket_error(err);
        return FALSE;
    }
    ra = fs->refarray_new(0);
    *vret = vp_Value(ra);

    for (ai = res; ai != NULL; ai = ai->ai_next) {
        Value *vt = fs->refarray_push(ra);
        *vt = vp_Value(new_refsockaddr(ai->ai_addr, FALSE));
    }
    freeaddrinfo(res);

    return TRUE;
}

static int net_getifaddrs(Value *vret, Value *v, RefNode *node)
{
    RefArray *ra = fs->refarray_new(0);
    *vret = vp_Value(ra);

    if (!getifaddrs_sub(ra)) {
        return FALSE;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

/**
 * addr:IPAddr or Str
 */
static int socket_connect_sub(FileHandle *pfd, Value addr, int port)
{
    if (fs->Value_type(addr) == cls_ipaddr) {
        FileHandle fd;
        RefSockAddr *rsa = Value_vp(addr);

        switch (rsa->addr->sa_family) {
        case AF_INET: {
            struct sockaddr_in *v4 = (struct sockaddr_in*)rsa->addr;
            v4->sin_port = htons(port);
            break;
        }
        case AF_INET6: {
            struct sockaddr_in6 *v6 = (struct sockaddr_in6*)rsa->addr;
            v6->sin6_port = htons(port);
            break;
        }
        }

        fd = socket(rsa->addr->sa_family, SOCK_STREAM, 0);
        if (fd == -1) {
            return FALSE;
        }
        if (connect(fd, rsa->addr, rsa->len) < 0) {
            closesocket_fox(fd);
            return FALSE;
        }
        *pfd = fd;
        return TRUE;
    } else {
        RefStr *addr_r = Value_vp(addr);
        struct addrinfo *ai = NULL;
        struct addrinfo *res;
        char *hostname = fs->str_dup_p(addr_r->c, addr_r->size, NULL);

        if (getaddrinfo_sub(&res, hostname, &port, 0) != 0) {
            free(hostname);
            return FALSE;
        }
        free(hostname);

        for (ai = res; ai != NULL; ai = ai->ai_next) {
            int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
            if (fd != -1) {
                if (connect(fd, ai->ai_addr, ai->ai_addrlen) >= 0) {
                    *pfd = fd;
                    freeaddrinfo(res);
                    return TRUE;
                }
                closesocket_fox(fd);
            }
        }
        freeaddrinfo(res);
        return FALSE;
    }
}

static int socket_new(Value *vret, Value *v, RefNode *node)
{
    FileHandle fd;
    Value addr = v[1];
    int port = fs->Value_int64(v[2], NULL);
    RefNode *addr_type = fs->Value_type(addr);

    Ref *r = fs->ref_new(cls_socketio);
    *vret = vp_Value(r);

    if (port < 0 || port > 65535) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal port number (0 - 65535)");
        return FALSE;
    }

    if (addr_type == cls_ipaddr || addr_type == fs->cls_str) {
        if (!socket_connect_sub(&fd, addr, port)) {
            fs->throw_errorf(fs->mod_io, "SocketError", "Failed to connect");
            return FALSE;
        }
    } else if (addr_type == fs->cls_list) {
        RefArray *ra = Value_vp(addr);
        int i;
        int succeeded = FALSE;
        for (i = 0; i < ra->size; i++) {
            Value ad = ra->p[i];
            RefNode *ad_type = fs->Value_type(ad);
            if (ad_type != cls_ipaddr && ad_type != fs->cls_str) {
                fs->throw_errorf(fs->mod_lang, "TypeError", "Array of IPAddr or Str required but %n (argument #1)", ad_type);
                return FALSE;
            }
            if (socket_connect_sub(&fd, ad, port)) {
                succeeded = TRUE;
                break;
            }
        }
        if (!succeeded) {
            fs->throw_errorf(fs->mod_io, "SocketError", "Fault to connect");
            return FALSE;
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "TypeError", "IPAddr, Str, List required but %n (argument #1)", addr_type);
        return FALSE;
    }

    fs->init_stream_ref(r, STREAM_READ|STREAM_WRITE);
    {
        RefFileHandle *fh = fs->buf_new(NULL, sizeof(RefFileHandle));
        r->v[INDEX_FILEIO_HANDLE] = vp_Value(fh);
        fh->fd_read = fd;
        fh->fd_write = fd;
    }

    return TRUE;
}

static int socket_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *mb = Value_vp(v[1]);
    int size = fs->Value_int64(v[2], NULL);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);
    int rd;

    if (fh->fd_read == -1) {
        fs->throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }
    rd = recv_fox(fh->fd_read, mb->buf.p + mb->buf.size, size);
    mb->buf.size += rd;

    return TRUE;
}
/*
 * 基本的にエラーが発生しても例外をださない
 */
static int socket_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefBytesIO *mb = Value_vp(v[1]);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);
    int wrote_size = 0;

    if (fh->fd_write != -1) {
        wrote_size = send_fox(fh->fd_write, mb->buf.p, mb->buf.size);
    }
    *vret = int32_Value(wrote_size);

    return TRUE;
}
static int socket_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);
    int ret = fs->stream_flush_sub(*v);

    if (fh != NULL && fh->fd_read != -1) {
        closesocket_fox(fh->fd_read);
        fh->fd_read = -1;
        fh->fd_write = -1;
    }

    return ret;
}
static int socket_empty(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    RefFileHandle *fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);

    if (fh->fd_read != -1 || fh->fd_write != -1) {
        *vret = VALUE_FALSE;
    } else {
        *vret = VALUE_TRUE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int listener_new(Value *vret, Value *v, RefNode *node)
{
    const char *error_at = NULL;
    RefSockAddr *rsa = Value_vp(v[1]);
    int addr_nbytes = sockaddr_bytes_count(rsa->addr);
    int ssock_size = (rsa->addr->sa_family == AF_INET) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);

    int port = fs->Value_int64(v[2], NULL);
    RefListener *ls = fs->buf_new(cls_listener, sizeof(RefListener) + ssock_size);

    switch (rsa->addr->sa_family) {
    case AF_INET: {
        struct sockaddr_in *in4 = (struct sockaddr_in*)ls->addr;
        memcpy(in4, sockaddr_bytes(rsa->addr), addr_nbytes);
        in4->sin_port = htons(port);
        break;
    }
    case AF_INET6: {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6*)ls->addr;
        memcpy(in6, sockaddr_bytes(rsa->addr), addr_nbytes);
        in6->sin6_port = htons(port);
        break;
    }
    default:
        fs->throw_errorf(fs->mod_io, "SocketError", "Illigal IPAddr");
        return FALSE;
    }

    ls->sock = socket(rsa->addr->sa_family, SOCK_STREAM, 0);
    if (ls->sock == -1) {
        error_at = "socket";
        goto FINALLY;
    }
    if (bind(ls->sock, ls->addr, ssock_size) == -1) {
        error_at = "bind";
        goto FINALLY;
    }
    if (listen(ls->sock, 5) == -1) {
        error_at = "listen";
        goto FINALLY;
    }

    *vret = vp_Value(ls);

FINALLY:
    if (error_at != NULL) {
        fprintf(stderr, "%d, EINVAL=%d\n", errno, EINVAL);
        fs->throw_errorf(fs->mod_io, "SocketError", "Failed (%s)", error_at);
        return FALSE;
    }

    return TRUE;
}
static int listener_close(Value *vret, Value *v, RefNode *node)
{
    RefListener *ls = Value_vp(*v);

    if (ls->sock != -1) {
        closesocket_fox(ls->sock);
        ls->sock = -1;
    }

    return TRUE;
}
static int listener_accept(Value *vret, Value *v, RefNode *node)
{
    RefListener *ls = Value_vp(*v);
    socklen_t sa_len;
    struct sockaddr *sa;
    FileHandle sock;

    if (ls->sock == -1) {
        fs->throw_error_select(THROW_NOT_OPENED_FOR_READ);
        return FALSE;
    }

    sa = malloc(sizeof(struct sockaddr_in6));
    sock = accept(ls->sock, sa, &sa_len);

    if (sock != -1) {
        Ref *sock_r = fs->ref_new(cls_socketio);
        RefFileHandle *fh = fs->buf_new(NULL, sizeof(RefFileHandle));
        fs->init_stream_ref(sock_r, STREAM_READ|STREAM_WRITE);
        sock_r->v[INDEX_FILEIO_HANDLE] = vp_Value(fh);
        fh->fd_read = sock;
        fh->fd_write = sock;
        *vret = vp_Value(sock_r);
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int ipaddr_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    RefSockAddr *rsa;
    if (str_has0(rs->c, rs->size)) {
        fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
        return FALSE;
    }
    rsa = cstr_to_ipaddr(rs->c, FALSE);
    if (rsa == NULL) {
        return FALSE;
    }
    *vret = vp_Value(rsa);
    return TRUE;
}
static int ipaddr_family(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rsa = Value_vp(*v);
    const char *name = "";

    switch (rsa->addr->sa_family) {
    case AF_INET:
        name = "IPv4";
        break;
    case AF_INET6:
        name = "IPv6";
        break;
    default:
        break;
    }

    *vret = fs->cstr_Value(fs->cls_str, name, -1);
    return TRUE;
}
static int ipaddr_tostr(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rsa = Value_vp(*v);
    char cbuf[NI_MAXHOST];

    getnameinfo(rsa->addr, rsa->len, cbuf, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    *vret = fs->cstr_Value(fs->cls_str, cbuf, -1);

    return TRUE;
}
static int32_t bytes_hash(const char *p, int size)
{
    const char *c;
    const char *end;
    uint32_t h = 0;

    if (size < 0) {
        size = strlen(p);
    }
    end = p + size;

    for (c = p; c < end; c++) {
        h = h * 31 + *c;
    }
    return h & INT32_MAX;
}
static int ipaddr_hash(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rs = Value_vp(*v);
    const char *p = sockaddr_bytes(rs->addr);
    int size = sockaddr_bytes_count(rs->addr);

    *vret = int32_Value(bytes_hash(p, size));
    return TRUE;
}
static int ipaddr_eq(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rs1 = Value_vp(*v);
    RefSockAddr *rs2 = Value_vp(v[1]);

    const void *p1 = sockaddr_bytes(rs1->addr);
    const void *p2 = sockaddr_bytes(rs2->addr);
    int n1 = sockaddr_bytes_count(rs1->addr);
    int n2 = sockaddr_bytes_count(rs2->addr);

    if (n1 == n2 && memcmp(p1, p2, n1) == 0 && rs1->mask_bits == rs2->mask_bits) {
        *vret = VALUE_TRUE;
    } else {
        *vret = VALUE_FALSE;
    }

    return TRUE;
}
static int ipaddr_cmp(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rs1 = Value_vp(*v);
    RefSockAddr *rs2 = Value_vp(v[1]);

    if (rs1->addr->sa_family == rs2->addr->sa_family) {
        const void *p1 = sockaddr_bytes(rs1->addr);
        const void *p2 = sockaddr_bytes(rs2->addr);
        int n = sockaddr_bytes_count(rs1->addr);
        int cmp = memcmp(p1, p2, n);
        if (cmp != 0) {
            *vret = int32_Value(cmp);
        } else {
            // IPAddrではmask_bitsは常に0
            if (rs1->mask_bits < rs2->mask_bits) {
                *vret = int32_Value(-1);
            } else if (rs1->mask_bits > rs2->mask_bits) {
                *vret = int32_Value(1);
            } else {
                *vret = int32_Value(0);
            }
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "IP address families are different");
        return FALSE;
    }

    return TRUE;
}

static int ipaddr_const_any(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rsa = NULL;

    switch (FUNC_INT(node)) {
    case AF_INET:
        rsa = fs->buf_new(cls_ipaddr, sizeof(RefSockAddr) + sizeof(struct sockaddr_in));
        rsa->len = sizeof(struct sockaddr_in);
        {
            struct sockaddr_in *addr_in = (struct sockaddr_in*)rsa->addr;
            addr_in->sin_family = AF_INET;
        }
        break;
    case AF_INET6:
        rsa = fs->buf_new(cls_ipaddr, sizeof(RefSockAddr) + sizeof(struct sockaddr_in6));
        rsa->len = sizeof(struct sockaddr_in6);
        {
            struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6*)rsa->addr;
            addr_in6->sin6_family = AF_INET6;
        }
        break;
    default:
        break;
    }

    if (rsa != NULL) {
        rsa->mask_bits = 0;
        *vret = vp_Value(rsa);
    }
    return TRUE;
}

static int is_all_digit(const char *p)
{
    while (*p != '\0') {
        if (!isdigit_fox(*p)) {
            return FALSE;
        }
        p++;
    }
    return TRUE;
}
static int ipaddrrange_new_sub(Value *vret, const char *ipaddr, const char *mask)
{
    RefSockAddr *rsa = cstr_to_ipaddr(ipaddr, TRUE);

    if (rsa == NULL) {
        return FALSE;
    }
    if (mask == NULL || mask[0] == '\0') {
        rsa->mask_bits = 128;
    } else if (is_all_digit(mask)) {
        long bits;
        errno = 0;
        bits = strtol(mask, NULL, 10);
        if (errno == 0 && bits >= 0) {
            rsa->mask_bits = bits;
        }
    } else {
        struct addrinfo *ai = NULL;
        int err = getaddrinfo_sub(&ai, mask, NULL, AI_NUMERICHOST);
        if (err != 0) {
            throw_socket_error(err);
            return FALSE;
        }
        if (ai != NULL) {
            rsa->mask_bits = get_mask_bit_count(sockaddr_bytes(ai->ai_addr), sockaddr_bytes_count(ai->ai_addr));
        } else {
            fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
            return FALSE;
        }
        freeaddrinfo(ai);
    }

    {
        int max_bits = sockaddr_bytes_count(rsa->addr) * 8;
        if (rsa->mask_bits > max_bits) {
            rsa->mask_bits = max_bits;
        }
    }

    *vret = vp_Value(rsa);
    return TRUE;
}
static int ipaddrrange_new(Value *vret, Value *v, RefNode *node)
{
    RefStr *rs = Value_vp(v[1]);
    char *ipaddr = NULL;
    const char *mask = NULL;
    int ret;

    if (str_has0(rs->c, rs->size)) {
        fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
        return FALSE;
    }
    
    if (fg->stk_top > v + 2) {
        char bits_str[8];
        RefNode *v2_type = fs->Value_type(v[2]);
        if (v2_type == fs->cls_str) {
            mask = Value_cstr(v[2]);
        } else if (v2_type == fs->cls_int) {
            int64_t imask = fs->Value_int64(v[2], NULL);
            if (imask < 0) {
                imask = 0;
            } else if (imask > 128) {
                imask = 128;
            }
            sprintf(bits_str, "%d", (int)imask);
            mask = bits_str;
        } else {
            fs->throw_error_select(THROW_ARGMENT_TYPE2__NODE_NODE_NODE_INT, fs->cls_str, fs->cls_int, v2_type, 2);
            return FALSE;
        }
        ipaddr = Value_cstr(v[1]);
        ret = ipaddrrange_new_sub(vret, ipaddr, mask);
    } else {
        // "/"で分割する
        int i;
        for (i = 0; i < rs->size; i++) {
            if (rs->c[i] == '/') {
                ipaddr = fs->str_dup_p(rs->c, i, NULL);
                mask = rs->c + i + 1;
                break;
            }
        }
        if (ipaddr == NULL) {
            ipaddr = fs->str_dup_p(rs->c, rs->size, NULL);
            mask = NULL;
        }
        ret = ipaddrrange_new_sub(vret, ipaddr, mask);
        free(ipaddr);
    }
 
    return ret;
}

static int ipaddrrange_tostr(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rsa = Value_vp(*v);
    char cbuf[NI_MAXHOST + 16];
    char *dst;

    getnameinfo(rsa->addr, rsa->len, cbuf, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    dst = cbuf + strlen(cbuf);
    sprintf(dst, "/%d", rsa->mask_bits);
    *vret = fs->cstr_Value(fs->cls_str, cbuf, -1);

    return TRUE;
}
static int ipaddrrange_in(Value *vret, Value *v, RefNode *node)
{
    RefSockAddr *rs_range = Value_vp(*v);
    RefSockAddr *rs = Value_vp(v[1]);

    const void *p_range = sockaddr_bytes(rs_range->addr);
    const void *p = sockaddr_bytes(rs->addr);

    if (rs_range->addr->sa_family == rs->addr->sa_family) {
        // 前方nbitsだけ比較
        int nbits = rs_range->mask_bits;
        int nbytes = nbits / 8;
        nbits = nbits % 8;
        if (memcmp(p_range, p, nbytes) != 0) {
            *vret = VALUE_FALSE;
        } else if (nbits > 0) {
            int c1 = ((const char*)p_range)[nbytes] & 0xFF;
            int c2 = ((const char*)p)[nbytes] & 0xFF;
            int mask = (0xFF00 >> nbits) & 0xFF;
            if (((c1 ^ c2) & mask) == 0) {
                *vret = VALUE_TRUE;
            } else {
                *vret = VALUE_FALSE;
            }
        } else {
            *vret = VALUE_TRUE;
        }
    } else {
        fs->throw_errorf(fs->mod_lang, "ValueError", "IP address families are different");
        return FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

static int ifaddr_new(Value *vret, Value *v, RefNode *node)
{
    *vret = vp_Value(fs->ref_new(cls_ifaddr));
    return TRUE;
}
static int ifaddr_tostr(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    StrBuf buf;

    fs->StrBuf_init_refstr(&buf, 32);
    fs->StrBuf_add(&buf, "IFAddr(name=", -1);
    {
        RefStr *name = Value_vp(r->v[INDEX_IFADDR_NAME]);
        fs->StrBuf_add(&buf, name->c, name->size);
    }

    {
        char cbuf[NI_MAXHOST];
        RefSockAddr *rsa = Value_vp(r->v[INDEX_IFADDR_ADDR]);

        if (rsa != NULL) {
            fs->StrBuf_add(&buf, ", addr=", -1);
            getnameinfo(rsa->addr, rsa->len, cbuf, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            fs->StrBuf_add(&buf, cbuf, -1);
            if (rsa->rh.type == cls_ipaddrrange) {
                sprintf(cbuf, "/%d", rsa->mask_bits);
                fs->StrBuf_add(&buf, cbuf, -1);
            }
        }
    }
    fs->StrBuf_add(&buf, ")", -1);

    *vret = fs->StrBuf_str_Value(&buf, fs->cls_str);
    return TRUE;
}
static int ifaddr_get(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    int idx = FUNC_INT(node);
    *vret = fs->Value_cp(r->v[idx]);
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

static int net_dispose(Value *vret, Value *v, RefNode *node)
{
    shutdown_winsock();
    return TRUE;
}

#endif

///////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "getaddrinfo", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, net_getaddrinfo, 1, 1, (void*) TRUE, NULL, NULL, fs->cls_str);

    n = fs->define_identifier(m, m, "getifaddrs", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, net_getifaddrs, 0, 0, NULL);

#ifdef WIN32
    n = fs->define_identifier_p(m, m, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, net_dispose, 0, 0, NULL);
#endif
}
static void define_ipaddr_const(RefNode *m, RefNode *cls)
{
    RefNode *n;

    n = fs->define_identifier(m, cls, "ANY4", NODE_CONST_U_N, 0);
    fs->define_native_func_a(n, ipaddr_const_any, 0, 0, (void*)AF_INET);

    n = fs->define_identifier(m, cls, "ANY6", NODE_CONST_U_N, 0);
    fs->define_native_func_a(n, ipaddr_const_any, 0, 0, (void*)AF_INET6);
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    cls_socketio = fs->define_identifier(m, m, "SocketIO", NODE_CLASS, 0);
    cls_ipaddr = fs->define_identifier(m, m, "IPAddr", NODE_CLASS, 0);
    cls_ipaddrrange = fs->define_identifier(m, m, "IPAddrRange", NODE_CLASS, 0);
    cls_ifaddr = fs->define_identifier(m, m, "IFAddr", NODE_CLASS, 0);
    cls_macaddr = fs->define_identifier(m, m, "MACAddr", NODE_CLASS, 0);
    cls_listener = fs->define_identifier(m, m, "Listener", NODE_CLASS, 0);

    // SocketIO
    cls = cls_socketio;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, socket_new, 2, 2, NULL, NULL, fs->cls_int);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, socket_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, socket_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "empty", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, socket_empty, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, socket_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, socket_write, 1, 1, NULL, fs->cls_bytesio);

    cls->u.c.n_memb = INDEX_FILEIO_NUM;
    fs->extends_method(cls, fs->cls_streamio);


    // Listener
    cls = cls_listener;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, listener_new, 2, 2, NULL, cls_ipaddr, fs->cls_int);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, listener_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, listener_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "accept", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, listener_accept, 0, 0, NULL);
    fs->extends_method(cls, fs->cls_obj);


    // IPAddr
    cls = cls_ipaddr;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ipaddr_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddr_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ipaddr_hash, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddr_eq, 1, 1, NULL, cls_ipaddr);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddr_cmp, 1, 1, NULL, cls_ipaddr);
    n = fs->define_identifier(m, cls, "family", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ipaddr_family, 0, 0, NULL);
    
    define_ipaddr_const(m, cls);
    fs->extends_method(cls, fs->cls_obj);


    // IPAddrRange
    cls = cls_ipaddrrange;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ipaddrrange_new, 1, 2, NULL, fs->cls_str, NULL);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddrrange_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier_p(m, cls, fs->str_hash, NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ipaddr_hash, 0, 0, NULL);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_EQ], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddr_eq, 1, 1, NULL, cls_ipaddrrange);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_CMP], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddr_cmp, 1, 1, NULL, cls_ipaddrrange);
    n = fs->define_identifier_p(m, cls, fs->symbol_stock[T_IN], NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ipaddrrange_in, 1, 1, NULL, cls_ipaddr);
    n = fs->define_identifier(m, cls, "family", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ipaddr_family, 0, 0, NULL);

    fs->extends_method(cls, fs->cls_obj);


    // MACAddr
    cls = cls_macaddr;
    fs->extends_method(cls, fs->cls_obj);


    // IFAddr
    cls = cls_ifaddr;
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ifaddr_new, 1, 1, NULL, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ifaddr_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
    n = fs->define_identifier(m, cls, "name", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ifaddr_get, 0, 0, (void*)INDEX_IFADDR_NAME);
    n = fs->define_identifier(m, cls, "addr", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, ifaddr_get, 0, 0, (void*)INDEX_IFADDR_ADDR);
    cls->u.c.n_memb = INDEX_IFADDR_NUM;
    fs->extends_method(cls, fs->cls_obj);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_net = m;

#ifdef WIN32
    initialize_winsock();
#endif

    define_class(m);
    define_func(m);
}
const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\n";
}
