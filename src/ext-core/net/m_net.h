#ifndef _M_NET_H_
#define _M_NET_H_

#include "fox.h"
#include "fox_io.h"


enum {
	INDEX_IFADDR_NAME,
	INDEX_IFADDR_ADDR,
	INDEX_IFADDR_MASK,
	INDEX_IFADDR_NUM,
};

typedef struct RefSockAddr RefSockAddr;

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#define gai_strerror_fox(e) (winsock_strerror())
#define closesocket_fox closesocket
int send_fox(int fd, const char *buf, int size);
int recv_fox(int fd, char *buf, int size);
void initialize_winsock(void);
char *winsock_strerror(void);
void shutdown_winsock(void);

#else

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#define gai_strerror_fox(e) (gai_strerror(e))
#define closesocket_fox close
#define send_fox write
#define recv_fox read

#endif

RefSockAddr *new_refsockaddr(struct sockaddr *sa);
int getifaddrs_sub(RefArray *ra);

#ifdef DEFINE_GLOBALS
#define extern
#endif

extern const FoxStatic *fs;
extern FoxGlobal *fg;
extern RefNode *cls_ifaddr;

#ifdef DEFINE_GLOBALS
#undef extern
#endif


#endif /* _M_NET_H_ */
