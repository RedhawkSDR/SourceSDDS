#ifndef SDDSTOBULKIOUTILS_CPP_
#define SDDSTOBULKIOUTILS_CPP_

#include "sddspacket.h"
#include <bulkio/bulkio.h>

time_t getStartOfYear();
BULKIO::PrecisionUTCTime getBulkIOTimeStamp(SDDSpacket* sdds_pkt, long long &lastWSec, time_t &startOfYear);
unsigned short getBps(SDDSpacket* sdds_pkt);
void mergeSddsSRI(SDDSpacket* sdds_pkt, BULKIO::StreamSRI &sri, bool &changed);
void mergeUpstreamSRI(BULKIO::StreamSRI &currSRI, BULKIO::StreamSRI &upstreamSRI, bool &useUpstream, bool &changed, std::string &endianness);

#endif
