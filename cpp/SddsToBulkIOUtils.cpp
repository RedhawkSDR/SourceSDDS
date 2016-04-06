#include "SddsToBulkIOUtils.h"

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
BULKIO::PrecisionUTCTime getBulkIOTimeStamp(SDDSpacket* sdds_pkt, long long &lastWSec, time_t &startOfYear) {
	BULKIO::PrecisionUTCTime T;

	// TODO: Originally the SourceNIC component always set this to TCS_VALID, why? Which is correct?
	if (sdds_pkt->get_ttv()) {
		T.tcstatus = BULKIO::TCS_VALID; //Time tag is valid
	} else {
		T.tcstatus = BULKIO::TCS_INVALID; //Time tag is not valid
	}

	T.tcmode = BULKIO::TCM_SDDS; //Timecode mode is SDDS
	T.toff = 0; //The sample offset from the first sample time code is 0

	SDDSTime t = sdds_pkt->get_SDDSTime();
	unsigned long long frac_int = t.ps250() % 4000000000UL;
	unsigned long long secs_int = t.ps250() - frac_int;
	double frac_flt = frac_int / 4e9;
	double secs_flt = secs_int / 4e9;
	double ext_flt = 250e-12 * t.pf250() / 4294967296.0;
	frac_flt += ext_flt;

	/* this means the current year changed */
	if (secs_flt < (double) lastWSec) {
		startOfYear = getStartOfYear();
	}
	lastWSec = (long long) secs_flt;

	T.twsec = secs_flt + startOfYear; //add in the startofyear offset
	T.tfsec = frac_flt;

	return T;
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
void mergeSddsSRI(SDDSpacket* sdds_pkt, BULKIO::StreamSRI &sri, bool &changed) {

	/* Extract the appropriate portions of the SDDSpacket to populate the StreamSRI structure */
	changed = false;

	CORBA::Double recXdelta = (CORBA::Double)(1.0 / sdds_pkt->get_rate());
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
