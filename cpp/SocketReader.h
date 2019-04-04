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
 * SocketReader.h
 *
 *  Created on: Mar 29, 2016
 *      Author: 
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
public:
	SocketReader();
	virtual ~SocketReader();

    void run(SmartPacketBuffer<SDDSpacket> *pktbuffer, const bool confirmHosts);
    void shutDown();
    void setPktsPerRead(size_t pkts_per_read);
    size_t getPktsPerRead();
    void setConnectionInfo(std::string interface, std::string ip, uint16_t vlan, uint16_t port) throw (BadParameterError);
    void setSocketBufferSize(int socket_buffer_size);
    size_t getSocketBufferSize();
    std::string getInterface();
    bool setSocketBlockingEnabled(int fd, bool blocking);
	void setLogger(LOGGER log);
private:
    LOGGER _log;
    bool m_shuttingDown;
    bool m_running;
    int m_timeout;
    struct in_addr m_host_addr;
    size_t m_pkts_per_read;
    size_t m_socket_buffer_size;
    multicast_t m_multicast_connection;
    unicast_t m_unicast_connection;
    std::string m_interface;
    void confirmSingleHost(struct mmsghdr msgs[], size_t len);
    std::string getMcastIfaceFromRoutes(std::string group="224.0.0.0");

};

#endif /* SOCKETREADER_H_ */
