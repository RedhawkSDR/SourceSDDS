/*
 * SddsToBulkIOProcessor.cpp
 *
 *  Created on: Apr 5, 2016
 *      Author: ylbagou
 */

#include "SddsToBulkIOProcessor.h"

PREPARE_LOGGING(SddsToBulkIOProcessor)

SddsToBulkIOProcessor::SddsToBulkIOProcessor():m_pkts_per_read(500), m_running(false), m_shuttingDown(false) {
}

SddsToBulkIOProcessor::~SddsToBulkIOProcessor() {
}

void SddsToBulkIOProcessor::setPktsPerRead(size_t pkts_per_read) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set packets per read while thread is running");
		return;
	}
	m_pkts_per_read = pkts_per_read;
}

void SddsToBulkIOProcessor::shutDown() {
	LOG_DEBUG(SddsToBulkIOProcessor, "Shutting down the packet processsor");
	m_shuttingDown = true;
	m_running = false;
}

// TODO: Can we pass by reference?
void SddsToBulkIOProcessor::run(SmartPacketBuffer<SDDSpacket> *pktbuffer) {
	uint16_t expectedSeqNumber = 0;
	m_running = true;
	m_shuttingDown = false;

	bool firstPacket = true;
	std::vector<SddsPacketPtr> sptrVec;

	while (not m_shuttingDown) {
		// We HAVE to recycle this buffer.
		LOG_DEBUG(SddsToBulkIOProcessor, "Grabbing full packets");
		pktbuffer->pop_full_buffers(sptrVec, m_pkts_per_read);

		if (not m_shuttingDown) {
			for (size_t i = 0; i < sptrVec.size(); ++i) {
				if (firstPacket) {
					firstPacket = false;
					expectedSeqNumber = sptrVec[i]->get_seq() + 1;
					continue;
				}


				if (expectedSeqNumber != 0 && expectedSeqNumber % 32 == 31)
					expectedSeqNumber++;

				if (expectedSeqNumber != sptrVec[i]->get_seq()) {
					std::cout << "Expected packet: " << expectedSeqNumber << " Received: " << sptrVec[i]->get_seq() << "Dropped: " << sptrVec[i]->get_seq() - expectedSeqNumber << std::endl;
					firstPacket = true;
					continue;
				}

				expectedSeqNumber++;

			}
		}

		pktbuffer->recycle_buffers(sptrVec);
	}

	m_running = false;
}
