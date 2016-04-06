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

PREPARE_LOGGING(SocketReader)

SocketReader::SocketReader(): m_shuttingDown(false), m_running(false), m_timeout(1), m_pkts_per_read(1), m_socket_buffer_size(-1), m_sockfd(NULL) {}

SocketReader::~SocketReader() {}

void SocketReader::shutDown() {
	LOG_DEBUG(SocketReader, "Shutting down the socket reader");
	m_shuttingDown = true;
	m_running = false;
	LOG_DEBUG(SocketReader, "Closing socket");
	if (m_sockfd) { close(m_sockfd); }
}

void SocketReader::setTimeout(int timeout) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the timeout while the socket reader thread is running");
		return;
	}

	if (m_timeout >= 5) {
		LOG_WARN(SocketReader, "Socket timeout is >= 5 secs. This may cause issues when the component is stopped.");
	}

	m_timeout = timeout;
}

void SocketReader::setPktsPerRead(int pkts_per_read) {
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the packets per read while the socket reader thread is running");
		return;
	}
	m_pkts_per_read = pkts_per_read;
}

// TODO: Deal with the VLAN
void SocketReader::setConnectionInfo(std::string ip, uint16_t vlan, uint16_t port) {
	LOG_INFO(SocketReader, "IP: " << ip << "\nVLAN: " << vlan << "\nPort: " << port);
	if (m_running) {
		LOG_WARN(SocketReader, "Cannot change the socket address while the socket reader thread is running");
		return;
	}

    m_addr.sin_family = AF_INET;
    m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    m_addr.sin_port = htons(port);
}

void SocketReader::setSocketBufferSize(int socket_buffer_size) {
	m_socket_buffer_size = socket_buffer_size;
}

int SocketReader::getSocketBufferSize() {
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

    std::deque<SddsPacketPtr> bufQue;
    int retval;
    size_t i;

    struct mmsghdr msgs[m_pkts_per_read];
    struct iovec iovecs[m_pkts_per_read];

    struct timespec timeout;

    m_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sockfd == -1) {
        LOG_ERROR(SocketReader, "Failed to create socket file descriptor");
        m_running = false;
        return;
    }


    if (m_socket_buffer_size) {
    	if (setsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &m_socket_buffer_size, sizeof(m_socket_buffer_size))) {
    		LOG_WARN(SocketReader, "Failed to set socket buffer size to requested size");
    	}
    }


    // This is the socket connection time out, if we do not receive data in this amount of time we break out of
    // the recv call.
    struct timeval tv;
    tv.tv_sec = m_timeout;
    tv.tv_usec = 0;
    setsockopt(m_sockfd, SOL_SOCKET, SO_RCVTIMEO,(struct timeval *)&tv, sizeof(struct timeval));
    ////


    socklen_t optlen = sizeof(m_socket_buffer_size);
    getsockopt(m_sockfd, SOL_SOCKET, SO_RCVBUF, &m_socket_buffer_size, &optlen);

    if (bind(m_sockfd, (struct sockaddr *) &m_addr, sizeof(m_addr)) == -1) {
    	LOG_ERROR(SocketReader, "Failed to bind to socket");
		m_running = false;
		return;
    }

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
		//TODO: Should we add the flag MSG_WAITFORONE or MSG_WAITFORALL
		//TODO: Despite my attempts it seems this gets stuck if it is reading packets and then the packet stream stops.  How do I kill it?
		retval = recvmmsg(m_sockfd, msgs, m_pkts_per_read, MSG_WAITFORONE, &timeout);

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

		// Push the packets onto the queue
		pktbuffer->push_full_buffers(bufQue);
    }
}
