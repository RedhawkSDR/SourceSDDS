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
 * SddsToBulkIOProcessor.cpp
 *
 *  Created on: Apr 5, 2016
 *      Author: 
 */

#include "SddsToBulkIOProcessor.h"
#include "SddsToBulkIOUtils.h"
#include <math.h>

PREPARE_LOGGING(SddsToBulkIOProcessor)

//TODO: Should accum_error_tolerance be a setable property?  Should we report it back?
SddsToBulkIOProcessor::SddsToBulkIOProcessor(bulkio::OutOctetPort *octet_out, bulkio::OutShortPort *short_out, bulkio::OutFloatPort *float_out):
	m_pkts_per_read(DEFAULT_PKTS_PER_READ), m_running(false), m_shuttingDown(false), m_wait_for_ttv(false),
	m_push_on_ttv(false), m_first_packet(true), m_current_ttv_flag(false),m_expected_seq_number(0),
	m_last_sdds_time(0), m_pkts_dropped(0), m_bps(0), m_octet_out(octet_out), m_short_out(short_out),
	m_float_out(float_out), m_upstream_sri_set(false), m_endianness(ENDIANNESS::ENDIAN_DEFAULT),
	m_new_upstream_sri(false), m_use_upstream_sri(false), m_num_time_slips(0), m_current_sample_rate(0),
	m_max_time_step(0), m_min_time_step(0), m_ideal_time_step(0), m_time_error_accum(0),
	m_accum_error_tolerance(0.000001),m_non_conforming_device(false)
{
	// reserve size so it is done at construct time
	m_bulkIO_data.reserve(m_pkts_per_read * SDDS_DATA_SIZE);

	// Needs to be initialized.
	m_sri.streamID = "DEFAULT_SDDS_STREAM_ID";

	m_start_of_year = getStartOfYear();
}

SddsToBulkIOProcessor::~SddsToBulkIOProcessor() {
	shutDown();
}

/**
 * Sets the number of SDDS packets to read off of the smart packet buffer queue.
 * This also maps to the target number of SDDS packets pushed via the pushPacket call.
 * See the documentation for details.
 */
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

size_t SddsToBulkIOProcessor::getPktsPerRead() {
	return m_pkts_per_read;
}

/**
 * Sets the shut down boolean to true so that during the next pass
 * the SDDS to BulkIO processor will exit cleanly. Any currently
 * buffered data will be pushed out the BulkIO port.
 */
void SddsToBulkIOProcessor::shutDown() {
	LOG_DEBUG(SddsToBulkIOProcessor, "Shutting down the packet processor");
	m_shuttingDown = true;
	m_running = false;
}

/**
 * Sets the wait on TTV option, see the documentation for details.
 * Cannot be called while the run method is active.
 */
void SddsToBulkIOProcessor::setWaitForTTV(bool wait_for_ttv) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set wait on TTV while thread is running");
		return;
	}
	m_wait_for_ttv = wait_for_ttv;
}

/**
 * Sets the push on TTV option, see the documentation for details.
 * Cannot be called while the run method is active.
 */
void SddsToBulkIOProcessor::setPushOnTTV(bool push_on_ttv) {
	if (m_running) {
		LOG_WARN(SddsToBulkIOProcessor, "Cannot set push on TTV while thread is running");
		return;
	}

	m_push_on_ttv = push_on_ttv;
}

/**
 * This is the entry point to the processing thread. The provided pktbuffer will be
 * used to pull full packets from, processed via the processPackets call, then the processed
 * packets will be recycled. This method does not return until the shutdown method is called.
 */
void SddsToBulkIOProcessor::run(SmartPacketBuffer<SDDSpacket> *pktbuffer) {
	m_running = true;
	m_shuttingDown = false;
	pthread_setname_np(pthread_self(), "SddsToBulkIOProcessor");

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

	//Push any remaining data and an EOS
    pushPacket(true);


	// Shutting down, recycle all the packets
	pktbuffer->recycle_buffers(pktsToProcess);
	pktbuffer->recycle_buffers(pktsToRecycle);

	m_running = false;
	m_first_packet = true;
}

