// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

// Copyright (c) 2001-2008 International Computer Science Institute
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

// $XORP: xorp/fea/data_plane/io/io_ip_socket.hh,v 1.9 2008/01/03 22:59:44 pavlin Exp $


#ifndef __FEA_DATA_PLANE_IO_IO_IP_SOCKET_HH__
#define __FEA_DATA_PLANE_IO_IO_IP_SOCKET_HH__


//
// I/O IP raw socket support.
//

#include "libxorp/xorp.h"
#include "libxorp/eventloop.hh"

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include "fea/io_ip.hh"


/**
 * @short A base class for I/O IP raw socket communication.
 * 
 * Each protocol 'registers' for I/O and gets assigned one object
 * of this class.
 */
class IoIpSocket : public IoIp {
public:
    /**
     * Constructor for a given address family and protocol.
     * 
     * @param fea_data_plane_manager the corresponding data plane manager
     * (@ref FeaDataPlaneManager).
     * @param iftree the interface tree to use.
     * @param family the address family (AF_INET or AF_INET6 for IPv4 and IPv6
     * respectively).
     * @param ip_protocol the IP protocol number (IPPROTO_*).
     */
    IoIpSocket(FeaDataPlaneManager& fea_data_plane_manager,
	       const IfTree& iftree, int family, uint8_t ip_protocol);

    /**
     * Virtual destructor.
     */
    virtual ~IoIpSocket();

    /**
     * Start operation.
     * 
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		start(string& error_msg);

    /**
     * Stop operation.
     * 
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		stop(string& error_msg);

    /**
     * Set the default TTL (or hop-limit in IPv6) for the outgoing multicast
     * packets.
     * 
     * @param ttl the desired IP TTL (a.k.a. hop-limit in IPv6) value.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		set_multicast_ttl(int ttl, string& error_msg);

    /**
     * Enable/disable multicast loopback when transmitting multicast packets.
     *
     * If the multicast loopback is enabled, a transmitted multicast packet
     * will be delivered back to this host (assuming the host is a member of
     * the same multicast group).
     *
     * @param is_enabled if true, enable the loopback, otherwise disable it.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		enable_multicast_loopback(bool is_enabled, string& error_msg);

    /**
     * Set default interface for transmitting multicast packets.
     * 
     * @param if_name the name of the interface that would become the default
     * multicast interface.
     * @param vif_name the name of the vif that would become the default
     * multicast interface.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		set_default_multicast_interface(const string& if_name,
						const string& vif_name,
						string& error_msg);

    /**
     * Join a multicast group on an interface.
     * 
     * @param if_name the name of the interface to join the multicast group.
     * @param vif_name the name of the vif to join the multicast group.
     * @param group the multicast group to join.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		join_multicast_group(const string& if_name,
				     const string& vif_name,
				     const IPvX& group,
				     string& error_msg);
    
    /**
     * Leave a multicast group on an interface.
     * 
     * @param if_name the name of the interface to leave the multicast group.
     * @param vif_name the name of the vif to leave the multicast group.
     * @param group the multicast group to leave.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		leave_multicast_group(const string& if_name,
				      const string& vif_name,
				      const IPvX& group,
				      string& error_msg);

    /**
     * Send a raw IP packet.
     *
     * @param if_name the interface to send the packet on. It is essential for
     * multicast. In the unicast case this field may be empty.
     * @param vif_name the vif to send the packet on. It is essential for
     * multicast. In the unicast case this field may be empty.
     * @param src_address the IP source address.
     * @param dst_address the IP destination address.
     * @param ip_ttl the IP TTL (hop-limit). If it has a negative value,
     * the TTL will be set internally before transmission.
     * @param ip_tos the Type Of Service (Diffserv/ECN bits for IPv4 or
     * IP traffic class for IPv6). If it has a negative value, the TOS will be
     * set internally before transmission.
     * @param ip_router_alert if true, then add the IP Router Alert option to
     * the IP packet.
     * @param ip_internet_control if true, then this is IP control traffic.
     * @param ext_headers_type a vector of integers with the types of the
     * optional IPv6 extention headers.
     * @param ext_headers_payload a vector of payload data, one for each
     * optional IPv6 extention header. The number of entries must match
     * ext_headers_type.
     * @param payload the payload, everything after the IP header and options.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		send_packet(const string&	if_name,
			    const string&	vif_name,
			    const IPvX&		src_address,
			    const IPvX&		dst_address,
			    int32_t		ip_ttl,
			    int32_t		ip_tos,
			    bool		ip_router_alert,
			    bool		ip_internet_control,
			    const vector<uint8_t>& ext_headers_type,
			    const vector<vector<uint8_t> >& ext_headers_payload,
			    const vector<uint8_t>& payload,
			    string&		error_msg);

    /**
     * Get the file descriptor for receiving protocol messages.
     *
     * @return a reference to the file descriptor for receiving protocol
     * messages.
     */
    XorpFd& protocol_fd_in() { return (_proto_socket_in); }

private:
    /**
     * Open the protocol sockets.
     * 
     * The protocol sockets are specific to the particular protocol of
     * this entry.
     * 
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		open_proto_sockets(string& error_msg);

    /**
     * Close the protocol sockets.
     * 
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		close_proto_sockets(string& error_msg);

    /**
     * Enable/disable the "Header Included" option (for IPv4) on the outgoing
     * protocol socket.
     * 
     * If enabled, the IP header of a raw packet should be created
     * by the application itself, otherwise the kernel will build it.
     * Note: used only for IPv4.
     * In RFC-3542, IPV6_PKTINFO has similar functions,
     * but because it requires the interface index and outgoing address,
     * it is of little use for our purpose. Also, in RFC-2292 this option
     * was a flag, so for compatibility reasons we better not set it
     * here; instead, we will use sendmsg() to specify the header's field
     * values.
     * 
     * @param is_enabled if true, enable the option, otherwise disable it.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		enable_ip_hdr_include(bool is_enabled, string& error_msg);

    /**
     * Enable/disable receiving information about a packet received on the
     * incoming protocol socket.
     * 
     * If enabled, values such as interface index, destination address and
     * IP TTL (a.k.a. hop-limit in IPv6), and hop-by-hop options will be
     * received as well.
     * 
     * @param is_enabled if true, set the option, otherwise reset it.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		enable_recv_pktinfo(bool is_enabled, string& error_msg);

    /**
     * Read data from a protocol socket, and then call the appropriate protocol
     * module to process it.
     *
     * This is called as a IoEventCb callback.
     * @param fd file descriptor that with event caused this method to be
     * called.
     * @param type the event type.
     */
    void	proto_socket_read(XorpFd fd, IoEventType type);

