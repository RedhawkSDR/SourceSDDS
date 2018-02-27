/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of REDHAWK rh.SourceSDDS.
 *
 * REDHAWK rh.SourceSDDS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * REDHAWK rh.SourceSDDS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */
/*
 * SocketReader.cpp
 *
 *  Created on: Mar 29, 2016
 *      Author: 
 */

#include "SocketReader.h"
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <fcntl.h>
#include <poll.h>


PREPARE_LOGGING(SocketReader)

/**
 * Creates the socket reader with default options set. You must set the connection info prior to starting the run
 * method.
 */
SocketReader::SocketReader(): m_shuttingDown(false), m_running(false), m_timeout(1), m_pkts_per_read(1), m_socket_buffer_size(-1) {
	memset(&m_multicast_connection, 0, sizeof(m_multicast_connection));
	memset(&m_unicast_connection, 0, sizeof(m_unicast_connection));
	m_host_addr.s_addr = 0;
}


/**
 * Calls the shutdown method and exits.
 */
SocketReader::~SocketReader() {
	shutDown();
}

/**
 * Sets the shut down boolean to true so that during the next pass
 * the socket reader will close the socket and exit cleanly.
 */
void SocketReader::shutDown() {
	LOG_DEBUG(SocketReader, "Shutting down the socket reader");
	m_shuttingDown = true;
	m_running = false;
}

/**
 * Sets the maximum number of UDP packets read per socket read. This cannot be changed
 * once the thread is up and running.
 */
void SocketReader::setPktsPerRead(size_t pkts_per_read) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the packets per read while the socket reader thread is running");
		return;
	}
	m_pkts_per_read = pkts_per_read;
}

/**
 * Returns the maximum number of UDP packets read per socket read.
 */
size_t SocketReader::getPktsPerRead() {
	return m_pkts_per_read;
}

/**
 * Sets up and opens the socket based on the provided interfance, IP, vlan, and port. If there are issues
 * setting up the socket a BadParameterError is thrown and the problem logged.
 * This method cannot be called after the socket reader has started.
 */
void SocketReader::setConnectionInfo(std::string interface, std::string ip, uint16_t vlan, uint16_t port) throw (BadParameterError) {
	LOG_DEBUG(SocketReader, "Setting connection info to Interface: " << interface << " IP: " << ip << " Port: " << port);
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the socket address while the socket reader thread is running");
		return;
	} else if (ip.empty()) {
		std::stringstream ss;
		ss << "IP address is empty, it must be provided.";
		LOG_ERROR(SocketReader, ss.str());
		throw BadParameterError(ss.str());
		return;
	}

	// If we'd already been running on a different socket.
	if (m_multicast_connection.sock) { multicast_close(m_multicast_connection); 	memset(&m_multicast_connection, 0, sizeof(m_multicast_connection)); }
	if (m_unicast_connection.sock) { unicast_close(m_unicast_connection); 			memset(&m_unicast_connection, 0, sizeof(m_unicast_connection)); }

	in_addr_t lowMulti = inet_network("224.0.0.0");
	in_addr_t highMulti = inet_network("239.255.255.255");

	if (vlan) {
		std::stringstream ss;
		ss << interface << "." << vlan;
		interface = ss.str();
	}

	// This throws BAD_PARAM if there are issues....sometimes. Other times it just returns -1.
	if ((inet_network(ip.c_str()) >= lowMulti) && (inet_addr(ip.c_str()) <= highMulti)) {
		// If interface is blank, try using routing table
		if (interface.empty()) {
			interface = getMcastIfaceFromRoutes(ip);
		}
		m_multicast_connection = multicast_client(interface.c_str(), ip.c_str(), port, interface);
	} else {
		m_unicast_connection = unicast_client(interface.c_str(), ip.c_str(), port, interface);
	}

	int socket = (m_multicast_connection.sock != 0) ? (m_multicast_connection.sock) : (m_unicast_connection.sock);

	if (socket < 0) {
		memset(&m_multicast_connection, 0, sizeof(m_multicast_connection));
		memset(&m_unicast_connection, 0, sizeof(m_unicast_connection));

		std::stringstream ss;
		ss << "Could not create socket, please check the parameters provided: Interface: " << interface << " IP: " << ip << " Port: " << port << " VLAN: " << vlan;
		LOG_ERROR(SocketReader, ss.str());
		throw BadParameterError(ss.str());
	}
	LOG_INFO(SocketReader, "Set connection interface: " << interface << " IP: " << ip << " Port: " << port << " VLAN: " << vlan);
	m_interface = interface;
}

/**
 * Sets the target socket buffer size. Cannot be set while the socket reader is running.
 * See the documentation for additional information.
 */
void SocketReader::setSocketBufferSize(int socket_buffer_size) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the socket buffer size while the socket reader thread is running");
		return;
	}
	m_socket_buffer_size = socket_buffer_size;
}

/**
 * Returns the currently set socket buffer size. If the run method has been called,
 * this value should reflect the actual buffer size, otherwise it reflects what the user
 * has provided as the target buffer size. A value of zero indicates that the user has
 * not provided a size and when run the system default will be used.
 */
