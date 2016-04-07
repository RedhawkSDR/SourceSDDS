/*
 * SddsToBulkIOProcessor.h
 *
 *  Created on: Apr 5, 2016
 *      Author: ylbagou
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
	void setStreamId(std::string stream_id);
	size_t getPktsPerRead();
	bool getPushOnTTV();
	bool getWaitOnTTV();
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
	long long m_last_wsec, m_pkts_dropped;
	time_t m_start_of_year;
	unsigned short m_bps;
	BULKIO::StreamSRI m_sri;
	BULKIO::PrecisionUTCTime m_bulkio_time_stamp;
	bulkio::OutOctetPort *m_octet_out;
	bulkio::OutShortPort *m_short_out;
	bulkio::OutFloatPort *m_float_out;
	std::string m_stream_id;

	void processPackets(std::deque<SddsPacketPtr> &pktsToWork, std::deque<SddsPacketPtr> &pktsToRecycle);
	bool orderIsValid(SddsPacketPtr &pkt);
	void pushPacket();
	void pushSri();
};

#endif /* SDDSTOBULKIOPROCESSOR_H_ */
