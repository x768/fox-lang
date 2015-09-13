#define DEFINE_GLOBALS
#include "fox_io.h"
#include "m_number.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#endif

#include <openssl/crypto.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
#include <openssl/rand.h>


enum {
    INDEX_SSL_SSL = INDEX_FILEIO_NUM,
    INDEX_SSL_PARENT,
    INDEX_SSL_NUM,
};
enum {
    SSL_GET_CIPHER,
    SSL_GET_CIPHER_NAME,
    SSL_GET_CIPHER_VERSION,
};
enum {
    CERT_GET_SUBJECT,
    CERT_GET_ISSUER,
    CERT_GET_NOT_BEFORE,
    CERT_GET_NOT_AFTER,
};

typedef struct {
    RefHeader rh;
    SSL_CTX *ctx;
} RefSSLContext;

typedef struct {
    RefHeader rh;
    X509 *cert;
} RefCertificate;


static const FoxStatic *fs;
static FoxGlobal *fg;

static RefNode *mod_crypt;
static RefNode *cls_sslsocketio;
static RefNode *cls_cert;
static NativeFunc fn_socket_new;


static void throw_ssl_error()
{
    char errbuf[256];
    long err = ERR_get_error();

    ERR_error_string_n(err, errbuf, sizeof(errbuf));
    fs->throw_errorf(mod_crypt, "SSLError", "%s", errbuf);
}
static void init_openssl()
{
    static int initialized = FALSE;

    if (!initialized) {
        SSL_library_init();
        SSL_load_error_strings();
        initialized = TRUE;
    }
}

static int ssl_ctx_new(Value *vret, Value *v, RefNode *node)
{
    const SSL_METHOD *method = NULL;
    RefNode *cls_ssl_ctx = FUNC_VP(node);
    RefStr *mode = Value_vp(v[1]);
    RefSSLContext *ctx = fs->buf_new(cls_ssl_ctx, sizeof(RefSSLContext));
    *vret = vp_Value(ctx);

    init_openssl();

    if (str_eq(mode->c, mode->size, "TLSv1", -1)) {
        method = TLSv1_client_method();
#if 0
    } else if (str_eq(mode->c, mode->size, "TLSv1.1", -1)) {
        method = TLSv1_1_client_method();
    } else if (str_eq(mode->c, mode->size, "TLSv1.2", -1)) {
        method = TLSv1_2_client_method();
#endif
    } else if (str_eq(mode->c, mode->size, "DTLSv1", -1)) {
        method = DTLSv1_client_method();
    } else {
        fs->throw_errorf(mod_crypt, "SSLError", "Not supported type %q", mode->c);
        return FALSE;
    }
    ctx->ctx = SSL_CTX_new(method);
    if (ctx->ctx == NULL) {
        throw_ssl_error();
        return FALSE;
    }

    return TRUE;
}
static int ssl_ctx_close(Value *vret, Value *v, RefNode *node)
{
    RefSSLContext *ctx = Value_vp(*v);
    SSL_CTX_free(ctx->ctx);
    return TRUE;
}
static int ssl_ctx_connect(Value *vret, Value *v, RefNode *node)
{
    RefSSLContext *ctx = Value_vp(*v);
    Ref *r;
    RefFileHandle *fh;
    SSL *ssl;

    if (!fn_socket_new(vret, v, node)) {
        return FALSE;
    }

    r = Value_vp(*vret);
    ssl = SSL_new(ctx->ctx);
    fh = Value_vp(r->v[INDEX_FILEIO_HANDLE]);
    SSL_set_fd(ssl, fh->fd_read);

    if (SSL_connect(ssl) != 1) {
        throw_ssl_error();
        return FALSE;
    }
    r->v[INDEX_SSL_SSL] = ptr_Value(ssl);

    return TRUE;
}
static int ssl_ctx_add_ca(Value *vret, Value *v, RefNode *node)
{
    RefSSLContext *ctx = Value_vp(*v);
    RefCertificate *cr = Value_vp(v[1]);

    if (SSL_CTX_add_client_CA(ctx->ctx, cr->cert) == 0) {
        fs->throw_errorf(mod_crypt, "SSLError", "SSL_CTX_add_client_CA failt");
        return FALSE;
    }

    return TRUE;
}

