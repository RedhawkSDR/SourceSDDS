#ifndef AFFINITYUTILS_H_
#define AFFINITYUTILS_H_

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <boost/lexical_cast.hpp>
#include "ossie/debug.h"

template <typename T>
T ConvertString(const std::string &data, bool &success)
{
  success = true;
  if(!data.empty())
  {
    for (size_t i=0; i < data.length(); ++i) {
      if (!isxdigit(data[i])) {
        success = false;
        return T();
      }
    }

    T ret;
    std::istringstream iss(data);
    iss >> std::hex >> ret;

    if(iss.fail())
    {
      success = false;
      return T();
    }
    return ret;
  } else {
    success = false;
  }
  return T();
}


uint64_t ipow(uint64_t base, uint64_t exp)
{
	uint64_t result = 1;
    while (exp)
    {
        if (exp & 1)
            result *= base;
        exp >>= 1;
        base *= base;
    }

    return result;
}

int setAffinity(pthread_t thread, std::string str_mask) {
	cpu_set_t cpuset;
	uint64_t mask;
	int s,j;
	bool success = false;

	mask = ConvertString<uint64_t>(str_mask, success);
	if (not success) {
		return -1;
	}

	CPU_ZERO(&cpuset);

	// This loop runs through 1024 possible CPUs (currently that's what the setsize is)
	for (j = 0; j < CPU_SETSIZE; j++) {
		if (ipow(2,j) & mask) {
			CPU_SET(j, &cpuset);
		}
	}

	s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (s != 0) {
		return -1;
	}
	return 0;
}

std::string getAffinity(pthread_t thread) {
	int s, j;
	cpu_set_t cpuset;

	/* Set affinity mask to include CPUs 0 to 7 */

	CPU_ZERO(&cpuset);

	/* Check the actual affinity mask assigned to the thread */
	s = pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (s != 0) {
		RH_NL_WARN("SourceSDDSUtils", "Could not get affinity for given thread");
		return "";
	}


	uint64_t mask = 0;

	// This loop runs through 1024 possible CPUs
	for (j = 0; j < CPU_SETSIZE; j++) {
	   if (CPU_ISSET(j, &cpuset)) {
		   mask |= ipow(2,j);  // TODO: This will obviously break on a system that has more than 64 cores. Fix it.
	   }
	}

	std::stringstream stream;
	stream << std::uppercase << std::hex << mask;
	return stream.str();
}

static uint64_t get_rx_queue(std::string ip, uint16_t port, int &num_listeners) {
	// Im not super happy with this but there doesnt seem to be any other API to get the current
	// buffer "fullness" other than the proc file system....so we're file parsing in C++ to get a kernel
	// value.

	// Taken from: http://www.makelinux.net/alp/050

	FILE* fp;
	char buffer[1024*1024];  // Too big?
	size_t bytes_read;
	char* match;

	// local_address rem_address   st tx_queue:rx_queue
	char local_address[128];
	char rem_address[128];
	char st[128];
	char tx_rx_queue[128];

	uint64_t rx_queue = 0;
	num_listeners = 0;

	/* Read the entire contents of /proc/net/udp into the buffer.  */
	fp = fopen ("/proc/net/udp", "r");
	if (fp == NULL) {
		RH_NL_DEBUG("SourceSDDSUtils", "Failed to open /proc/net/udp");
		return 0;
	}
	bytes_read = fread (buffer, 1, sizeof (buffer), fp);
	fclose (fp);

	/* Bail if read failed or if buffer isn't big enough.  */
	if (bytes_read == 0 || bytes_read == sizeof (buffer)) {
	 return 0;
	}

	/* NUL-terminate the text.  */
	buffer[bytes_read] = '\0';

	/* Locate the line that starts with our ip and port  */
	in_addr_t ip_add = inet_addr(ip.c_str());
	std::stringstream ss;
	ss << std::uppercase << std::hex << ip_add << ":" << port;

	// Find all occurrences and return the max
	match = strstr (buffer, ss.str().c_str());
	if (match == NULL) {
		RH_NL_DEBUG("SourceSDDSUtils", "In parsing /proc/net/udp the IP and port not found, is the socket bound?");
		return 0;
	}

	while (match) {
		num_listeners++;
		/* Parse the line to extract the tx_queue:rx_queue.  */
		// 00000000:0801 00000000:0000 07 00000000:00000000
		sscanf (match, "%s %s %s %s", &local_address[0], &rem_address[0], &st[0], &tx_rx_queue[0]);

		std::string rx_queue_str(tx_rx_queue);
		std::size_t colon_pos = rx_queue_str.find(':');

		if (colon_pos == std::string::npos) {
			RH_NL_WARN("SourceSDDSUtils", "Failed to properly parse the UDP socket buffer information from /proc/net/udp");
			return 0;
		}

		rx_queue_str = rx_queue_str.substr(colon_pos+1, rx_queue_str.size());

		bool success;
		uint64_t tmp_rx_queue = ConvertString<uint64_t>(rx_queue_str, success);

		if (success) {
			rx_queue = std::max(rx_queue, tmp_rx_queue);
		}


		// Move the pointer along
		match++;
		match = strstr (match, ss.str().c_str());
	}

	return rx_queue;
}

static int64_t get_rx_dropped(std::string interface) {
	FILE* fp;
	char buffer[1024];
	size_t bytes_read;

	std::stringstream ss;
	ss << "/sys/class/net/" << interface << "/statistics/rx_dropped";

	fp = fopen(ss.str().c_str(), "r");
	if (!fp) {
		RH_NL_DEBUG("SourceSDDSUtils", "Failed to open " << ss.str());
		return -1;
	}
	bytes_read = fread(buffer, 1, sizeof (buffer), fp);
	fclose (fp);

	/* Bail if read failed or if buffer isn't big enough.  */
	if (bytes_read == 0 || bytes_read == sizeof (buffer)) {
	 return -1;
	}

	/* NUL-terminate the text.  */
	buffer[bytes_read] = '\0';

	return atol(buffer);
}

int setPolicyAndPriority(pthread_t thread, CORBA::Long priority, std::string thread_desc) {
	struct sched_param param;
	int retVal = 0;
	param.sched_priority = priority;

	if (priority > 0) {
		retVal = pthread_setschedparam(thread, SCHED_RR, &param);
	} else if (priority == 0) {
		retVal = pthread_setschedparam(thread, SCHED_OTHER, &param);
	}

	if (retVal != 0) {
		RH_NL_WARN("SourceSDDSUtils", "Failed to set " << thread_desc << " policy and priority. Permissions may prevent this or values provided may be invalid");
	}

	return retVal;
}

int getPriority(pthread_t thread, CORBA::Long &priority, std::string thread_desc) {
	int retVal = 0, policy;
	struct sched_param param;
	retVal = pthread_getschedparam(thread, &policy, &param);
	if (retVal != 0) {
		RH_NL_WARN("SourceSDDSUtils", "Could not get priority of the " << thread_desc);
	} else {
		priority = param.sched_priority;
	}

	return retVal;
}


#endif
