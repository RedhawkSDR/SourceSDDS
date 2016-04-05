/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "SourceSDDS.h"
#include <signal.h>

PREPARE_LOGGING(SourceSDDS_i)

SourceSDDS_i::SourceSDDS_i(const char *uuid, const char *label) :
    SourceSDDS_base(uuid, label),
	m_socketReaderThread(NULL),
	m_sddsToBulkioThread(NULL)
{
}

SourceSDDS_i::~SourceSDDS_i()
{
}

void SourceSDDS_i::start() throw (CORBA::SystemException, CF::Resource::StartError) {
	if (started()) {
		LOG_WARN(SourceSDDS_i, "Already started, call to start ignored.");
		return;
	}


	stopAndJoinThreads();

	// Setup the socketReader
	setupConnectionOptions();
	m_pktbuffer.initialize(advanced_optimizations.buffer_size);
	m_socketReaderThread = new boost::thread(boost::bind(&SocketReader::run, boost::ref(m_socketReader), &m_pktbuffer));


	// Call the parent start
	SourceSDDS_base::start();
}

void SourceSDDS_i::stop () throw (CF::Resource::StopError, CORBA::SystemException) {
	stopAndJoinThreads();
	SourceSDDS_base::stop();
}

/**
 * Sets the IP and port on the class socket reader from either the SDDS port or the properties depending on
 * override settings.
 */
void SourceSDDS_i::setupConnectionOptions() {
	if (attachment_override.enabled) {
		m_socketReader.setConnectionInfo(attachment_override.ip_address, attachment_override.vlan, attachment_override.port);
	} else {
		// TODO: Get the settings from the SDDS port
	}

}

void SourceSDDS_i::constructor()
{
    /***********************************************************************************
     This is the RH constructor. All properties are properly initialized before this function is called
    ***********************************************************************************/
}

/**
 * Stops the socket reader thread and the SDDS to Bulkio worker threads
 */
void SourceSDDS_i::stopAndJoinThreads() {

	// Its possible that the socket readers could be caught waiting on a buffer so call shutdown
	// then destroy the buffers to break the blocking call. It may return buffers back to the queue so we can call destroy again
	// at the end.  It shouldnt hurt.
	m_socketReader.shutDown();
	m_pktbuffer.destroyBuffers();

	// Delete the socket reader and sddsToBulkIO thread if it already exists
	if (m_socketReaderThread) {
		LOG_DEBUG(SourceSDDS_i, "Shutting down the socket reader thread");
		m_socketReaderThread->join();
		delete m_socketReaderThread;
		m_socketReaderThread = NULL;
	}

	if (m_sddsToBulkioThread) {
		LOG_DEBUG(SourceSDDS_i, "Shutting down the sdds to bulkio thread");
		// TODO: Shutdown the threads activity
		m_sddsToBulkioThread->join();
		delete m_sddsToBulkioThread;
		m_sddsToBulkioThread = NULL;
	}

	LOG_DEBUG(SourceSDDS_i, "Destroying the existing packet buffers");
	// Calling destroy a second time on purpose in case during thread shutdown the threads recycled some buffers.
	m_pktbuffer.destroyBuffers();
}

int SourceSDDS_i::serviceFunction()
{
    LOG_INFO(SourceSDDS_i, "Component has no service function, returning FINISHED");
    return FINISH;
}

