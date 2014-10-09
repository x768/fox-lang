#define DEFINE_GLOBALS
#include "m_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct RefSockAddr {
	RefHeader rh;
	int len;
	struct sockaddr addr[0];
};


static RefNode *mod_net;
static RefNode *cls_socketio;
static RefNode *cls_ipaddr;

///////////////////////////////////////////////////////////////////////////////////////////

static void throw_socket_error(int err)
{
	fs->throw_errorf(fs->mod_io, "SocketError", "%s", gai_strerror_fox(err));
}

RefSockAddr *new_refsockaddr(struct sockaddr *sa)
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

	rsa = fs->buf_new(cls_ipaddr, sizeof(RefSockAddr) + len);
	rsa->len = len;
	memcpy(&rsa->addr, sa, len);
	return rsa;
}
static int getaddrinfo2(struct addrinfo **ai, const char *hostname, int *port, int flags)
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

int ipaddr_by_string(Value *v, const char *hostname)
{
	struct addrinfo *ai = NULL;
	int err = getaddrinfo2(&ai, hostname, NULL, AI_NUMERICHOST);

	if (err != 0) {
		throw_socket_error(err);
		return FALSE;
	}
	if (ai != NULL) {
		*v = vp_Value(new_refsockaddr(ai->ai_addr));
	} else {
		fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
		return FALSE;
	}
	freeaddrinfo(ai);

	return TRUE;
}

static int getaddrinfo_sub(Value *v, const char *hostname)
{
	struct addrinfo *res = NULL;
	struct addrinfo *ai;
	RefArray *ra;
	int err = getaddrinfo2(&res, hostname, NULL, 0);

	if (err != 0) {
		throw_socket_error(err);
		return FALSE;
	}

	ra = fs->refarray_new(0);
	*v = vp_Value(ra);
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		Value *vt = fs->refarray_push(ra);
		*vt = vp_Value(new_refsockaddr(ai->ai_addr));
	}
	freeaddrinfo(res);

	return TRUE;
}

static int net_getaddrinfo(Value *vret, Value *v, RefNode *node)
{
	RefStr *rs = Value_vp(v[1]);
	if (str_has0(rs->c, rs->size)) {
		fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
		return FALSE;
	}
	if (!getaddrinfo_sub(vret, rs->c)) {
		return FALSE;
	}
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
static int socket_connect_sub(int *pfd, Value addr, int port)
{
	if (fs->Value_type(addr) == cls_ipaddr) {
		int fd;
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

		if (getaddrinfo2(&res, hostname, &port, 0) != 0) {
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
	int fd;
	Value addr = v[1];
	int port = fs->Value_int(v[2], NULL);
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
				fs->throw_errorf(fs->mod_lang, "TypeError", "Astruct sockaddrrray of IPAddr or Str required but %n (argument #1)", ad_type);
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
	int size = fs->Value_int(v[2], NULL);
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

	if (fh->fd_write != -1) {
		send_fox(fh->fd_write, mb->buf.p, mb->buf.size);
	}

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

static int ipaddr_new(Value *vret, Value *v, RefNode *node)
{
	RefStr *rs = Value_vp(v[1]);
	if (str_has0(rs->c, rs->size)) {
		fs->throw_errorf(fs->mod_io, "SocketError", "No address found");
		return FALSE;
	}
	if (!ipaddr_by_string(vret, rs->c)) {
		return FALSE;
	}
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

///////////////////////////////////////////////////////////////////////////////////////////

static int ifaddr_new(Value *vret, Value *v, RefNode *node)
{
	*vret = vp_Value(fs->ref_new(cls_ifaddr));
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
	fs->define_native_func_a(n, net_dispose, 1, 1, (void*) TRUE, NULL, NULL, fs->cls_str);
#endif
}
static void define_class(RefNode *m)
{
	RefNode *cls;
	RefNode *n;

	cls_socketio = fs->define_identifier(m, m, "SocketIO", NODE_CLASS, 0);
	cls_ipaddr = fs->define_identifier(m, m, "IPAddr", NODE_CLASS, 0);
	cls_ifaddr = fs->define_identifier(m, m, "IFAddr", NODE_CLASS, 0);

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


	// IPAddr
	cls = cls_ipaddr;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, ipaddr_new, 1, 1, NULL, fs->cls_str);

	n = fs->define_identifier_p(m, cls, fs->str_tostr, NODE_FUNC_N, 0);
	fs->define_native_func_a(n, ipaddr_tostr, 0, 2, NULL, fs->cls_str, fs->cls_locale);
	n = fs->define_identifier(m, cls, "family", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, ipaddr_family, 0, 0, NULL);
	fs->extends_method(cls, fs->cls_obj);


	// IFAddr
	cls = cls_ifaddr;
	n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
	fs->define_native_func_a(n, ifaddr_new, 1, 1, NULL, fs->cls_str);

	n = fs->define_identifier(m, cls, "name", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, ifaddr_get, 0, 0, (void*)INDEX_IFADDR_NAME);
	n = fs->define_identifier(m, cls, "addr", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, ifaddr_get, 0, 0, (void*)INDEX_IFADDR_ADDR);
	n = fs->define_identifier(m, cls, "mask", NODE_FUNC_N, NODEOPT_PROPERTY);
	fs->define_native_func_a(n, ifaddr_get, 0, 0, (void*)INDEX_IFADDR_MASK);
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
