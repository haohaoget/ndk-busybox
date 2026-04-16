/* vi: set sw=4 ts=4: */
/*
 * ipmonitor.c		"ip monitor".
 *
 * Authors:	Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "ip_common.h"  /* #include "libbb.h" is inside */
#include "libiproute/utils.h"

// 补齐可能缺失的宏
#ifndef RTMGRP_LINK
#define RTMGRP_LINK 1
#endif
#ifndef RTMGRP_NEIGH
#define RTMGRP_NEIGH 4
#endif
#ifndef RTMGRP_IPV4_IFADDR
#define RTMGRP_IPV4_IFADDR 0x10
#endif
#ifndef RTMGRP_IPV4_MROUTE
#define RTMGRP_IPV4_MROUTE 0x20
#endif
#ifndef RTMGRP_IPV4_ROUTE
#define RTMGRP_IPV4_ROUTE 0x40
#endif
#ifndef RTMGRP_IPV4_RULE
#define RTMGRP_IPV4_RULE 0x80
#endif
#ifndef RTMGRP_IPV6_IFADDR
#define RTMGRP_IPV6_IFADDR 0x100
#endif
#ifndef RTMGRP_IPV6_MROUTE
#define RTMGRP_IPV6_MROUTE 0x200
#endif
#ifndef RTMGRP_IPV6_ROUTE
#define RTMGRP_IPV6_ROUTE 0x400
#endif
#ifndef RTMGRP_IPV6_RULE
#define RTMGRP_IPV6_RULE 0x1000
#endif

#define IPMON_LLINK		(1<<0)
#define IPMON_LADDR		(1<<1)
#define IPMON_LROUTE	(1<<2)
#define IPMON_LMROUTE	(1<<3)
#define IPMON_LNEIGH	(1<<4)
#define IPMON_LRULE		(1<<5)
#define IPMON_L_ALL		(~0)

static int accept_msg(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	switch (n->nlmsg_type) {
	case RTM_NEWROUTE:
	case RTM_DELROUTE:
		print_route(who, n, arg);
		return 0;
	case RTM_NEWLINK:
	case RTM_DELLINK:
		ll_remember_index(who, n, NULL);
		print_linkinfo(who, n, arg);
		return 0;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		print_addrinfo(who, n, arg);
		return 0;
	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
		// if (preferred_family && (struct ndmsg*)NLMSG_DATA(n)->ndm_family != preferred_family) return 0;
		print_neigh(who, n, arg);
		return 0;
	case RTM_NEWRULE:
	case RTM_DELRULE:
		print_rule(who, n, arg);
		return 0;
	case NLMSG_ERROR:
	case NLMSG_NOOP:
	case NLMSG_DONE:
		return 0;
	default:
		return 0;
	}
	return 0;
}

int FAST_FUNC do_ipmonitor(char **argv)
{
	struct rtnl_handle rth;
	char *buf;
	int fd;
	unsigned groups = 0;
	unsigned lmask = 0;

	while (*argv) {
		if (strcmp(*argv, "all") == 0) {
			lmask = IPMON_L_ALL;
		} else if (strcmp(*argv, "link") == 0) {
			lmask |= IPMON_LLINK;
		} else if (strcmp(*argv, "address") == 0 || strcmp(*argv, "addr") == 0) {
			lmask |= IPMON_LADDR;
		} else if (strcmp(*argv, "route") == 0 || strcmp(*argv, "r") == 0) {
			lmask |= IPMON_LROUTE;
		} else if (strcmp(*argv, "mroute") == 0) {
			lmask |= IPMON_LMROUTE;
		} else if (strcmp(*argv, "neigh") == 0 || strcmp(*argv, "n") == 0) {
			lmask |= IPMON_LNEIGH;
		} else if (strcmp(*argv, "rule") == 0 || strcmp(*argv, "ru") == 0) {
			lmask |= IPMON_LRULE;
		} else {
			invarg_1_to_2(*argv, "ip monitor");
		}
		argv++;
	}

	if (lmask == 0)
		lmask = IPMON_L_ALL;

	if (lmask & IPMON_LLINK)
		groups |= RTMGRP_LINK;
	if (lmask & IPMON_LADDR) {
		if (!preferred_family || preferred_family == AF_INET)
			groups |= RTMGRP_IPV4_IFADDR;
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= RTMGRP_IPV6_IFADDR;
	}
	if (lmask & IPMON_LROUTE) {
		if (!preferred_family || preferred_family == AF_INET)
			groups |= RTMGRP_IPV4_ROUTE;
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= RTMGRP_IPV6_ROUTE;
	}
	if (lmask & IPMON_LMROUTE) {
		if (!preferred_family || preferred_family == AF_INET)
			groups |= RTMGRP_IPV4_MROUTE;
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= RTMGRP_IPV6_MROUTE;
	}
	if (lmask & IPMON_LNEIGH) {
		groups |= RTMGRP_NEIGH;
	}
	if (lmask & IPMON_LRULE) {
		if (!preferred_family || preferred_family == AF_INET)
			groups |= RTMGRP_IPV4_RULE;
		if (!preferred_family || preferred_family == AF_INET6)
			groups |= RTMGRP_IPV6_RULE;
	}

	fd = xsocket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	memset(&rth, 0, sizeof(rth));
	rth.fd = fd;
	rth.local.nl_family = AF_NETLINK;
	rth.local.nl_groups = groups;
	xbind(fd, (struct sockaddr*)&rth.local, sizeof(rth.local));
	bb_getsockname(fd, (struct sockaddr*)&rth.local, sizeof(rth.local));
	rth.seq = time(NULL);

	ll_init_map(&rth);

	buf = xmalloc(8 * 1024);
	while (1) {
		struct sockaddr_nl nladdr;
		struct iovec iov = { buf, 8 * 1024 };
		struct msghdr msg = {
			.msg_name = (void*)&nladdr,
			.msg_namelen = sizeof(nladdr),
			.msg_iov = &iov,
			.msg_iovlen = 1,
			.msg_control = NULL,
			.msg_controllen = 0,
			.msg_flags = 0
		};
		int status;
		struct nlmsghdr *h;

		status = recvmsg(fd, &msg, 0);

		if (status < 0) {
			if (errno == EINTR)
				continue;
			bb_simple_perror_msg("OVERRUN");
			continue;
		}
		if (status == 0) {
			bb_simple_error_msg("EOF on netlink");
			break;
		}

		for (h = (struct nlmsghdr*)buf; status >= (int)sizeof(*h); ) {
			int len = h->nlmsg_len;
			int l = len - sizeof(*h);

			if (l < 0 || len > status) {
				if (msg.msg_flags & MSG_TRUNC) {
					bb_simple_error_msg("truncated message");
					break;
				}
				bb_error_msg_and_die("malformed message: len=%d!", len);
			}

			accept_msg(&nladdr, h, stdout);

			status -= NLMSG_ALIGN(len);
			h = (struct nlmsghdr*)((char*)h + NLMSG_ALIGN(len));
		}
		fflush_all();
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		free(buf);

	return 0;
}
