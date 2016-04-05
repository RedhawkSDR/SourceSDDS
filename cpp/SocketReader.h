/*
 * SocketReader.h
 *
 *  Created on: Mar 29, 2016
 *      Author: ylbagou
 */

#ifndef SOCKETREADER_H_
#define SOCKETREADER_H_

#include <boost/shared_ptr.hpp>
#include "sddspacket.h"
#include "SmartPacketBuffer.h"
#include "ossie/debug.h"

#define SDDS_PACKET_SIZE 1080

typedef boost::shared_ptr<SDDSpacket> SddsPacketPtr;

class SocketReader {
	ENABLE_LOGGING
public:
	SocketReader();
	virtual ~SocketReader();

    void run(SmartPacketBuffer<SDDSpacket> *pktbuffer);
    void shutDown();
    void setTimeout(int timeout);
    void setPktsPerRead(int pkts_per_read);
    void setConnectionInfo(std::string ip, uint16_t vlan, uint16_t port);
    void setSocketBufferSize(int socket_buffer_size);
    int  getSocketBufferSize();
private:
    bool m_shuttingDown;
    bool m_running;
    int m_timeout;
    size_t m_pkts_per_read;
    struct sockaddr_in m_addr;
    int m_socket_buffer_size, m_sockfd;
};

#endif /* SOCKETREADER_H_ */
