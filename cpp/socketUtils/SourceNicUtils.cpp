#include <unistd.h>
#include <string.h>
#include <debug.h>
#include "SourceNicUtils.h"

PREPARE_LOGGING(SourceNicUtils)

void
SourceNicUtils::verify_ (
    int condition,         // condition value (must be true)
    const char* message,   // Failure message
    const char* condtext,  // Textual representation of condition
    const char* file,      // File location of condition
    int line,              // Line number of condition
    int errno_)            // Error number (0 to exclude)
{
  if (!condition) {
	  SourceNicUtils::verify_error(message, condtext, file, line, errno_);
  }
};
 void
SourceNicUtils::verify_error (
    const char* message,   // Failure message
    const char* condtext,  // Textual representation of condition
    const char* file,      // File location of condition
    int line,              // Line number of condition
    int errno_)            // Error number (0 to exclude)
{
    char msg[SourceNicUtils::max_bufsize];
    char errstr[512];

    if (errno_ != 0) {
        snprintf(msg, sizeof(msg),
                 "Verify '%s' failed at line %d: %s [errno %d: %s] (%s)",
                 file, line, message, errno_,
                 strerror_r(errno_, errstr, sizeof(errstr)),
                 condtext);
    } else {
        snprintf(msg, sizeof(msg),
                 "Verify '%s' failed at line %d: %s (%s)",
                 file, line,
                 message, condtext);
    }
    LOG_ERROR(SourceNicUtils, msg)
    throw BadParameterError(msg);
};