static int sslsocket_read(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    SSL *ssl = Value_ptr(r->v[INDEX_SSL_SSL]);
    RefBytesIO *mb = Value_vp(v[1]);
    int size = fs->Value_int32(v[2]);
    int rd;

    if (size == INT32_MIN) {
        fs->throw_errorf(fs->mod_lang, "ValueError", "Illigal range (argument #2)");
        return FALSE;
    }
    if (ssl == NULL) {
        fs->throw_errorf(fs->mod_io, "ReadError", "Not opened for read");
        return FALSE;
    }
    rd = SSL_read(ssl, mb->buf.p + mb->buf.size, size);
    mb->buf.size = rd;

    return TRUE;
}
static int sslsocket_write(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    SSL *ssl = Value_ptr(r->v[INDEX_SSL_SSL]);
    RefBytesIO *mb = Value_vp(v[1]);
    int wrote_size = 0;

    if (ssl != NULL) {
        wrote_size = SSL_write(ssl, mb->buf.p, mb->buf.size);
    }
    *vret = int32_Value(wrote_size);
    return TRUE;
}
static int sslsocket_cipher(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    SSL *ssl = Value_ptr(r->v[INDEX_SSL_SSL]);
    if (ssl != NULL) {
        const char *s = "";

        switch (FUNC_INT(node)) {
        case SSL_GET_CIPHER:
            s = SSL_get_cipher(ssl);
            break;
        case SSL_GET_CIPHER_NAME:
            s = SSL_get_cipher_name(ssl);
            break;
        case SSL_GET_CIPHER_VERSION:
            s = SSL_get_cipher_version(ssl);
            break;
        }
        *vret = fs->cstr_Value(fs->cls_str, s, -1);
    }

    return TRUE;
}
static int sslsocket_certificate(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    SSL *ssl = Value_ptr(r->v[INDEX_SSL_SSL]);
    RefCertificate *cr = fs->buf_new(cls_cert, sizeof(RefCertificate));
    *vret = vp_Value(cr);
    cr->cert = SSL_get_peer_certificate(ssl);

    return TRUE;
}
static int sslsocket_close(Value *vret, Value *v, RefNode *node)
{
    Ref *r = Value_ref(*v);
    SSL *ssl = Value_ptr(r->v[INDEX_SSL_SSL]);
    RefFileHandle *fh = Value_ptr(r->v[INDEX_FILEIO_HANDLE]);

    if (ssl != NULL) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        r->v[INDEX_SSL_SSL] = VALUE_NULL;
    }
    if (fh != NULL && fh->fd_read != -1) {
#ifdef WIN32
        closesocket(fh->fd_read);
#else
        close(fh->fd_read);
#endif
        fh->fd_read = -1;
        fh->fd_write = -1;
    }

    return TRUE;
}

