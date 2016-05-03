#include "SddsToBulkIOUtils.h"
#include "ossie/debug.h"

namespace ENDIANNESS {
	const std::string BIG_ENDIAN_STR = "4321";
	const std::string LITTLE_ENDIAN_STR = "1234";
}

/****************************************************************************************
 * setStartOfYear()
 *
 * Takes:   void
 * Returns: void
 *
 * Functionality:
 *    Calculates the number of seconds that have passed from the EPOCH (Jan 1 1970) to
 *    00:00 Jan 1 of the current year.  This value is used to build the PrecisionUTC
 *    Time Tag from the time tag in the SDDS packets
 ****************************************************************************************/
time_t getStartOfYear() {
	time_t systemtime;
	tm *systemtime_struct;

	time(&systemtime);

	/* System Time in a struct of day, month, year */
	systemtime_struct = gmtime(&systemtime);

	/* Find time from EPOCH to Jan 1st of current year */
	systemtime_struct->tm_sec = 0;
	systemtime_struct->tm_min = 0;
	systemtime_struct->tm_hour = 0;
	systemtime_struct->tm_mday = 1;
	systemtime_struct->tm_mon = 0;
	return (mktime(systemtime_struct) - timezone);
} // end function setStartOfYear


/**
 * Provides the bulkIO timestamp given the sdds_pkt.
 * startOfYear is the value calculated from the getStartOfYear function and is updated if the year has rolled over.
 * lastWSec is the last whole number of seconds from the SDDS Packet and is updated each time. It is used to determine if the year has rolled over.
 */
BULKIO::PrecisionUTCTime getBulkIOTimeStamp(SDDSpacket* sdds_pkt, const SDDSTime &last_sdds_time, time_t &startOfYear) {
	BULKIO::PrecisionUTCTime T;

	// TODO: Originally the SourceNIC component always set this to TCS_VALID, why? Which is correct?
	if (sdds_pkt->get_ttv()) {
		T.tcstatus = BULKIO::TCS_VALID; //Time tag is valid
	} else {
		T.tcstatus = BULKIO::TCS_INVALID; //Time tag is not valid
	}

	T.tcmode = BULKIO::TCM_SDDS; //Timecode mode is SDDS
	T.toff = 0; //The sample offset from the first sample time code is 0

	// TODO: Why not just use the provided t.seconds()?
	SDDSTime t = sdds_pkt->get_SDDSTime();
	unsigned long long frac_int = t.ps250() % 4000000000UL;
	unsigned long long secs_int = t.ps250() - frac_int;
	double frac_flt = frac_int / 4e9;
	double secs_flt = secs_int / 4e9;
	double ext_flt = 250e-12 * t.pf250() / 4294967296.0;
	frac_flt += ext_flt;

	/* this means the current year changed */
	if (t < last_sdds_time) {
		RH_NL_INFO("SddsToBulkIOUtils", "Looks as though the year rolled over while processing SDDS time stamps. Happy New Year!");
		startOfYear = getStartOfYear();
	}

	T.twsec = secs_flt + startOfYear; //add in the startofyear offset
	T.tfsec = frac_flt;

	return T;
}

void getWholeAndFracSec(SDDSpacket* sdds_pkt, uint64_t &whole_sec, uint64_t &frac_sec, time_t &startOfYear) {
	SDDSTime t = sdds_pkt->get_SDDSTime();
	unsigned long long frac_int = t.ps250() % 4000000000UL;
	unsigned long long secs_int = t.ps250() - frac_int;
	double frac_flt = frac_int / 4e9;
	double secs_flt = secs_int / 4e9;
	double ext_flt = 250e-12 * t.pf250() / 4294967296.0;
	frac_flt += ext_flt;

	/* this means the current year changed */
	if (secs_flt < (double) whole_sec) {
		startOfYear = getStartOfYear();
	}
	whole_sec = (uint64_t) secs_flt;
	frac_sec = (uint64_t) frac_flt;
}

/**
 * Returns the bits per sample.
 * The SDDS packet only has 5 bits for this field so a value of 31 is equivalent to 32 bits.
 */
