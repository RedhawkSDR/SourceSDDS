#ifndef CPP_SOURCENICUTILS_H_
#define CPP_SOURCENICUTILS_H_

#include <errno.h>
#include <debug.h>
#include <stdexcept>
#include <string>


/**
 *   Exception thrown when verify fails
 */
class BadParameterError : public std::runtime_error {
 public:
    BadParameterError(const std::string& what_arg) :
         std::runtime_error(what_arg) {
    }
};

class SourceNicUtils {
	ENABLE_LOGGING

public:
	static const int max_bufsize = 1024;

	/**
	 *  Performs a verification on the condition, else
	 *  logs an error and raises a BadParameterException
	 */
	static void
	verify_ (
	    int condition,         // condition value (must be true)
	    const char* message,   // Failure message
	    const char* condtext,  // Textual representation of condition
	    const char* file,      // File location of condition
	    int line,              // Line number of condition
	    int errno_);           // Error number (0 to exclude)

	/**
	 *  Logs an error and raises a BadParameterException
	 */
	static void
	verify_error (
	    const char* message,   // Failure message
	    const char* condtext,  // Textual representation of condition
	    const char* file,      // File location of condition
	    int line,              // Line number of condition
	    int errno_);           // Error number (0 to exclude)

};


/**
 *    verify true condition, else log message and throw BadParameterExcption
 */
#define VERIFY(CONDITION, MESSAGE) \
     if (!(CONDITION)) { \
         SourceNicUtils::verify_error(MESSAGE, #CONDITION, __FILE__, __LINE__, 0); \
     }

/**
 *    verify true condition, else log errno based message and throw BadParameterExcption
 */
#define VERIFY_ERR(CONDITION, MESSAGE) \
     if (!(CONDITION)) { \
         SourceNicUtils::verify_error(MESSAGE, #CONDITION, __FILE__, __LINE__, errno); \
     }

#endif  // CPP_SOURCENICUTILS_H_
