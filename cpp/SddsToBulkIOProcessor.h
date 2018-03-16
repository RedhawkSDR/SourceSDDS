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
 * SddsToBulkIOProcessor.h
 *
 *  Created on: Apr 5, 2016
 *      Author: 
 */

#ifndef SDDSTOBULKIOPROCESSOR_H_
#define SDDSTOBULKIOPROCESSOR_H_

#include <boost/shared_ptr.hpp>
#include <deque>
#include <vector>

#include "SmartPacketBuffer.h"
#include "ossie/debug.h"
#include "sddspacket.h"
#include "bulkio.h"

#define SDDS_PACKET_SIZE 1080
#define SDDS_DATA_SIZE 1024
#define DEFAULT_PKTS_PER_READ 500
#define CORBA_MAX_XFER_BYTES omniORB::giopMaxMsgSize() - 2048

typedef boost::shared_ptr<SDDSpacket> SddsPacketPtr;

class SddsToBulkIOProcessor {
	ENABLE_LOGGING
public:
	SddsToBulkIOProcessor(bulkio::OutOctetPort *octet_out, bulkio::OutShortPort *short_out, bulkio::OutFloatPort *float_out);
	virtual ~SddsToBulkIOProcessor();
	void run(SmartPacketBuffer<SDDSpacket> *pktbuffer);
	void setPktsPerRead(size_t pkts_per_read);
	void shutDown();
	void setWaitForTTV(bool wait_for_ttv);
	void setPushOnTTV(bool push_on_ttv);
	void setUpstreamSri(BULKIO::StreamSRI upstream_sri);
	void unsetUpstreamSri();
	size_t getPktsPerRead();
	bool getPushOnTTV();
	bool getWaitOnTTV();
	unsigned short getBps();
	unsigned long long getNumDropped();
	uint16_t getExpectedSequenceNumber();
	std::string getStreamId();
	double getSampleRate();
	std::string getEndianness();
	void setEndianness(std::string endianness);
	long getTimeSlips();
private:
	size_t m_pkts_per_read;
	bool m_running;
	bool m_shuttingDown;
	bool m_wait_for_ttv;
	bool m_push_on_ttv;
	bool m_first_packet;
	bool m_current_ttv_flag;
	uint16_t m_expected_seq_number;
	std::vector<uint8_t> m_bulkIO_data;
	SDDSTime m_last_sdds_time;
	unsigned long long m_pkts_dropped;
	time_t m_start_of_year;
	unsigned short m_bps;
	BULKIO::StreamSRI m_sri;
	BULKIO::PrecisionUTCTime m_bulkio_time_stamp;
	bulkio::OutOctetPort *m_octet_out;
	bulkio::OutShortPort *m_short_out;
	bulkio::OutFloatPort *m_float_out;
	BULKIO::StreamSRI m_upstream_sri;
	bool m_upstream_sri_set;
	std::string m_endianness;
	bool m_new_upstream_sri;
	bool m_use_upstream_sri;
	long m_num_time_slips;
	double m_current_sample_rate;
	double m_max_time_step, m_min_time_step, m_ideal_time_step, m_time_error_accum, m_accum_error_tolerance;
	bool m_non_conforming_device;
	boost::mutex m_upstream_sri_lock;
    bulkio::OutFloatStream floatStream;
    bulkio::OutShortStream shortStream;
    bulkio::OutOctetStream octetStream;

	void processPackets(std::deque<SddsPacketPtr> &pktsToWork, std::deque<SddsPacketPtr> &pktsToRecycle);
	bool orderIsValid(SddsPacketPtr &pkt);
	void pushSri();
	void checkForTimeSlip(SddsPacketPtr &pkt);
	void updateExpectedXdelta(double rate, bool complex);
	void createOutputStreams();
};

#endif /* SDDSTOBULKIOPROCESSOR_H_ */