/**
 * Calculates the expected xdelta based on the provided rate, complex flag, and current m_bps.
 * The member variables max, ideal, and min time steps are updated which are used to deteremine
 * if a time slip has occured.
 */
void SddsToBulkIOProcessor::updateExpectedXdelta(double rate, bool complex) {
	// Update our current sample rate and last times
	m_current_sample_rate = rate;
	// If complex then the samples per packet is half
	if (m_bps == 0) {
		LOG_FATAL(SddsToBulkIOProcessor, "Bits per sample on SDDS stream is set to zero! Cannot generate expected Xdelta, expect lots of errors.");
		return;
	}
	int samps_per_packet = SDDS_DATA_SIZE / (m_bps / 8);

	if (complex) {
		samps_per_packet = samps_per_packet / 2;
	}

	m_max_time_step = ((double) (samps_per_packet + 1)) / m_current_sample_rate;
	m_ideal_time_step = ((double) (samps_per_packet)) / m_current_sample_rate;
	m_min_time_step = ((double) (samps_per_packet - 1)) / m_current_sample_rate;
}

/**
 * Returns true if this packet's sequence number matches the expected sequence number
 * Increments the expected sequence number if true.
 * If the packet does not match the expected, we calculate packets dropped and reset first packet
 */
bool SddsToBulkIOProcessor::orderIsValid(SddsPacketPtr &pkt) {

	// First packet, its valid.
	if (m_first_packet) {
		m_first_packet = false;
		m_current_ttv_flag = pkt->get_ttv();
		m_expected_seq_number = pkt->get_seq();
		m_bps = (pkt->bps == 31) ? 32 : pkt->bps;
		m_last_sdds_time = 0;

		updateExpectedXdelta(m_non_conforming_device ? pkt->get_rate() * 2 : pkt->get_rate(), pkt->cx != 0);
		return true;
	}

	// If it doesn't match what we're expecting then it's not valid.
	if (m_expected_seq_number != pkt->get_seq()) {
		// No need to worry about the wrap around, if everything is uint16_t twos compliment takes care of it all for us.
		uint16_t numDropped = pkt->get_seq() - m_expected_seq_number;
		LOG_WARN(SddsToBulkIOProcessor, "Expected packet " << m_expected_seq_number << " Received: " << pkt->get_seq() << " Dropped: " << numDropped);
		m_pkts_dropped += numDropped;
		m_first_packet = true;
		return false;
	}

	return true;
}

/**
 * Checks the provided packet to see if a time slip has occured. This can either be a time
 * discontinuity between subsequent packets or a slow time slip over a number of packets by
 * calculating the accumulated time slip. See the documentation for details.
 * There is also a check, and adjustments for poorly behaving devices which may not abide by the SDDS standard (such as the MSDD)
 * see the note below for details.
 */
