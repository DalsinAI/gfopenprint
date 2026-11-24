/* Copyright (c) 2026 AmiCradle Team. SPDX-License-Identifier: MIT */
/* GFOS's host as C:OAPDiscover meets it (BL-134): a UDP bind to 0.0.0.0
 * is kept on 127.0.0.1 unless the network is in LAN address mode. Every
 * mDNS query is written to $GFOS_BIND_LOG with the address it leaves from,
 * and is not sent, so no datagram reaches the LAN. */
#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
int bind(int s, const struct sockaddr *to, socklen_t len)
{
    int (*real_bind)(int, const struct sockaddr *, socklen_t) = (int (*)(int, const struct sockaddr *, socklen_t))dlsym(RTLD_NEXT, "bind");
    struct sockaddr_in a;
    int type = 0;
    socklen_t tl = sizeof(type);
    if (to && len >= sizeof(a) && to->sa_family == AF_INET &&
        !getsockopt(s, SOL_SOCKET, SO_TYPE, &type, &tl) && type == SOCK_DGRAM) {
        a = *(const struct sockaddr_in *)to;
        if (a.sin_addr.s_addr == htonl(INADDR_ANY)) {
            a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            return real_bind(s, (struct sockaddr *)&a, sizeof(a));
        }
    }
    return real_bind(s, to, len);
}
ssize_t sendto(int s, const void *buf, size_t n, int flags, const struct sockaddr *to, socklen_t len)
{
    ssize_t (*real_sendto)(int, const void *, size_t, int, const struct sockaddr *, socklen_t) =
        (ssize_t (*)(int, const void *, size_t, int, const struct sockaddr *, socklen_t))dlsym(RTLD_NEXT, "sendto");
    const struct sockaddr_in *a = (const struct sockaddr_in *)to;
    if (to && len >= sizeof(*a) && a->sin_family == AF_INET &&
        a->sin_addr.s_addr == htonl(0xe00000fb) && a->sin_port == htons(5353)) {
        struct sockaddr_in me;
        socklen_t ml = sizeof(me);
        const char *path = getenv("GFOS_BIND_LOG");
        FILE *f = path ? fopen(path, "a") : NULL;
        if (getsockname(s, (struct sockaddr *)&me, &ml))
            me.sin_addr.s_addr = 0;
        if (f) {
            fprintf(f, "mdns-query-from %s\n", inet_ntoa(me.sin_addr));
            fclose(f);
        }
        return (ssize_t)n;
    }
    return real_sendto(s, buf, n, flags, to, len);
}