unsigned short getBps(SDDSpacket* sdds_pkt) {
	return (sdds_pkt->bps == 31) ? (32) : (sdds_pkt->bps);
}


/**
 * Sets the bulkIO SRI objects xdelta and mode based on the sdds_pkt content.
 * If values within the SRI object are changed, the changed boolean is set to true.
 * If no values within the SRI object is changed the boolean is set to false.
 */
void mergeSddsSRI(SDDSpacket* sdds_pkt, BULKIO::StreamSRI &sri, bool &changed, bool non_conforming_device) {

	CORBA::Double recXdelta = (CORBA::Double)(1.0 / sdds_pkt->get_rate());

	if (non_conforming_device) {
		recXdelta *= 0.5;
	}

	if (recXdelta != sri.xdelta) {
		changed = true;
		sri.xdelta = recXdelta;
	}

	short recMode = (short) sdds_pkt->cx;
	if (recMode != sri.mode) {
		sri.mode = recMode;
		changed = true;
	}

} // end function mergeSddsSRI

bool compareSRI(BULKIO::StreamSRI A, BULKIO::StreamSRI B) {
	bool same = false;

	if ((A.hversion == B.hversion) and (A.xstart == B.xstart) and (A.xunits == B.xunits) and (A.ystart == B.ystart) and (A.ydelta == B.ydelta)
			and
			/* these values aren't checked because we get them from the sdds packet headers */
			//(A.mode == B.mode) and
			//(A.xdelta == B.xdelta) and
			//(A.subsize == B.subsize) and
			(!strcmp(A.streamID, B.streamID))) {
		same = true;
	} else {
		same = false;
		return same;
	}

	if (A.keywords.length() != B.keywords.length()) {
		same = false;
		return same;
	}

	for (unsigned int i = 0; i < A.keywords.length(); i++) {
		std::string action = "ne";
		if (ossie::compare_anys(A.keywords[i].value, B.keywords[i].value, action)) {
			same = false;
			return same;
		}
	}

	return same;
}

void mergeUpstreamSRI(BULKIO::StreamSRI &currSRI, BULKIO::StreamSRI &upstreamSRI, bool &useUpstream, bool &changed, std::string &endianness) {
	if (!compareSRI(currSRI, upstreamSRI)) {
		changed = true;
		currSRI.streamID = upstreamSRI.streamID;

		currSRI.hversion = upstreamSRI.hversion;
		currSRI.xstart = upstreamSRI.xstart;

		/* Platinum time code (1 == seconds) */
		currSRI.xunits = upstreamSRI.xunits;

		/* # frames to be delivered by pushPacket() call; set to 0 for single packet */
		currSRI.ystart = upstreamSRI.ystart;
		currSRI.ydelta = upstreamSRI.ydelta;
		currSRI.yunits = upstreamSRI.yunits;

		currSRI.keywords.length(upstreamSRI.keywords.length());

		for (unsigned int i = 0; i < currSRI.keywords.length(); i++) {
			currSRI.keywords[i].id = upstreamSRI.keywords[i].id;
			currSRI.keywords[i].value = upstreamSRI.keywords[i].value;

			if (std::string(upstreamSRI.keywords[i].id) == "BULKIO_SRI_PRIORITY" || std::string(upstreamSRI.keywords[i].id) == "use_BULKIO_SRI" || std::string(upstreamSRI.keywords[i].id) == "sddsPacketAlt") {
				useUpstream = true;
			} else if (std::string(upstreamSRI.keywords[i].id) == "dataRef" || std::string(upstreamSRI.keywords[i].id) == "DATA_REF_STR") {
				endianness = ossie::any_to_string(upstreamSRI.keywords[i].value);
				if (endianness == "43981") { // In hex this is 0xabcd
					endianness = ENDIANNESS::LITTLE_ENDIAN_STR;
				} else if (endianness == "52651") {
					endianness = ENDIANNESS::BIG_ENDIAN_STR;
				}

				currSRI.keywords[i].value <<= CORBA::string_dup(endianness.c_str());
			}
		}

		if (useUpstream) {
			currSRI.xdelta = upstreamSRI.xdelta;
			currSRI.mode = upstreamSRI.mode;
		}
	}
}
