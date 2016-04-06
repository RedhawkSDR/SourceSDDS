/*
 * SddsToBulkIOProcessor.cpp
 *
 *  Created on: Apr 5, 2016
 *      Author: ylbagou
 */

#include "SddsToBulkIOProcessor.h"
#include "SddsToBulkIOUtils.h"
#include <math.h>

PREPARE_LOGGING(SddsToBulkIOProcessor)

SddsToBulkIOProcessor::SddsToBulkIOProcessor(bulkio::OutOctetPort *octet_out, bulkio::OutShortPort *short_out, bulkio::OutFloatPort *float_out):
	m_pkts_per_read(DEFAULT_PKTS_PER_READ), m_running(false), m_shuttingDown(false), m_wait_for_ttv(false), m_push_on_ttv(false), m_first_packet(true), m_current_ttv_flag(false),m_expected_seq_number(0), m_last_wsec(0), m_start_of_year(0), m_bps(0), m_octet_out(octet_out), m_short_out(short_out), m_float_out(float_out), m_stream_id("DEFAULT_STREAM_ID")
{
	// reserve size so it is done at construct time
	m_bulkIO_data.reserve(m_pkts_per_read * SDDS_DATA_SIZE);
}

SddsToBulkIOProcessor::~SddsToBulkIOProcessor() {
}

void SddsToBulkIOProcessor::setStreamId(std::string stream_id) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set stream ID while running");
		return;
	}
	m_stream_id = stream_id;
}

void SddsToBulkIOProcessor::setPktsPerRead(size_t pkts_per_read) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set packets per read while thread is running");
		return;
	}
	if ((pkts_per_read * SDDS_DATA_SIZE) > (CORBA_MAX_XFER_BYTES)) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set packets per read to " << pkts_per_read << " this would be larger than what can be pushed by CORBA (" << ((CORBA_MAX_XFER_BYTES)) << " Bytes)");
		m_pkts_per_read = floorl((CORBA_MAX_XFER_BYTES) / (SDDS_DATA_SIZE));
		LOG_WARN(SddsToBulkIOProcessor, "Setting pkts per read to the max value: " << m_pkts_per_read);
	} else {
		m_pkts_per_read = pkts_per_read;
	}

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
	// Since these are deques there is no reserve so we can just throw it on the stack.
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
// TODO: How should we deal with out of order packets?  Right now we don't they are considered a drop, and a big one, then another drop.
bool SddsToBulkIOProcessor::orderIsValid(SddsPacketPtr &pkt) {

	// First packet, its valid.
	if (m_first_packet) {
		m_first_packet = false;
		m_current_ttv_flag = pkt->get_ttv();
		m_expected_seq_number = pkt->get_seq() + 1;
		m_bps = pkt->bps;

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

		// The user may have requested we not push when the timecode is invalid. If this is the case we just need to recycle
		// the buffers that don't have good ttv's and continue with the next packet hoping the ttv is true.
		//XXX If they want to wait for ttv, this may result in a single push packet occurring at a non-max size.
		// since the ttv changes so infrequently this is probably okay.
		if (m_wait_for_ttv && !pkt->get_ttv()) {
			pktsToRecycle.push_back(pkt);
			pkt_it = pktsToWork.erase(pkt_it);
			continue;
		}

		// If the order is not valid we've lost some packets, we need to push what we have, reset the SRI.
		if (!orderIsValid(pkt)) {
			pushPacket();
			m_first_packet = true;
			return;
		} else {
			// The order is valid.

			// If the current ttv flag does not match this packets, there has been a state change.
			// This only matters if the user has requested we push on ttv.
			// If this is the case we need to push and restart with the new ttv state.
			if (m_current_ttv_flag != pkt->get_ttv() && m_push_on_ttv) {
				m_current_ttv_flag = pkt->get_ttv();
				pushPacket();
				return;
			}


			// At this point we should have a good packet and have dealt with any specific user requests regarding the ttv field.
			// Grab and push SRI if we need it
			bool sriChanged;
			mergeSddsSRI(pkt.get(), m_sri, sriChanged);

			if (sriChanged) {
				pushPacket();
				pushSri();
				return; // Refill our packets
			}

			// Create the bulkIO time stamp if this is the first packet to send.
			if (m_bulkIO_data.size() == 0) {
				m_bulkio_time_stamp = getBulkIOTimeStamp(pkt.get(), m_last_wsec, m_start_of_year);
			}

			// TODO: Deal with endianness.


			// Copying the data of the current packet into the bulkIO data vector
			//XXX: I wasnt sure if sizeof(pkt->d) would work but it does return 1024.
			m_bulkIO_data.insert(m_bulkIO_data.end(), pkt->d, pkt->d + sizeof(pkt->d));

			// And we are done with this packet. Take it off the pktsToWork que and add it to the pktsToRecycle que.
			pktsToRecycle.push_back(pkt);
			pkt_it = pktsToWork.erase(pkt_it);

			// We've worked through the full stack of packets, push the data and clear the buffer
			if (pkt_it == pktsToWork.end()) {
				pushPacket();
				m_bulkIO_data.clear();
			}
		}
	}
}

void SddsToBulkIOProcessor::pushSri() {
	LOG_DEBUG(SddsToBulkIOProcessor, "Pushing SRI");
	switch(m_bps) {
	case 8:
		m_octet_out->pushSRI(m_sri);
		break;
	case 16:
		m_short_out->pushSRI(m_sri);
		break;
	case 32:
		m_float_out->pushSRI(m_sri);
		break;
	default:
		LOG_ERROR(SddsToBulkIOProcessor, "Could not push sri, the bits per sample are non-standard and set to: " << m_bps);
		break;
	}

}

/**
 * Pushes bulkIO and possibly an SRI packet if SRI has never been sent to that port.
 * Will also clear the m_bulkIO_data vector.
 */
//TODO: Do we ever need to push an EOS flag?
void SddsToBulkIOProcessor::pushPacket() {
	if (m_bulkIO_data.size() == 0) {
		return;
	}

	switch(m_bps) {
	case 8:
		if (m_octet_out->getCurrentSRI().count(m_stream_id)==0) {
			m_octet_out->pushSRI(m_sri);
		}

		m_octet_out->pushPacket(m_bulkIO_data, m_bulkio_time_stamp, false, m_stream_id);
		break;
	case 16:
		if (m_short_out->getCurrentSRI().count(m_stream_id)==0) {
			m_short_out->pushSRI(m_sri);
		}

		m_short_out->pushPacket(reinterpret_cast<short*> (&m_bulkIO_data[0]), m_bulkIO_data.size()/sizeof(short), m_bulkio_time_stamp, false, m_stream_id);
		break;
	case 32:
		if (m_float_out->getCurrentSRI().count(m_stream_id)==0) {
			m_float_out->pushSRI(m_sri);
		}

		m_float_out->pushPacket(reinterpret_cast<float*>(&m_bulkIO_data[0]), m_bulkIO_data.size()/sizeof(float), m_bulkio_time_stamp, false, m_stream_id);
		break;
	default:
		LOG_ERROR(SddsToBulkIOProcessor, "Could not push packet, the bits per sample are non-standard and set to: " << m_bps);
		break;
	}

	m_bulkIO_data.clear();
}