void SddsToBulkIOProcessor::checkForTimeSlip(SddsPacketPtr &pkt) {
	// If time tag is not valid no need to check for time slips.
	bool slip = false;

	if (not pkt->get_ttv()) {
		return;
	}

	// This magically works. :-) (operator overloading)
	if (m_last_sdds_time == 0) {
		m_last_sdds_time = pkt->get_SDDSTime();
		return;
	}

	SDDSTime curr_time = pkt->get_SDDSTime();
	double deltaTime = curr_time.seconds() - m_last_sdds_time.seconds();

	m_last_sdds_time = curr_time;

	if (deltaTime < 0) {
		LOG_INFO(SddsToBulkIOProcessor, "Received a negative delta between packet time stamps, time is either going backwards or the year has rolled over");
		m_last_sdds_time = curr_time;
		return;
	}

	if (deltaTime > m_max_time_step || deltaTime < m_min_time_step) {
		// XXX Special case here! Some devices, like the MSDD do not conform to the SDDS standard and the header contains a bad sample rate
		// the sample rate is off by a factor of two which we detect here based on the xdelta and account for with the m_non_conforming_device boolean.
		// we also check m_num_time_slips just in case we have a device that is slipping a lot and happens to fall into this position.
		if (!m_non_conforming_device && pkt->cx != 0 && m_num_time_slips == 0 && 2*deltaTime < m_max_time_step && 2*deltaTime > m_min_time_step) {
			LOG_INFO(SddsToBulkIOProcessor, "Based on the received XDelta between packets, it appears that these SDDS packets do not conform to the spec. "
						   "This is a known issue for some devices (eg. MSDD) where the sample rate in the header is off by a factor of two. "
						   "The expected XDelta has been adjusted, this will also be reflected in the output SRI unless overridden via SRI Keywords, if the SRI is not overridden it will result in a single erroneous SRI push");
			m_non_conforming_device = true;
			updateExpectedXdelta(2*m_current_sample_rate, pkt->cx != 0);
		} else {
			LOG_WARN(SddsToBulkIOProcessor, "Delta time of " << deltaTime << " occurred on packet: " << pkt->seq << " delta between packets expected to be between " << m_min_time_step  << " and " << m_max_time_step);
			slip = true;
		}
	}

	m_time_error_accum += deltaTime - m_ideal_time_step;

	if (std::abs(m_time_error_accum) > m_accum_error_tolerance) {
		LOG_WARN(SddsToBulkIOProcessor, "The time slip accumulator has exceeded the limit set, counting it as a time slip.");
		m_time_error_accum = 0.0;
		slip = true;
	}

	if(slip) {
		m_num_time_slips++;
	}
}
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

		if (m_wait_for_ttv && (pkt->get_ttv() == 0)) {
			pktsToRecycle.push_back(pkt);
			pkt_it = pktsToWork.erase(pkt_it);
			if (m_bulkIO_data.size() > 0) {
				pushPacket(false);
			}
			continue;
		}

		// If the order is not valid we've lost some packets, we need to push what we have, reset the SRI.
		if (!orderIsValid(pkt)) {
			pushPacket(false);
			m_first_packet = true;
			return;
		} else {

			// If the current ttv flag does not match this packets, there has been a state change.
			// This only matters if the user has requested we push on ttv.
			// If this is the case we need to push and restart with the new ttv state.
			if (m_push_on_ttv && m_current_ttv_flag != (pkt->get_ttv() != 0) ) {
				m_current_ttv_flag = (pkt->get_ttv() != 0);
				pushPacket(false);
				return;
			}

			// At this point we should have a good packet and have dealt with any specific user requests regarding the ttv field.

			// We can assume that the SDDS streams SRI (xdelta) should stay the same for a given stream.
			bool sriChanged = false;


			{
				boost::unique_lock<boost::mutex> lock(m_upstream_sri_lock);
				if (m_upstream_sri_set && m_new_upstream_sri) {
					m_new_upstream_sri = false;
					mergeUpstreamSRI(m_sri, m_upstream_sri, m_use_upstream_sri, sriChanged, m_endianness);
				}
			}

			if (!m_use_upstream_sri) {
				mergeSddsSRI(pkt.get(), m_sri, sriChanged, m_non_conforming_device);
			}

			if (sriChanged) {
				pushPacket(false);
				pushSri();
				updateExpectedXdelta(m_non_conforming_device ? pkt->get_rate() * 2 : pkt->get_rate(), pkt->cx != 0);
				m_last_sdds_time = 0;
				return; // Refill our packets
			}

			// Create the bulkIO time stamp if this is the first packet to send.
			if (m_bulkIO_data.size() == 0) {
				m_bulkio_time_stamp = getBulkIOTimeStamp(pkt.get(), m_last_sdds_time, m_start_of_year);
			}

			// Check for time slips
			checkForTimeSlip(pkt);

			// Did some quick testing to see if an insert or a resize + memcopy was faster, insert FTW.
			//I wasn't sure if sizeof(pkt->d) would work but it does return 1024.
			m_bulkIO_data.insert(m_bulkIO_data.end(), pkt->d, pkt->d + sizeof(pkt->d));

			// And we are done with this packet. Take it off the pktsToWork que and add it to the pktsToRecycle que.
			pktsToRecycle.push_back(pkt);
			pkt_it = pktsToWork.erase(pkt_it);

			// Now that we are officially done with the packet we can increment our packet counter
			m_expected_seq_number++;

			// Adjust for the CRC packet
			if (m_expected_seq_number != 0 && m_expected_seq_number % 32 == 31)
				m_expected_seq_number++;

			// We've worked through the full stack of packets, push the data and clear the buffer
			if (pkt_it == pktsToWork.end()) {
				pushPacket(false);
				m_bulkIO_data.clear();
			}
		}
	}
}

