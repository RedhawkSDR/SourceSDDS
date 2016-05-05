#ifndef SDDSTOBULKIOUTILS_CPP_
#define SDDSTOBULKIOUTILS_CPP_

#include "sddspacket.h"
#include <bulkio/bulkio.h>

namespace ENDIANNESS {
	const std::string BIG_ENDIAN_STR = "4321";
	const std::string LITTLE_ENDIAN_STR = "1234";
	const std::string ENDIAN_DEFAULT = BIG_ENDIAN_STR;
}

time_t getStartOfYear();
BULKIO::PrecisionUTCTime getBulkIOTimeStamp(SDDSpacket* sdds_pkt, const SDDSTime &last_sdds_time, time_t &startOfYear);
unsigned short getBps(SDDSpacket* sdds_pkt);
void mergeSddsSRI(SDDSpacket* sdds_pkt, BULKIO::StreamSRI &sri, bool &changed, bool non_conforming_device);
void mergeUpstreamSRI(BULKIO::StreamSRI &currSRI, BULKIO::StreamSRI &upstreamSRI, bool &useUpstream, bool &changed, std::string &endianness);

#endif
