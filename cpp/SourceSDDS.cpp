/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "SourceSDDS.h"
#include <signal.h>
#include "AffinityUtils.h"

PREPARE_LOGGING(SourceSDDS_i)

SourceSDDS_i::SourceSDDS_i(const char *uuid, const char *label) :
    SourceSDDS_base(uuid, label),
	m_socketReaderThread(NULL),
	m_sddsToBulkIOThread(NULL)
{
}

SourceSDDS_i::~SourceSDDS_i()
{
}

void SourceSDDS_i::start() throw (CORBA::SystemException, CF::Resource::StartError) {
	// TODO: Should we start all this processing up if nobody has connected to our ports?

	if (started()) {
		LOG_WARN(SourceSDDS_i, "Already started, call to start ignored.");
		return;
	}

	// This also destroys all of our buffers
	destroyBuffersAndJoinThreads();

	// Initialize our buffer of packets
	m_pktbuffer.initialize(advanced_optimizations.buffer_size);

	//////////////////////////////////////////
	// Setup the socketReader
	//////////////////////////////////////////
	setupConnectionOptions();
	m_socketReaderThread = new boost::thread(boost::bind(&SocketReader::run, boost::ref(m_socketReader), &m_pktbuffer));

	// Attempt to set the affinity of the socket reader thread if the user has told us to.
	if (!advanced_optimizations.socket_read_thread_affinity.empty() && !(advanced_optimizations.socket_read_thread_affinity == "")) {
		setAffinity(m_socketReaderThread->native_handle(), advanced_optimizations.socket_read_thread_affinity);
	}

	advanced_optimizations.socket_read_thread_affinity = getAffinity(m_socketReaderThread->native_handle());

	//////////////////////////////////////////
	// Now setup the packet processor
	//////////////////////////////////////////
	m_sddsToBulkIOThread = new boost::thread(boost::bind(&SddsToBulkIOProcessor::run, boost::ref(m_sddsToBulkIO), &m_pktbuffer));

	// Call the parent start
	SourceSDDS_base::start();
}

void SourceSDDS_i::stop () throw (CF::Resource::StopError, CORBA::SystemException) {
	destroyBuffersAndJoinThreads();
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
void SourceSDDS_i::destroyBuffersAndJoinThreads() {

	// Its possible that the socket readers could be caught waiting on a buffer so call shutdown
	// then destroy the buffers to break the blocking call. It may return buffers back to the queue so we can call destroy again
	// at the end.  It shouldn't hurt...right?
	LOG_DEBUG(SourceSDDS_i, "Shutting down the socket reader thread");
	m_socketReader.shutDown();
	LOG_DEBUG(SourceSDDS_i, "Shutting down the sdds to bulkio thread");
	m_sddsToBulkIO.shutDown();

	LOG_DEBUG(SourceSDDS_i, "Destroying the existing packet buffers");
	m_pktbuffer.destroyBuffers();

	// Delete the socket reader and sddsToBulkIO thread if it already exists
	if (m_socketReaderThread) {
		LOG_DEBUG(SourceSDDS_i, "Joining the socket reader thread");
		m_socketReaderThread->join();
		delete m_socketReaderThread;
		m_socketReaderThread = NULL;
	}

	if (m_sddsToBulkIOThread) {
		LOG_DEBUG(SourceSDDS_i, "Joining the sdds to bulkio thread");
		m_sddsToBulkIOThread->join();
		delete m_sddsToBulkIOThread;
		m_sddsToBulkIOThread = NULL;
	}

	LOG_DEBUG(SourceSDDS_i, "Destroying any packet buffers added during thread shutdown");
	// Calling destroy a second time on purpose in case during thread shutdown the threads recycled some buffers.
	m_pktbuffer.destroyBuffers();

	LOG_DEBUG(SourceSDDS_i, "Everything should be shutdown and joined");
}

int SourceSDDS_i::serviceFunction()
{
    LOG_INFO(SourceSDDS_i, "Component has no service function, returning FINISHED");
    return FINISH;
}