size_t SocketReader::getSocketBufferSize() {
	return m_socket_buffer_size;
}

/**
 * Returns the currently set socket interface. If the run method has been called,
 * this value should reflect the actual interface, otherwise it reflects what the user
 * has provided as the target interface. A value of empty string indicates that the user
 * has not provided a size and when run a suitable interface will be used.
 */
std::string SocketReader::getInterface() {
	return m_interface;
}

/**
 * This method should be called in its own thread as it will block until the shutDown method is called.
 * Empty buffers will be pulled from the pktbuffer and filled with SDDS packets read from the previously setup socket.
 * If confirmHosts is set, each received packet will be inspected to confirm they all came from the same host.
 * Full packet buffers will be placed back into the pktbuffer's full buffer container for the SDDS to BulkIO
 * processor to consume.
 */
void SocketReader::run(SmartPacketBuffer<SDDSpacket> *pktbuffer, const bool confirmHosts) {
	LOG_DEBUG(SocketReader, "Starting to run");
	pthread_setname_np(pthread_self(), "SocketReader");
	m_shuttingDown = false;
	m_running = true;
	struct pollfd poll_struct[1];
	errno = 0;

	int socket = (m_multicast_connection.sock != 0) ? (m_multicast_connection.sock) : (m_unicast_connection.sock);

	poll_struct[0].events = POLLIN | POLLERR | POLLHUP;
	poll_struct[0].fd = socket;

	// While a blocking socket is more simple / nicer, it forces the thread into a sleep state which can
	// cause a thread context switch. This thread has a need for speed!
	if (not setSocketBlockingEnabled(socket, false)) {
		LOG_ERROR(SocketReader, "Error when setting the socket to non-blocking");
	}

    std::deque<SddsPacketPtr> bufQue;
    int pktsReadThisPass = 0;
    size_t i;

    struct mmsghdr msgs[m_pkts_per_read];
    struct iovec iovecs[m_pkts_per_read];
	sockaddr_in source_addrs[m_pkts_per_read];

    if (m_socket_buffer_size) {
    	if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &m_socket_buffer_size, sizeof(m_socket_buffer_size)) != 0) {
    		LOG_WARN(SocketReader, "Failed to set socket buffer size to the requested size: " << m_socket_buffer_size);
    	}
    }

    socklen_t optlen = sizeof(m_socket_buffer_size);
    getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &m_socket_buffer_size, &optlen);

	memset(msgs, 0, sizeof(msgs));

	// Fill our buffer with free packets
	pktbuffer->pop_empty_buffers(bufQue, m_pkts_per_read);

	for (i = 0; i < m_pkts_per_read; i++) {
		iovecs[i].iov_len          = SDDS_PACKET_SIZE;
		iovecs[i].iov_base         = bufQue[i].get();
		msgs[i].msg_hdr.msg_iov    = &iovecs[i];
		msgs[i].msg_hdr.msg_iovlen = 1;

		if (confirmHosts) {
			msgs[i].msg_hdr.msg_name = &source_addrs[i];
			msgs[i].msg_hdr.msg_namelen = sizeof(sockaddr_in);
		}
	}

	LOG_DEBUG(SocketReader, "Entering socket read while loop");
    while (not m_shuttingDown) {

		// Get packets, the MSG_DONTWAIT does nothing since we already set this to non-blocking socket. Same with the timeout.
		pktsReadThisPass = recvmmsg(socket, msgs, m_pkts_per_read, MSG_DONTWAIT, NULL);

		switch(errno) {
		case 0: // This is the happy path, things went really well.

			// I don't think doing this in a single call would help any, we still need to protect two queues.
			// Push the packets onto the queue that we've received.
			pktbuffer->push_full_buffers(bufQue, pktsReadThisPass);

			// Fill our buffer with free packets
			pktbuffer->pop_empty_buffers(bufQue, m_pkts_per_read);

			// Re-point the iovecs to the new buffers
			// Note that we've added pktsReadThisPass to the top of the bufQue so we only have to repoint the new buffers
			for (i = 0; i < (size_t) pktsReadThisPass; ++i) {
				iovecs[i].iov_base = bufQue[i].get();
			}

			// Its possible that you have two different hosts sending multicast to the same address. This feature was added to
			// aid in debugging situations where you want to know who is missconfigured.
			if (__builtin_expect(confirmHosts,false)) {
				confirmSingleHost(msgs, (size_t) pktsReadThisPass);
			}
			break;

		// Same value as EAGAIN
		case EWOULDBLOCK: // No data was available. Wait for data.
			poll(poll_struct, 1, 100); // 100 ms max wait poll if no data is available.
			errno = 0;
			break;
		case EINTR:
		// Someone is trying to kill us.
			LOG_ERROR(SocketReader, "Socket read was killed by an interrupt. Will stop reading.");
			m_shuttingDown = true;
			m_running = false;
			break;
		default:
			LOG_ERROR(SocketReader, "Received unexpected errno from socket read: " << errno);
			m_shuttingDown = true;
			m_running = false;
			break;
		}
    }

    // Shutting down
	// Don't drop the buffers! Put them back where you found them.
	// XXX If we ever switch to a single producer single consumer no-lock no-wait data structure this may mess things up.
	pktbuffer->recycle_buffers(bufQue);
	m_running = false;

	LOG_DEBUG(SocketReader, "Closing socket");
	if (m_multicast_connection.sock) { multicast_close(m_multicast_connection); 	memset(&m_multicast_connection, 0, sizeof(m_multicast_connection)); }
	if (m_unicast_connection.sock) { unicast_close(m_unicast_connection); 			memset(&m_unicast_connection, 0, sizeof(m_unicast_connection)); }
}

