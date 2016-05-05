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
#include <fcntl.h>
#include <poll.h>

PREPARE_LOGGING(SocketReader)

SocketReader::SocketReader(): m_shuttingDown(false), m_running(false), m_timeout(1), m_pkts_per_read(1), m_socket_buffer_size(-1) {
	memset(&m_multicast_connection, 0, sizeof(m_multicast_connection));
	memset(&m_unicast_connection, 0, sizeof(m_unicast_connection));
}

SocketReader::~SocketReader() {
	shutDown();
}

void SocketReader::shutDown() {
	LOG_DEBUG(SocketReader, "Shutting down the socket reader");
	m_shuttingDown = true;
	m_running = false;
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
	LOG_INFO(SocketReader, "Setting connection info to Interface: " << interface << " IP: " << ip << " Port: " << port);
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the socket address while the socket reader thread is running");
		return;
	} else if (ip.empty() || interface.empty()) {
		LOG_ERROR(SocketReader, "IP or interface was empty when trying to setup the socket connection.")
	}

	// If we'd already been running on a different socket.
	if (m_multicast_connection.sock) { multicast_close(m_multicast_connection); 	memset(&m_multicast_connection, 0, sizeof(m_multicast_connection)); }
	if (m_unicast_connection.sock) { unicast_close(m_unicast_connection); 			memset(&m_unicast_connection, 0, sizeof(m_unicast_connection)); }

	in_addr_t lowMulti = inet_network("224.0.0.0");
	in_addr_t highMulti = inet_network("239.255.255.250");

	if (vlan) {
		std::stringstream ss;
		ss << interface << "." << vlan;
		interface = ss.str();
	}

	// Looks like we're doing a multicast port. Create multicast client.
	// This throws BAD_PARAM if there are issues....sometimes. Other times it just returns -1.
	if ((inet_network(ip.c_str()) > lowMulti) && (inet_addr(ip.c_str()) < highMulti)) {
		m_multicast_connection = multicast_client(interface.c_str(), ip.c_str(), port);
	} else {
		m_unicast_connection = unicast_client(interface.c_str(), ip.c_str(), port);
	}

	int socket = (m_multicast_connection.sock != 0) ? (m_multicast_connection.sock) : (m_unicast_connection.sock);

	if (socket < 0) {
		memset(&m_multicast_connection, 0, sizeof(m_multicast_connection));
		memset(&m_unicast_connection, 0, sizeof(m_unicast_connection));

		std::stringstream ss;
		ss << "Could not create socket, please check the parameters provided: Interface: " << interface << " IP: " << ip << " Port: " << port;
		LOG_ERROR(SocketReader, ss.str());
		throw BadParameterError(ss.str());
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
void SocketReader::run(SmartPacketBuffer<SDDSpacket> *pktbuffer) {
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

/** Returns true on success, or false if there was an error */
bool SocketReader::setSocketBlockingEnabled(int fd, bool blocking)
{
   if (fd < 0) return false;

   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0) return false;
   flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
   return (fcntl(fd, F_SETFL, flags) == 0) ? true : false;
}
