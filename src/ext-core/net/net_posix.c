#include "m_net.h"
#include <sys/socket.h>
#include <ifaddrs.h>


int getifaddrs_sub(RefArray *ra)
{
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) {
        fs->throw_errorf(fs->mod_io, "SocketError", "getifaddrs failed");
        return FALSE;
    }
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        Value *v;
        Ref *r;
        if (ifa->ifa_addr == NULL) {
            continue;
        }
        v = fs->refarray_push(ra);
        r = fs->ref_new(cls_ifaddr);
        *v = vp_Value(r);

        r->v[INDEX_IFADDR_NAME] = fs->cstr_Value(NULL, ifa->ifa_name, -1);
        if (ifa->ifa_netmask != NULL) {
            RefSockAddr *addr = new_refsockaddr(ifa->ifa_addr, TRUE);
            addr->mask_bits = sockaddr_get_bit_count(ifa->ifa_netmask);
            r->v[INDEX_IFADDR_ADDR] = vp_Value(addr);
        } else {
            r->v[INDEX_IFADDR_ADDR] = vp_Value(new_refsockaddr(ifa->ifa_addr, FALSE));
        }
    }
    freeifaddrs(ifaddr);

    return TRUE;
}
