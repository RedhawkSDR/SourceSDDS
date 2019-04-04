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
#include <unistd.h>
#include "SourceNicUtils.h"

void SourceNicUtils::verify_ (
    int condition,         // condition value (must be true)
    const char* message,   // Failure message
    const char* condtext,  // Textual representation of condition
    const char* file,      // File location of condition
    int line,              // Line number of condition
    int errno_,            // Error number (0 to exclude)
    LOGGER _log)           // RH logger
{
    if (!_log) {
        _log = rh_logger::Logger::getLogger("SourceSDDS_utils");
        RH_DEBUG(_log, "verify method passed null logger; creating logger "<<_log->getName());
    } else {
        RH_DEBUG(_log, "verify method passed valid logger "<<_log->getName());
    }
    if (!condition) {
        SourceNicUtils::verify_error(message, condtext, file, line, errno_, _log);
    }
};

void SourceNicUtils::verify_error (
    const char* message,   // Failure message
    const char* condtext,  // Textual representation of condition
    const char* file,      // File location of condition
    int line,              // Line number of condition
    int errno_,            // Error number (0 to exclude)
    LOGGER _log)           // RH logger
{
    if (!_log) {
        _log = rh_logger::Logger::getLogger("SourceSDDS_utils");
        RH_DEBUG(_log, "verify_error method passed null logger; creating logger "<<_log->getName());
    } else {
        RH_DEBUG(_log, "verify_error method passed valid logger "<<_log->getName());
    }

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
    RH_ERROR(_log, msg);
    throw BadParameterError(msg);
};