/**
 * Pushes the current SRI to the appropriate port based on m_bps.
 */
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
		LOG_ERROR(SddsToBulkIOProcessor, "Could not push sri, either the bits per sample is non-standard set to: " << m_bps);
		break;
	}

}

/**
 * Pushes bulkIO and possibly an SRI packet if SRI has never been sent to that port.
 * Will also clear the m_bulkIO_data vector.
 */
//TODO: Do we ever need to push an EOS flag?
void SddsToBulkIOProcessor::pushPacket(bool eos) {
	if (m_bulkIO_data.size() == 0 and !eos) {
		return;
	}

	switch(m_bps) {
	case 8:
		if (m_octet_out->getCurrentSRI().count(m_sri.streamID.in())==0) {
			m_octet_out->pushSRI(m_sri);
		}

		m_octet_out->pushPacket(m_bulkIO_data, m_bulkio_time_stamp, eos, m_sri.streamID.in());
		break;
	case 16:
		if (m_short_out->getCurrentSRI().count(m_sri.streamID.in())==0) {
			m_short_out->pushSRI(m_sri);
		}

		// Ugh, we need to byte swap. At least there is a nice builtin for swapping bytes for shorts.
		if (atol(m_endianness.c_str()) != __BYTE_ORDER) {
			swab(&m_bulkIO_data[0], &m_bulkIO_data[0], m_bulkIO_data.size());
		}

		m_short_out->pushPacket(reinterpret_cast<short*> (&m_bulkIO_data[0]), m_bulkIO_data.size()/sizeof(short), m_bulkio_time_stamp, eos, m_sri.streamID.in());
		break;
	case 32:
		if (m_float_out->getCurrentSRI().count(m_sri.streamID.in())==0) {
			m_float_out->pushSRI(m_sri);
		}

		// Ugh, we need to byte swap and for floats there is no nice method for us to use like there is for shorts. Time to iterate.
		if (atol(m_endianness.c_str()) != __BYTE_ORDER) {
			uint32_t *buf = reinterpret_cast<uint32_t*>(&m_bulkIO_data[0]);
			for (size_t i = 0; i < m_bulkIO_data.size() / sizeof(float); ++i) {
				buf[i] = __builtin_bswap32(buf[i]);
			}
		}

		m_float_out->pushPacket(reinterpret_cast<float*>(&m_bulkIO_data[0]), m_bulkIO_data.size()/sizeof(float), m_bulkio_time_stamp, eos, m_sri.streamID.in());
		break;
	default:
		LOG_ERROR(SddsToBulkIOProcessor, "Could not push packet, the bits per sample are non-standard and set to: " << m_bps);
		break;
	}

	m_bulkIO_data.clear();
}
/**
 * Returns whether the processor is set to push on a time tag valid flag change.
 * See the documentation for details.
 */
bool SddsToBulkIOProcessor::getPushOnTTV() {
	return m_push_on_ttv;
}

/**
 * Returns whether the processor is set to wait on a time tag valid flag.
 * See the documentation for details.
 */
bool SddsToBulkIOProcessor::getWaitOnTTV() {
	return m_wait_for_ttv;
}