/**
 * Sets the provided file descriptor (assumed to be a socket)
 * to be blocking or non-blocking based on provided blocking boolean.
 * Returns true on success and false otherwise.
 */
bool SocketReader::setSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return false;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
}

/**
 * Runs through the list of received messages and confirms that they all came from
 * the expected host address. The expected host address is initially empty and set
 * on the first call to this method to the host address of the first packet received.
 * In the event that we are receiving packets from multiple hosts, a warning is printed.
 *
 */
void SocketReader::confirmSingleHost(struct mmsghdr msgs[], size_t len) {

	for (size_t i = 0; i < len; ++i) {
		sockaddr_in * rcv_host_struct = reinterpret_cast<sockaddr_in *>(msgs[i].msg_hdr.msg_name);

		if (rcv_host_struct->sin_addr.s_addr != m_host_addr.s_addr) {
			if (m_host_addr.s_addr != 0) {
				//XXX: Do not combine these into a single log statement. The inet_ntoa returns a pointer to an internal array containing the string so if you call it twice
				// on the same line it will overwrite itself, display the same value twice in the log statement, and cause the developer debugging to question their sanity.
				LOG_WARN(SocketReader, "Expected packets to come from: " << inet_ntoa(m_host_addr));
				LOG_WARN(SocketReader, "Received packet from: " << inet_ntoa(rcv_host_struct->sin_addr));
			}

			m_host_addr = rcv_host_struct->sin_addr;
		}
	}
}

/**
 * Selects a network interface that has a route for the multicast group passed in as
 * ac argument. If no multicast group is specified, 224.0.0.0 is used to select the
 * default multicast route. If no other routes match, the interface with the default
 * gateway will be selected. The selected interface is returned.
 */
std::string SocketReader::getMcastIfaceFromRoutes(std::string group) {

	FILE* fp;
	char buffer[1024*1024];  // Too big?
	size_t bytes_read;
	std::stringstream routes;
	std::string iface, trash, iface_match = "";
	in_addr_t dest, mask, mask_match = 0;

	// Convert group to hex representation
	in_addr_t group_hex = inet_addr(group.c_str());

	// Read the entire contents of /proc/net/route into the buffer.
	// Expects format: Iface	Destination	Gateway 	Flags	RefCnt	Use	Metric	Mask ...
	fp = fopen ("/proc/net/route", "r");
	if (fp == NULL) {
		RH_NL_WARN("SourceSDDSUtils", "Failed to open /proc/net/route");
		return "";
	}
	bytes_read = fread (buffer, 1, sizeof (buffer), fp);
	fclose (fp);

	/* Bail if read failed.  */
	if (bytes_read == 0) {
		RH_NL_WARN("SourceSDDSUtils", "Failed to read /proc/net/route");
		return "";
	}

	// write to stringstream
	routes.write(buffer, bytes_read);
	RH_NL_DEBUG("SourceSDDSUtils", "Routes: ["<<routes.str()<<"]");

    // Parse the results, using format below:
	//	Iface	Destination	Gateway 	Flags	RefCnt	Use	Metric	Mask		MTU	Window	IRTT
	//	em2		000000E0	00000000	0001	0		0	0		000000F0	0	0		0 ## This is the default multicast route
	//	em2		0067A8EA	00000000	0001	0		0	0		00FFFFFF	0	0		0 ## This is a class C multicast route
	//	em4		0068A8EA	00000000	0001	0		0	0		00FFFFFF	0	0		0 ## This is another class C mcast route
	// logic: bool match = (<Destination> == (inet_addr(ip.c_str()) & <Mask>))
	// And matches with greater mask take priority

	routes.ignore(bytes_read, '\n'); // skip title row
	for (routes >> iface; !routes.eof(); routes >> iface ){
		routes >> std::hex >> dest >> trash >> trash >> trash >> trash >> trash >> mask;
	    RH_NL_DEBUG("SourceSDDSUtils", "iface="<<iface
	    		<<"  dest=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<std::uppercase<<dest<<std::nouppercase
				<<"  mask=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<std::uppercase<<mask<<std::nouppercase);
		if (mask >= mask_match && (dest == (group_hex & mask))) {
			RH_NL_DEBUG("SourceSDDSUtils", "Found match: iface="<<iface);
			iface_match = iface;
            mask_match = mask;
		}
		routes.ignore(bytes_read, '\n'); // skip remainder of row
	}
	RH_NL_DEBUG("SourceSDDSUtils", "Determined "<<iface_match<<" is used to route "<<group<<" using /proc/net/route");
	return iface_match;
}
