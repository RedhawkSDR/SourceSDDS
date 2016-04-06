/*
 * SddsToBulkIOProcessor.cpp
 *
 *  Created on: Apr 5, 2016
 *      Author: ylbagou
 */

#include "SddsToBulkIOProcessor.h"
#include "SddsToBulkIOUtils.h"

PREPARE_LOGGING(SddsToBulkIOProcessor)

SddsToBulkIOProcessor::SddsToBulkIOProcessor():m_pkts_per_read(DEFAULT_PKTS_PER_READ), m_running(false), m_shuttingDown(false), m_wait_for_ttv(false), m_push_on_ttv(false), m_first_packet(false), m_expected_seq_number(0), m_current_ttv_flag(false), m_last_wsec(0), m_start_of_year(0) {
}

SddsToBulkIOProcessor::~SddsToBulkIOProcessor() {
}


// TODO: We need to make sure this value is < the max corba transfer size and limit it to that if the user has asked to exceed it.
void SddsToBulkIOProcessor::setPktsPerRead(size_t pkts_per_read) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set packets per read while thread is running");
		return;
	}
	m_pkts_per_read = pkts_per_read;

	// This way we only ever allocate memory here with the reserve call
	m_bulkIO_data.reserve(m_pkts_per_read * SDDS_DATA_SIZE);
}

void SddsToBulkIOProcessor::shutDown() {
	LOG_DEBUG(SddsToBulkIOProcessor, "Shutting down the packet processsor");
	m_shuttingDown = true;
	m_running = false;
}

void SddsToBulkIOProcessor::setWaitForTTV(bool wait_for_ttv) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set packets per read while thread is running");
		return;
	}
	m_wait_for_ttv = wait_for_ttv;
}

void SddsToBulkIOProcessor::setPushOnTTV(bool push_on_ttv) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set packets per read while thread is running");
		return;
	}

	m_push_on_ttv = push_on_ttv;
}

// TODO: Can we pass by reference?
void SddsToBulkIOProcessor::run(SmartPacketBuffer<SDDSpacket> *pktbuffer) {
	m_running = true;
	m_shuttingDown = false;

	// Feed in packets to process,
	// TODO: reserve size for these as member variables.
	std::deque<SddsPacketPtr> pktsToProcess;
	std::deque<SddsPacketPtr> pktsToRecycle;

	while (not m_shuttingDown) {
		// We HAVE to recycle this buffer.
		pktbuffer->pop_full_buffers(pktsToProcess, m_pkts_per_read);

		if (not m_shuttingDown) {
			processPackets(pktsToProcess, pktsToRecycle);
		}

		pktbuffer->recycle_buffers(pktsToRecycle);
	}

	// Shutting down, recycle all the packets
	pktbuffer->recycle_buffers(pktsToProcess);
	pktbuffer->recycle_buffers(pktsToRecycle);

	m_running = false;
}

/**
 * Returns true if this packet's sequence number matches the expected sequence number
 * Increments the expected sequence number if true.
 * If the packet does not match the expected, we calculate packets dropped and reset first packet
 */
// TODO: How should we deal with out of order packets?  Right now we dont
bool SddsToBulkIOProcessor::orderIsValid(SddsPacketPtr &pkt) {

	// First packet, its valid.
	if (m_first_packet) {
		m_first_packet = false;
		m_current_ttv_flag = pkt->get_ttv();
		m_expected_seq_number = pkt->get_seq() + 1;

		// Adjust for the CRC packet
		if (m_expected_seq_number != 0 && m_expected_seq_number % 32 == 31)
			m_expected_seq_number++;

		return true;
	}

	// If it doesn't match what we're expecting then it's not valid.
	if (m_expected_seq_number != pkt->get_seq()) {
		LOG_WARN(SddsToBulkIOProcessor, "Expected packet " << m_expected_seq_number << " Received: " << pkt->get_seq());
		// TODO: Calculate accurately the number of packets dropped
		m_first_packet = true;
		return false;
	}

	m_expected_seq_number++;

	// Adjust for the CRC packet
	if (m_expected_seq_number != 0 && m_expected_seq_number % 32 == 31)
		m_expected_seq_number++;

	return true;
}



