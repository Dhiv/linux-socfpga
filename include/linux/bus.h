#ifndef _LINUX_BUS_H
#define _LINUX_BUS_H

#include <linux/socket.h>

/* 'protocol' to use in socket(AF_BUS, SOCK_SEQPACKET, protocol) */
#define BUS_PROTO_NONE	0
#define BUS_PROTO_DBUS	1
#define BUS_PROTO_MAX	1

#define BUS_PATH_MAX	108

/**
 * struct bus_addr - af_bus address
 * @s_addr: an af_bus address (16-bit prefix + 48-bit client address)
 */
struct bus_addr {
	u64 s_addr;
};


/**
 * struct sockaddr_bus - af_bus socket address
 * @sbus_family: the socket address family
 * @sbus_addr: an af_bus address
 * @sbus_path: a path name
 */
struct sockaddr_bus {
	__kernel_sa_family_t sbus_family;
	struct bus_addr      sbus_addr;
	char sbus_path[BUS_PATH_MAX];
};

#endif /* _LINUX_BUS_H */
