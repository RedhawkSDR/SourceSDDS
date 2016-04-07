#ifndef AFFINITYUTILS_H_
#define AFFINITYUTILS_H_

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <boost/lexical_cast.hpp>

template <typename T>
T ConvertString(const std::string &data, bool &success)
{
  success = true;
  if(!data.empty())
  {
    for (size_t i=0; i < data.length(); ++i) {
      if (!isxdigit(data[i])) {
        std::cout << "1" << std::endl;
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
        std::cout << "2" << std::endl;
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
		// TODO: Log error
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

#endif