/**
 * First Packet
 *   Check sequence
 *     if good (always the case)
 *       Check for ttv state change (Never changed we just set it)
 *         Check SRI?
 *         Fill bulkIO packet
 *
 *
 * Nth Packet
 *   Check sequence
 *     if good
 *       check for ttv state change
 *         if same
 *           Check SRI?
 *           Fill bulkIO packet
 *         if changed
 *           push what we have, set new state, return so we get a refill
 *     if bad FN
 *        push what we have, set first packet flag, return so we get a refill.
 */



/**
 * This is the main method for processing the sdds packets. We want to push out in chunks of m_pkts_per_read so we try and keep
 * pktsToWork that size by keeping a second container of pkts to recycle. When we find a time discontinuity or are waiting for
 * a TTV we can recycle what we've used and get a refill on pktsToWork to bring it back up to size.
 */
void SddsToBulkIOProcessor::processPackets(std::deque<SddsPacketPtr> &pktsToWork, std::deque<SddsPacketPtr> &pktsToRecycle) {
	std::deque<SddsPacketPtr>::iterator pkt_it = pktsToWork.begin();
	while (pkt_it != pktsToWork.end()) {
		SddsPacketPtr pkt = *pkt_it;

		// If the order is not valid we've lost some packets, we need to push what we have, reset the SRI.
		if (!orderIsValid(pkt)) {
			//TODO: push what we have.
			//TODO: reset Time stamp with new time stamp because there is a time discontinuity and resend SRI.
			m_first_packet = true;
			return;
		} else {
			// The order is valid.

			// If the current ttv flag does not match this packets, there has been a state change.
			// This only matters if the user has requested we push on ttv.
			// If this is the case we need to push, flush the queue and return so we start on
			// the new ttv state.
			if (m_current_ttv_flag != pkt->get_ttv() && m_push_on_ttv) {
				m_current_ttv_flag = pkt->get_ttv();
				// TODO: push what we have
				// TODO: reset SRI
				// TODO: return so we can refill our buffer
			}

			// The user may have requested we not push when the timecode is invalid. If this is the case we just need to recycle
			// the buffers that don't have good ttv's and continue with the next packet hoping the ttv is true.
			// TODO: The old SourceNIC would simply wait for the first TTV and then it didnt care. What do people want?
			if (m_wait_for_ttv && !pkt->get_ttv()) {
				pktsToRecycle.push_back(pkt);
				pkt_it = pktsToWork.erase(pkt_it);
				continue;
			}

			// At this point we should have a good packet and have dealt with any specific user requests regarding the ttv field.

			//TODO: all the work needed
			BULKIO::PrecisionUTCTime bulkio_time_stamp = getBulkIOTimeStamp(pkt.get(), m_last_wsec, m_start_of_year);
			m_bps = getBps(pkt.get());

			// TODO: Deal with endianness.

			// Grab and push SRI if we need it
			bool sriChanged;
			mergeSddsSRI(pkt.get(), m_sri, sriChanged);

			if (sriChanged) {
				//TODO: Should we also flush the current buffer? If we do we need to consider that this may be the first time SRI is set.
				//TODO: Push SRI
			}

			// Copying the data of the current packet into the bulkIO data vector

			//XXX: I wasnt sure if sizeof(pkt->d) would work but it does return 1024.
			m_bulkIO_data.insert(m_bulkIO_data.end(), pkt->d, pkt->d + sizeof(pkt->d));

			// And we are done with this packet. Take it off the pktsToWork que and add it to the pktsToRecycle que.
			pktsToRecycle.push_back(pkt);
			pkt_it = pktsToWork.erase(pkt_it);

			// We've worked through the full stack of packets, push the data and clear the buffer
			if (pkt_it == pktsToWork.end()) {
				//TODO: push packet
				m_bulkIO_data.clear();
			}
		}
	}
}