/**
 * Returns the number of bits per sample which is pulled directly from the SDDS packet header
 * except for when the value in the header is 31 in which case it represents 32 bits and just
 * doesn't have the resolution to represent 32.
 */
unsigned short SddsToBulkIOProcessor::getBps() {
	return m_bps;
}

/**
 * The number of lost SDDS packets. For simplicity, the calculation includes the
 * optional checksum packets in the lost SDDS packet count (sent every 32 packets)
 * so it may not reflect the exact number of dropped packets if checksum packets are not used (and they never are).
 */
unsigned long long SddsToBulkIOProcessor::getNumDropped() {
	return m_pkts_dropped;
}

/**
 * Returns the expected SDDS sequence number. This processor assumes that
 * the checksum packet will never be sent.
 */
uint16_t SddsToBulkIOProcessor::getExpectedSequenceNumber() {
	return m_expected_seq_number;
}

/**
 * Replaces any existing SRI with the provided SRI object and flags the processor
 * to merge this new SRI in with the current SRI object during the next SDDS packet processed.
 * If the SRI has changed, it will cause a push packet such that the next BulkIO push has the new
 * SRI associated. This method causes a lock to be aquired since it is likely called
 * from outside of the processing thread.
 *
 * @param upstream_sri The SRI object which will now be mapped to this stream.
 */
void SddsToBulkIOProcessor::setUpstreamSri(BULKIO::StreamSRI upstream_sri) {
	boost::unique_lock<boost::mutex> lock(m_upstream_sri_lock);
	m_upstream_sri = upstream_sri;
	m_upstream_sri_set = true;
	m_new_upstream_sri = true;
}

/**
 * Flags the processor loop to not use the upstream SRI. This will
 * also reset the endianness back to the default (Network Byte Order)
 * This method causes a lock to be aquired since it is likely called
 * from outside of the processing thread.
 */
void SddsToBulkIOProcessor::unsetUpstreamSri() {
	boost::unique_lock<boost::mutex> lock(m_upstream_sri_lock);
	m_use_upstream_sri = false;
	m_upstream_sri_set = false;
	m_endianness = ENDIANNESS::ENDIAN_DEFAULT; // Default to big endian
	m_sri.streamID = "DEFAULT_SDDS_STREAM_ID";
}

/**
 * Returns the current stream ID which is derived from
 * the upstream SRI or created if none is provided.
 */
std::string SddsToBulkIOProcessor::getStreamId() {
	return CORBA::string_dup(m_sri.streamID);
}

/**
 * Returns the SDDS sample rate. The sample rate can be found in either
 * the SDDS header, or the upstream SRI if the upstream SRI is being used.
 * See the documentation for details on how to set and use the upstream SRI
 * in place of the SDDS header value.
 */
double SddsToBulkIOProcessor::getSampleRate() {
	return m_current_sample_rate;
}

/**
 * Returns the currently set assumed engianness of the
 * data portion of the SDDS packet. Valid strings are
 * definied in the header file.
 */
std::string SddsToBulkIOProcessor::getEndianness() {
	return m_endianness;
}

/**
 * Sets the expected endianness of the data portion of the SDDS packet.
 * Cannot be called while the processor is running. Acceptable values are
 * defined in the header file. Unknown values will be logged and ignored.
 */
void SddsToBulkIOProcessor::setEndianness(std::string endianness) {
	if (m_running) {
		LOG_ERROR(SddsToBulkIOProcessor, "Cannot change endianness while running.");
		return;
	}

	if (endianness != ENDIANNESS::BIG_ENDIAN_STR && endianness != ENDIANNESS::LITTLE_ENDIAN_STR) {
		LOG_ERROR(SddsToBulkIOProcessor, "Tried to set endianness to unknown value: " << endianness << " Endianness will not be changed.");
		return;
	}

	m_endianness = endianness;
}

/**
 * Returns the total number of timeslips where a time slip
 * can either be an accumulated time slip or a time slip between
 * packets. See the documentation for more details.
 */
long SddsToBulkIOProcessor::getTimeSlips() {
	return m_num_time_slips;
}