static Value X509_NAME_to_val(X509_NAME *name)
{
    if (name != NULL) {
        char *s = X509_NAME_oneline(name, NULL, 0);
        Value v = fs->cstr_Value_conv(s, -1, NULL);
        free(s);
        return v;
    } else {
        return VALUE_NULL;
    }
}
static void ASN1_TIME_to_calendar(DateTime *dt, ASN1_TIME* tm)
{
    const char* str = (const char*)tm->data;

    switch (tm->type) {
    case V_ASN1_UTCTIME:
        dt->d.year = (str[0] - '0') * 10 + (str[1] - '0');
        if (dt->d.year < 70) {
            dt->d.year += 2000;
        } else {
            dt->d.year += 1900;
        }
        str += 2;
        break;
    case V_ASN1_GENERALIZEDTIME:
        dt->d.year = (str[0] - '0') * 1000 + (str[1] - '0') * 100 + (str[2] - '0') * 10 + (str[3] - '0');
        str += 4;
        break;
    }
    dt->d.month = (str[0] - '0') * 10 + (str[1] - '0');
    str += 2;
    dt->d.day_of_month = (str[0] - '0') * 10 + (str[1] - '0');
    str += 2;
    dt->t.hour = (str[0] - '0') * 10 + (str[1] - '0');
    str += 2;
    dt->t.minute = (str[0] - '0') * 10 + (str[1] - '0');
    str += 2;
    dt->t.second = (str[0] - '0') * 10 + (str[1] - '0');
    str += 2;
    dt->t.millisec = 0;
}
static Value ASN1_TIME_to_val(ASN1_TIME *tm)
{
    if (tm != NULL) {
        RefInt64 *rt = fs->buf_new(fs->cls_timestamp, sizeof(RefInt64));
        DateTime dt;
        memset(&dt, 0, sizeof(dt));
        ASN1_TIME_to_calendar(&dt, tm);
        fs->DateTime_to_Timestamp(&rt->u.i, &dt);
        return vp_Value(rt);
    } else {
        return VALUE_NULL;
    }
}
static int cert_get_string(Value *vret, Value *v, RefNode *node)
{
    RefCertificate *cr = Value_vp(*v);
    X509_NAME *name = NULL;

    if (cr->cert != NULL) {
        switch (FUNC_INT(node)) {
        case CERT_GET_SUBJECT:
            name = X509_get_subject_name(cr->cert);
            break;
        case CERT_GET_ISSUER:
            name = X509_get_issuer_name(cr->cert);
            break;
        }
        *vret = X509_NAME_to_val(name);
    }
    return TRUE;
}
static int cert_get_time(Value *vret, Value *v, RefNode *node)
{
    RefCertificate *cr = Value_vp(*v);
    ASN1_TIME *tm = NULL;

    if (cr->cert != NULL) {
        switch (FUNC_INT(node)) {
        case CERT_GET_NOT_BEFORE:
            tm = X509_get_notBefore(cr->cert);
            break;
        case CERT_GET_NOT_AFTER:
            tm = X509_get_notAfter(cr->cert);
            break;
        }
        *vret = ASN1_TIME_to_val(tm);
    }

    return TRUE;
}
static int cert_close(Value *vret, Value *v, RefNode *node)
{
    RefCertificate *cr = Value_vp(*v);

    if (cr->cert != NULL) {
        X509_free(cr->cert);
        cr->cert = NULL;
    }
    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

enum {
    HASH_MD5,
    HASH_SHA1,
    HASH_SHA256,
    HASH_UNKNOWN,
};

static int get_hash_type(const char *src_p, int src_size)
{
    if (str_eqi(src_p, src_size, "md5", -1)) {
        return HASH_MD5;
    } else if (str_eqi(src_p, src_size, "sha1", -1)) {
        return HASH_SHA1;
    } else if (str_eqi(src_p, src_size, "sha256", -1)) {
        return HASH_SHA256;
    }
    return HASH_UNKNOWN;
}
static int ssl_get_hash(Value *vret, Value *v, RefNode *node)
{
    RefStr *src = Value_vp(v[1]);
    RefStr *type_r = Value_vp(v[2]);
    int type = get_hash_type(type_r->c, type_r->size);

    switch (type) {
    case HASH_MD5: {
        RefStr *rs = fs->refstr_new_n(fs->cls_bytes, MD5_DIGEST_LENGTH);
        *vret = vp_Value(rs);
        MD5((unsigned char *)src->c, src->size, (unsigned char *)rs->c);
        rs->c[rs->size] = '\0';
        break;
    }
    case HASH_SHA1: {
        RefStr *rs = fs->refstr_new_n(fs->cls_bytes, SHA_DIGEST_LENGTH);
        *vret = vp_Value(rs);
        SHA1((unsigned char *)src->c, src->size, (unsigned char *)rs->c);
        rs->c[rs->size] = '\0';
        break;
    }
    case HASH_SHA256: {
        RefStr *rs = fs->refstr_new_n(fs->cls_bytes, SHA256_DIGEST_LENGTH);
        *vret = vp_Value(rs);
        SHA256((unsigned char *)src->c, src->size, (unsigned char *)rs->c);
        rs->c[rs->size] = '\0';
        break;
    }
    default:
        fs->throw_errorf(fs->mod_lang, "ValueError", "Unknwon hash type (md5, sha1, sha256)");
        return FALSE;
    }

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void define_func(RefNode *m)
{
    RefNode *n;

    n = fs->define_identifier(m, m, "get_hash", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ssl_get_hash, 2, 2, NULL, fs->cls_sequence, fs->cls_str);
}
static void get_fn_socket_new(void)
{
    RefNode *m = fs->get_module_by_name("io.net", -1, TRUE, FALSE);
    RefNode *cls_socket = fs->Hash_get(&m->u.m.h, "SocketIO", -1);
    RefNode *node_socket_new = fs->Hash_get_p(&cls_socket->u.c.h, fs->str_new);
    fn_socket_new = node_socket_new->u.f.u.fn;
}
static void define_class(RefNode *m)
{
    RefNode *cls;
    RefNode *n;

    get_fn_socket_new();

    cls_sslsocketio = fs->define_identifier(m, m, "SSLSocketIO", NODE_CLASS, 0);
    cls_cert = fs->define_identifier(m, m, "Certificate", NODE_CLASS, 0);

    cls = fs->define_identifier(m, m, "SSLContext", NODE_CLASS, 0);
    n = fs->define_identifier_p(m, cls, fs->str_new, NODE_NEW_N, 0);
    fs->define_native_func_a(n, ssl_ctx_new, 1, 1, cls, fs->cls_str);

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ssl_ctx_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "connect", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ssl_ctx_connect, 2, 2, cls_sslsocketio, NULL, fs->cls_int);
    n = fs->define_identifier(m, cls, "add_ca", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, ssl_ctx_add_ca, 2, 2, NULL, NULL, cls_cert);
    fs->extends_method(cls, fs->cls_obj);


    cls = cls_sslsocketio;

    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, sslsocket_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "close", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, sslsocket_close, 0, 0, NULL);
    n = fs->define_identifier(m, cls, "_read", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, sslsocket_read, 2, 2, NULL, fs->cls_bytesio, fs->cls_int);
    n = fs->define_identifier(m, cls, "_write", NODE_FUNC_N, 0);
    fs->define_native_func_a(n, sslsocket_write, 1, 1, NULL, fs->cls_bytesio);
    n = fs->define_identifier(m, cls, "cipher", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, sslsocket_cipher, 0, 0, (void*)SSL_GET_CIPHER);
    n = fs->define_identifier(m, cls, "cipher_name", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, sslsocket_cipher, 0, 0, (void*)SSL_GET_CIPHER_NAME);
    n = fs->define_identifier(m, cls, "cipher_version", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, sslsocket_cipher, 0, 0, (void*)SSL_GET_CIPHER_VERSION);
    n = fs->define_identifier(m, cls, "certificate", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, sslsocket_certificate, 0, 0, NULL);

    cls->u.c.n_memb = INDEX_SSL_NUM;
    fs->extends_method(cls, fs->cls_streamio);


    cls = cls_cert;
    n = fs->define_identifier_p(m, cls, fs->str_dispose, NODE_FUNC_N, 0);
    fs->define_native_func_a(n, cert_close, 0, 0, NULL);

    n = fs->define_identifier(m, cls, "subject", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cert_get_string, 0, 0, (void*)CERT_GET_SUBJECT);
    n = fs->define_identifier(m, cls, "issuer", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cert_get_string, 0, 0, (void*)CERT_GET_ISSUER);
    n = fs->define_identifier(m, cls, "not_before", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cert_get_time, 0, 0, (void*)CERT_GET_NOT_BEFORE);
    n = fs->define_identifier(m, cls, "not_after", NODE_FUNC_N, NODEOPT_PROPERTY);
    fs->define_native_func_a(n, cert_get_time, 0, 0, (void*)CERT_GET_NOT_AFTER);
    fs->extends_method(cls, fs->cls_obj);


    cls = fs->define_identifier(m, m, "SSLError", NODE_CLASS, 0);
    cls->u.c.n_memb = 2;
    fs->extends_method(cls, fs->cls_error);
}

void define_module(RefNode *m, const FoxStatic *a_fs, FoxGlobal *a_fg)
{
    fs = a_fs;
    fg = a_fg;
    mod_crypt = m;

    define_class(m);
    define_func(m);
}

const char *module_version(const FoxStatic *a_fs)
{
    return "Build at\t" __DATE__ "\nOpenSSL\t" OPENSSL_VERSION_TEXT;
}
