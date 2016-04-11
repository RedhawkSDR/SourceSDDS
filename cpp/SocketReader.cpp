/*
 * SocketReader.cpp
 *
 *  Created on: Mar 29, 2016
 *      Author: ylbagou
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


PREPARE_LOGGING(SocketReader)

SocketReader::SocketReader(): m_shuttingDown(false), m_running(false), m_timeout(1), m_pkts_per_read(1), m_socket_buffer_size(-1) {
	m_multicast_connection = {0};
	m_unicast_connection = {0};
}

SocketReader::~SocketReader() {}

void SocketReader::shutDown() {
	LOG_DEBUG(SocketReader, "Shutting down the socket reader");
	m_shuttingDown = true;
	m_running = false;
	LOG_DEBUG(SocketReader, "Closing socket");
	if (m_multicast_connection.sock) { multicast_close(m_multicast_connection); 	m_multicast_connection = {0}; }
	if (m_unicast_connection.sock) { unicast_close(m_unicast_connection); 			m_unicast_connection = {0}; }
}

void SocketReader::setTimeout(int timeout) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the timeout while the socket reader thread is running");
		return;
	}

	if (m_timeout >= MAX_ALLOWED_TIMEOUT) {
		LOG_WARN(SocketReader, "Socket timeout is >= " << MAX_ALLOWED_TIMEOUT << " secs. This may cause issues when the component is stopped.");
	}

	m_timeout = timeout;
}

void SocketReader::setPktsPerRead(size_t pkts_per_read) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the packets per read while the socket reader thread is running");
		return;
	}
	m_pkts_per_read = pkts_per_read;
}

size_t SocketReader::getPktsPerRead() {
	return m_pkts_per_read;
}

void SocketReader::setConnectionInfo(std::string interface, std::string ip, uint16_t vlan, uint16_t port) throw (BadParameterError) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the socket address while the socket reader thread is running");
		return;
	} else if (ip.empty() || interface.empty()) {
		LOG_ERROR(SocketReader, "IP or interface was empty when trying to setup the socket connection.")
	}

	// If we'd already been running on a different socket.
	if (m_multicast_connection.sock) { multicast_close(m_multicast_connection); 	m_multicast_connection = {0}; }
	if (m_unicast_connection.sock) { unicast_close(m_unicast_connection); 			m_unicast_connection = {0}; }

	in_addr_t lowMulti = inet_network("224.0.0.0");
	in_addr_t highMulti = inet_network("239.255.255.250");


	// Looks like we're doing a multicast port. Create multicast client.
	// This throws BAD_PARAM if there are issues.
	if ((inet_network(ip.c_str()) > lowMulti) && (inet_addr(ip.c_str()) < highMulti)) {
		m_multicast_connection = multicast_client(interface.c_str(), ip.c_str(), port);
	} else {
		m_unicast_connection = unicast_client(interface.c_str(), ip.c_str(), port);
	}
}

void SocketReader::setSocketBufferSize(int socket_buffer_size) {
	m_socket_buffer_size = socket_buffer_size;
}

size_t SocketReader::getSocketBufferSize() {
	return m_socket_buffer_size;
}

/**
 * Called in a boost thread to get unused buffers from packet buffer, fill with packets, place back into buffer.
 */
// TODO: Dont pass a pointer to the packet buffer that is so ugly
void SocketReader::run(SmartPacketBuffer<SDDSpacket> *pktbuffer) {
	LOG_DEBUG(SocketReader, "Starting to run");
	m_shuttingDown = false;
	m_running = true;
	int socket = (m_multicast_connection.sock != 0) ? (m_multicast_connection.sock) : (m_unicast_connection.sock);


    std::deque<SddsPacketPtr> bufQue;
    int retval;
    size_t i;

    struct mmsghdr msgs[m_pkts_per_read];
    struct iovec iovecs[m_pkts_per_read];

    struct timespec timeout;

    if (m_socket_buffer_size) {
    	if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &m_socket_buffer_size, sizeof(m_socket_buffer_size)) != 0) {
    		LOG_WARN(SocketReader, "Failed to set socket buffer size to the requested size: " << m_socket_buffer_size);
    	}
    }

    // This is the socket connection time out, if we do not receive data in this amount of time we break out of
    // the recv call.
    struct timeval tv;
    tv.tv_sec = m_timeout;
    tv.tv_usec = 0;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv, sizeof(struct timeval));
    ////


    socklen_t optlen = sizeof(m_socket_buffer_size);
    getsockopt(socket, SOL_SOCKET, SO_RCVBUF, &m_socket_buffer_size, &optlen);

	memset(msgs, 0, sizeof(msgs));
	for (i = 0; i < m_pkts_per_read; i++) {
		iovecs[i].iov_len          = SDDS_PACKET_SIZE;
		msgs[i].msg_hdr.msg_iov    = &iovecs[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
	}

	LOG_DEBUG(SocketReader, "Entering socket read while loop");
    while (not m_shuttingDown) {

		// Fill our buffer with free packets
    	pktbuffer->pop_empty_buffers(bufQue, m_pkts_per_read);

		// Repoint the iovecs to the new buffers
		for (i = 0; i < m_pkts_per_read; i++) {
			iovecs[i].iov_base         = bufQue[i].get();
		}

		// Reset the timeout
		timeout.tv_sec = m_timeout;
		timeout.tv_nsec = 0;

		// Get packets
		retval = recvmmsg(socket, msgs, m_pkts_per_read, MSG_WAITFORONE, &timeout);

		if (retval == -1) {
			if (m_shuttingDown) {
				// Don't drop the buffers! Put them back where you found them.
				// XXX If we ever switch to a single producer single consumer no-lock no-wait data structure this may mess things up.
				pktbuffer->recycle_buffers(bufQue);
				m_running = false;
				break;
			} // TODO: Considering using a boost::interrupt point instead
			continue;
		}

		// Push the packets onto the queue that we've received.
		pktbuffer->push_full_buffers(bufQue, retval);
    }
}
