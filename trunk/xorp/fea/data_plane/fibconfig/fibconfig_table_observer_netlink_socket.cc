// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

// Copyright (c) 2001-2007 International Computer Science Institute
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software")
// to deal in the Software without restriction, subject to the conditions
// listed in the XORP LICENSE file. These conditions include: you must
// preserve this copyright notice, and you cannot mention the copyright
// holders in advertising related to the Software without their permission.
// The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
// notice is a summary of the XORP LICENSE file; the license in that file is
// legally binding.

#ident "$XORP: xorp/fea/forwarding_plane/fibconfig/fibconfig_table_observer_netlink_socket.cc,v 1.6 2007/04/30 23:40:32 pavlin Exp $"

#include "fea/fea_module.h"

#include "libxorp/xorp.h"
#include "libxorp/xlog.h"
#include "libxorp/debug.h"

#ifdef HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif
#ifdef HAVE_LINUX_RTNETLINK_H
#include <linux/rtnetlink.h>
#endif

#include "fea/fibconfig.hh"
#include "fea/fibconfig_table_observer.hh"


//
// Observe whole-table information change about the unicast forwarding table.
//
// E.g., if the forwarding table has changed, then the information
// received by the observer would NOT specify the particular entry that
// has changed.
//
// The mechanism to observe the information is netlink(7) sockets.
//


FibConfigTableObserverNetlink::FibConfigTableObserverNetlink(FibConfig& fibconfig)
    : FibConfigTableObserver(fibconfig),
      NetlinkSocket(fibconfig.eventloop()),
      NetlinkSocketObserver(*(NetlinkSocket *)this)
{
#ifdef HAVE_NETLINK_SOCKETS
    fibconfig.register_fibconfig_table_observer_primary(this);
#endif
}

FibConfigTableObserverNetlink::~FibConfigTableObserverNetlink()
{
    string error_msg;

    if (stop(error_msg) != XORP_OK) {
	XLOG_ERROR("Cannot stop the netlink(7) sockets mechanism to observe "
		   "whole forwarding table from the underlying "
		   "system: %s",
		   error_msg.c_str());
    }
}

int
FibConfigTableObserverNetlink::start(string& error_msg)
{
#ifndef HAVE_NETLINK_SOCKETS
    error_msg = c_format("The netlink(7) mechanism to observe "
			 "whole forwarding table from the "
			 "underlying system is not supported");
    XLOG_UNREACHABLE();
    return (XORP_ERROR);

#else // HAVE_NETLINK_SOCKETS

    uint32_t nl_groups = 0;

    if (_is_running)
	return (XORP_OK);

    //
    // Listen to the netlink multicast group for IPv4 routing entries
    //
    if (fibconfig().have_ipv4())
	nl_groups |= RTMGRP_IPV4_ROUTE;

#ifdef HAVE_IPV6
    //
    // Listen to the netlink multicast group for IPv6 routing entries
    //
    if (fibconfig().have_ipv6())
	nl_groups |= RTMGRP_IPV6_ROUTE;
#endif // HAVE_IPV6

    //
    // Set the netlink multicast groups to listen for on the netlink socket
    //
    NetlinkSocket::set_nl_groups(nl_groups);

    if (NetlinkSocket::start(error_msg) < 0)
	return (XORP_ERROR);

    _is_running = true;

    return (XORP_OK);
#endif // HAVE_NETLINK_SOCKETS
}
    
int
FibConfigTableObserverNetlink::stop(string& error_msg)
{
    if (! _is_running)
	return (XORP_OK);

    if (NetlinkSocket::stop(error_msg) < 0)
	return (XORP_ERROR);

    _is_running = false;

    return (XORP_OK);
}

void
FibConfigTableObserverNetlink::receive_data(const vector<uint8_t>& buffer)
{
    list<FteX> fte_list;

    //
    // Get the IPv4 routes
    //
    if (fibconfig().have_ipv4()) {
	FibConfigTableGetNetlink::parse_buffer_netlink_socket(
	    AF_INET,
	    fibconfig().iftree(),
	    fte_list,
	    buffer,
	    false);
	if (! fte_list.empty()) {
	    fibconfig().propagate_fib_changes(fte_list, this);
	    fte_list.clear();
	}
    }

#ifdef HAVE_IPV6
    //
    // Get the IPv6 routes
    //
    if (fibconfig().have_ipv6()) {
	FibConfigTableGetNetlink::parse_buffer_netlink_socket(AF_INET6,
							      fibconfig().iftree(),
							      fte_list,
							      buffer,
							      false);
	if (! fte_list.empty()) {
	    fibconfig().propagate_fib_changes(fte_list, this);
	    fte_list.clear();
	}
    }
#endif // HAVE_IPV6
}

void
FibConfigTableObserverNetlink::nlsock_data(const vector<uint8_t>& buffer)
{
    receive_data(buffer);
}