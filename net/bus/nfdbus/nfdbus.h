/*
 * nfdbus.h  Netfilter module for AF_BUS/BUS_PROTO_DBUS.
 *
 * Copyright (C) 2012  Collabora Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef NETFILTER_DBUS_H
#define NETFILTER_DBUS_H

#include <linux/types.h>
#include <linux/bus.h>

#define NFDBUS_CMD_ADDMATCH        0x01
#define NFDBUS_CMD_REMOVEMATCH     0x02
#define NFDBUS_CMD_REMOVEALLMATCH  0x03

struct nfdbus_nl_cfg_req {
	__u32 cmd;
	__u32 len;
	struct sockaddr_bus addr;
	__u64 pad;
	unsigned char data[0];
};

struct nfdbus_nl_cfg_reply {
	__u32 ret_code;
};

#endif /* NETFILTER_DBUS_H */
