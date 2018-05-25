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
void mergeUpstreamSRI(BULKIO::StreamSRI &currSRI, BULKIO::StreamSRI &upstreamSRI, bool &useUpstream, bool &changed,bool &streamIDChanged, std::string &endianness);


/* floatingPointCompare is a helper function to handle floating point comparison
 * Return values:
 * if lhs == rhs: 0.0
 * if lhs >  rhs: 1.0 or greater
 * if lhs <  rhs: -1.0 or less
 * Recommended usage is to convert a comparison such as: (lhs OP rhs)
 * to (floatingPointCompare(lhs,rhs) OP 0), where OP is a comparison operator
 * (==, <, >, <=, >=, !=).
 * "places" is used to specify precision. The default is 1, which
 * uses a single decimal place of precision.
 */
inline double floatingPointCompare(double lhs, double rhs, size_t places = 1){
	return round((lhs-rhs)*pow(10,places));
}

#endif