    /**
     * Transmit a packet on a protocol socket.
     *
     * @param ifp the interface to send the packet on.
     * @param vifp the vif to send the packet on.
     * @param src_address the IP source address.
     * @param dst_address the IP destination address.
     * @param error_msg the error message (if error).
     * @return XORP_OK on success, otherwise XORP_ERROR.
     */
    int		proto_socket_transmit(const IfTreeInterface* ifp,
				      const IfTreeVif*	vifp,
				      const IPvX&	src_address,
				      const IPvX&	dst_address,
				      string&		error_msg);

    // Private state
    XorpFd	_proto_socket_in;    // The socket to receive protocol message
    XorpFd	_proto_socket_out;   // The socket to end protocol message
    bool	_is_ip_hdr_included; // True if IP header is included on send
    uint16_t	_ip_id;		     // IPv4 Header ID

    uint8_t*	_rcvbuf;	// Data buffer for receiving
    uint8_t*	_sndbuf;	// Data buffer for sending
    uint8_t*	_rcvcmsgbuf;	// Control recv info (IPv6 only)
    uint8_t*	_sndcmsgbuf;	// Control send info (IPv6 only)

    struct iovec	_rcviov[1]; // The scatter/gatter array for receiving
    struct iovec	_sndiov[1]; // The scatter/gatter array for sending

#ifndef HOST_OS_WINDOWS
    struct msghdr	_rcvmh;	// The msghdr structure used by recvmsg()
    struct msghdr	_sndmh;	// The msghdr structure used by sendmsg()
    struct sockaddr_in	_from4;	// The source addr of recvmsg() msg (IPv4)
    struct sockaddr_in  _to4;	// The dest.  addr of sendmsg() msg (IPv4)
#ifdef HAVE_IPV6
    struct sockaddr_in6	_from6;	// The source addr of recvmsg() msg (IPv6)
    struct sockaddr_in6	_to6;	// The dest.  addr of sendmsg() msg (IPv6)
#endif
#endif // ! HOST_OS_WINDOWS
};

#endif // __FEA_DATA_PLANE_IO_IO_IP_SOCKET_HH__