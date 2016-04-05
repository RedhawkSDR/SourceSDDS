/*
 * SddsToBulkIOProcessor.h
 *
 *  Created on: Apr 5, 2016
 *      Author: ylbagou
 */

#ifndef SDDSTOBULKIOPROCESSOR_H_
#define SDDSTOBULKIOPROCESSOR_H_

#include <boost/shared_ptr.hpp>
#include "SmartPacketBuffer.h"
#include "ossie/debug.h"
#include "sddspacket.h"

#define SDDS_PACKET_SIZE 1080

typedef boost::shared_ptr<SDDSpacket> SddsPacketPtr;

class SddsToBulkIOProcessor {
	ENABLE_LOGGING
public:
	SddsToBulkIOProcessor();
	virtual ~SddsToBulkIOProcessor();
	void run(SmartPacketBuffer<SDDSpacket> *pktbuffer);
	void setPktsPerRead(size_t pkts_per_read);
	void shutDown();
private:
	size_t m_pkts_per_read;
	bool m_running;
	bool m_shuttingDown;
};

#endif /* SDDSTOBULKIOPROCESSOR_H_ */
