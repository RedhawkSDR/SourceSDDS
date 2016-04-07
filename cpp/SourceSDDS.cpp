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
	m_sddsToBulkIOThread(NULL),
	m_sddsToBulkIO(octet_out, short_out, float_out)
{
	setPropertyQueryImpl(advanced_configuration, this, &SourceSDDS_i::get_advanced_configuration_struct);
	setPropertyQueryImpl(advanced_optimizations, this, &SourceSDDS_i::get_advanced_optimizations_struct);

	setPropertyConfigureImpl(advanced_configuration, this, &SourceSDDS_i::set_advanced_configuration_struct);
	setPropertyConfigureImpl(advanced_optimizations, this, &SourceSDDS_i::set_advanced_optimization_struct);
}

SourceSDDS_i::~SourceSDDS_i()
{
}

struct advanced_configuration_struct SourceSDDS_i::get_advanced_configuration_struct() {
	struct advanced_configuration_struct retVal;
	retVal.push_on_ttv = m_sddsToBulkIO.getPushOnTTV();
	retVal.wait_on_ttv = m_sddsToBulkIO.getWaitOnTTV();
	return retVal;
}

struct advanced_optimizations_struct SourceSDDS_i::get_advanced_optimizations_struct() {
	// Most of the variables are kept track of in their respective classes
	// The only ones kept track of in this class is the buffer_size and the affinity values.

	struct advanced_optimizations_struct retVal;
	retVal.buffer_size = advanced_optimizations.buffer_size;
	retVal.pkts_per_socket_read = m_socketReader.getPktsPerRead();
	retVal.sdds_pkts_per_bulkio_push = m_sddsToBulkIO.getPktsPerRead();
	retVal.sdds_to_bulkio_thread_affinity = advanced_optimizations.sdds_to_bulkio_thread_affinity;
	retVal.socket_read_thread_affinity = advanced_optimizations.socket_read_thread_affinity;
	retVal.udp_socket_buffer_size = m_socketReader.getSocketBufferSize();
	return retVal;
}

void SourceSDDS_i::set_advanced_configuration_struct(struct advanced_configuration_struct request) {
	m_sddsToBulkIO.setPushOnTTV(request.push_on_ttv);
	m_sddsToBulkIO.setWaitForTTV(request.wait_on_ttv);
}

void SourceSDDS_i::set_advanced_optimization_struct(struct advanced_optimizations_struct request) {
	if (started() && advanced_optimizations.buffer_size != request.buffer_size) {
		LOG_WARN(SourceSDDS_i, "Cannot set the buffer size while the component is running");
	} else {
		advanced_optimizations.buffer_size = request.buffer_size;
	}

	if (not started()) {
		m_socketReader.setPktsPerRead(request.pkts_per_socket_read);
	} else if(m_socketReader.getPktsPerRead() != request.pkts_per_socket_read) {
		LOG_WARN(SourceSDDS_i, "Cannot set the buffer size while the component is running");
	}

	if (not started()) {
		m_sddsToBulkIO.setPktsPerRead(request.sdds_pkts_per_bulkio_push);
	} else if (m_sddsToBulkIO.getPktsPerRead() != request.sdds_pkts_per_bulkio_push) {
		LOG_WARN(SourceSDDS_i, "Cannot set the packets per bulkIO push while the component is running");
	}

	if (started() && m_sddsToBulkIOThread) {
		if (setAffinity(m_sddsToBulkIOThread->native_handle(), request.sdds_to_bulkio_thread_affinity) != 0) {
			LOG_WARN(SourceSDDS_i, "Failed to set affinity of the SDDS to bulkIO thread");
		}
		advanced_optimizations.sdds_to_bulkio_thread_affinity = getAffinity(m_sddsToBulkIOThread->native_handle());
	} else {
		advanced_optimizations.sdds_to_bulkio_thread_affinity = request.sdds_to_bulkio_thread_affinity;
	}

	if (started() && m_socketReaderThread) {
		if (setAffinity(m_socketReaderThread->native_handle(), request.socket_read_thread_affinity) != 0) {
			LOG_WARN(SourceSDDS_i, "Failed to set affinity of the socket reader thread");
		}
		advanced_optimizations.socket_read_thread_affinity = getAffinity(m_socketReaderThread->native_handle());
	} else {
		advanced_optimizations.socket_read_thread_affinity = request.socket_read_thread_affinity;
	}

	if (not started()) {
		m_socketReader.setSocketBufferSize(request.udp_socket_buffer_size);
	} else if (m_socketReader.getSocketBufferSize() != request.udp_socket_buffer_size) {
		LOG_WARN(SourceSDDS_i, "Cannot set the socket buffer size while the component is running");
	}
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
	setupSocketReaderOptions();
	m_socketReaderThread = new boost::thread(boost::bind(&SocketReader::run, boost::ref(m_socketReader), &m_pktbuffer));

	// Attempt to set the affinity of the socket reader thread if the user has told us to.
	if (!advanced_optimizations.socket_read_thread_affinity.empty() && !(advanced_optimizations.socket_read_thread_affinity == "")) {
		setAffinity(m_socketReaderThread->native_handle(), advanced_optimizations.socket_read_thread_affinity);
	}

	advanced_optimizations.socket_read_thread_affinity = getAffinity(m_socketReaderThread->native_handle());

	//////////////////////////////////////////
	// Now setup the packet processor
	//////////////////////////////////////////
	setupSddsToBulkIOOptions();
	m_sddsToBulkIOThread = new boost::thread(boost::bind(&SddsToBulkIOProcessor::run, boost::ref(m_sddsToBulkIO), &m_pktbuffer));

	// Attempt to set the affinity of the sdds to bulkio thread if the user has told us to.
	if (!advanced_optimizations.sdds_to_bulkio_thread_affinity.empty() && !(advanced_optimizations.sdds_to_bulkio_thread_affinity== "")) {
		setAffinity(m_sddsToBulkIOThread->native_handle(), advanced_optimizations.sdds_to_bulkio_thread_affinity);
	}

	advanced_optimizations.sdds_to_bulkio_thread_affinity = getAffinity(m_sddsToBulkIOThread->native_handle());


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
void SourceSDDS_i::setupSocketReaderOptions() {
	if (attachment_override.enabled) {
		m_socketReader.setConnectionInfo(attachment_override.ip_address, attachment_override.vlan, attachment_override.port);
	} else {
		// TODO: Get the settings from the SDDS port
	}

	m_socketReader.setPktsPerRead(advanced_optimizations.pkts_per_socket_read);


}

void SourceSDDS_i::setupSddsToBulkIOOptions() {
	m_sddsToBulkIO.setPktsPerRead(advanced_optimizations.sdds_pkts_per_bulkio_push);
	advanced_optimizations.sdds_pkts_per_bulkio_push = m_sddsToBulkIO.getPktsPerRead();

	m_sddsToBulkIO.setPushOnTTV(advanced_configuration.push_on_ttv);
	m_sddsToBulkIO.setWaitForTTV(advanced_configuration.wait_on_ttv);
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
    LOG_DEBUG(SourceSDDS_i, "Component has no service function, returning FINISHED");
    return FINISH;
}

