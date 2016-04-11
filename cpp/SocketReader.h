/*
 * SocketReader.h
 *
 *  Created on: Mar 29, 2016
 *      Author: ylbagou
 */

#ifndef SOCKETREADER_H_
#define SOCKETREADER_H_

#define MAX_ALLOWED_TIMEOUT 3

#include <boost/shared_ptr.hpp>
#include "sddspacket.h"
#include "SmartPacketBuffer.h"
#include "ossie/debug.h"
#include "socketUtils/multicast.h"
#include "socketUtils/unicast.h"
#include "socketUtils/SourceNicUtils.h"

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
    void setPktsPerRead(size_t pkts_per_read);
    size_t getPktsPerRead();
    void setConnectionInfo(std::string interface, std::string ip, uint16_t vlan, uint16_t port) throw (BadParameterError);
    void setSocketBufferSize(int socket_buffer_size);
    size_t getSocketBufferSize();
private:
    bool m_shuttingDown;
    bool m_running;
    int m_timeout;
    size_t m_pkts_per_read;
    size_t m_socket_buffer_size;
    multicast_t m_multicast_connection;
    unicast_t m_unicast_connection;
};

#endif /* SOCKETREADER_H_ */
