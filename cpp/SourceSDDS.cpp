/**************************************************************************

    This is the component code. This file contains the child class where
    custom functionality can be added to the component. Custom
    functionality to the base class can be extended here. Access to
    the ports can also be done from this class

**************************************************************************/

#include "SourceSDDS.h"
#include <signal.h>
#include "AffinityUtils.h"
#include <ossie/CF/cf.h>

PREPARE_LOGGING(SourceSDDS_i)

SourceSDDS_i::SourceSDDS_i(const char *uuid, const char *label) :
    SourceSDDS_base(uuid, label),
	m_socketReaderThread(NULL),
	m_sddsToBulkIOThread(NULL),
	m_sddsToBulkIO(octet_out, short_out, float_out)
{
	setPropertyQueryImpl(advanced_configuration, this, &SourceSDDS_i::get_advanced_configuration_struct);
	setPropertyQueryImpl(advanced_optimizations, this, &SourceSDDS_i::get_advanced_optimizations_struct);
	setPropertyQueryImpl(status, this, &SourceSDDS_i::get_status_struct);

	setPropertyConfigureImpl(advanced_configuration, this, &SourceSDDS_i::set_advanced_configuration_struct);
	setPropertyConfigureImpl(advanced_optimizations, this, &SourceSDDS_i::set_advanced_optimization_struct);

	sdds_in->setNewAttachDetachCallback(this);
	sdds_in->setNewSriListener(this, &SourceSDDS_i::newSriListener);
	sdds_in->setSriChangeListener(this, &SourceSDDS_i::newSriListener);

	m_attach_stream.attached = false;
}

SourceSDDS_i::~SourceSDDS_i()
{
}

void SourceSDDS_i::newSriListener(const BULKIO::StreamSRI & newSri) {
	LOG_INFO(SourceSDDS_i, "Received new upstream SRI");
	m_sddsToBulkIO.setUpstreamSri(newSri);
}

struct status_struct SourceSDDS_i::get_status_struct() {
	struct status_struct retVal;
	retVal.bits_per_sample = m_sddsToBulkIO.getBps();

	float percent = 100*(float) m_pktbuffer.get_num_full_buffers() / (float) advanced_optimizations.buffer_size;
	std::stringstream ss;
	ss.precision(2);
	ss << std::fixed << m_pktbuffer.get_num_full_buffers() << " (" << percent << "%)";
	retVal.buffers_to_work = ss.str();
	ss.str("");

	percent = 100*(float) m_pktbuffer.get_num_empty_buffers() / (float) advanced_optimizations.buffer_size;
	ss << std::fixed << m_pktbuffer.get_num_empty_buffers() << " (" << percent << "%)";
	retVal.empty_buffers_available = ss.str();
	ss.str("");

	retVal.dropped_packets = m_sddsToBulkIO.getNumDropped();

	retVal.expected_sequence_number = m_sddsToBulkIO.getExpectedSequenceNumber();

	// Not 100% sure why but the queue can actually get about 280 bytes larger than the set max. I guess linux gives 110% har har har (not actually 110%)
	uint64_t rx_queue = get_rx_queue(attachment_override.ip_address, attachment_override.port);
	percent = 100*(float) rx_queue / (float) m_socketReader.getSocketBufferSize();
	ss << std::fixed << rx_queue << " / " << m_socketReader.getSocketBufferSize() << " (" << percent << "%)";
	retVal.udp_socket_buffer_queue = ss.str();
	ss.str("");

	retVal.input_address = (attachment_override.enabled) ? attachment_override.ip_address:m_attach_stream.multicastAddress;
	retVal.input_port = (attachment_override.enabled) ? attachment_override.port:m_attach_stream.port;
	retVal.input_vlan = (attachment_override.enabled) ? attachment_override.vlan:m_attach_stream.vlan;
	retVal.input_stream_id = m_sddsToBulkIO.getStreamId();
	retVal.input_samplerate = m_sddsToBulkIO.getSampleRate();
	retVal.input_endianness = m_sddsToBulkIO.getEndianness();
	retVal.time_slips = m_sddsToBulkIO.getTimeSlips();

	return retVal;
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

	if (m_sddsToBulkIOThread) {
		getPriority(m_sddsToBulkIOThread->native_handle(), advanced_optimizations.sdds_to_bulkio_thread_priority, "sdds to bulkio thread");
	}

	if (m_socketReaderThread) {
		getPriority(m_socketReaderThread->native_handle(), advanced_optimizations.socket_read_thread_priority, "socket reader thread");
	}

	// Set to the struct version right now in case NOT_SET and then check to see if threads are up and get actuals
	retVal.sdds_to_bulkio_thread_priority = advanced_optimizations.sdds_to_bulkio_thread_priority;
	retVal.socket_read_thread_priority = advanced_optimizations.socket_read_thread_priority;

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
		advanced_optimizations.pkts_per_socket_read = request.pkts_per_socket_read;
		m_socketReader.setPktsPerRead(request.pkts_per_socket_read);
	} else if(m_socketReader.getPktsPerRead() != request.pkts_per_socket_read) {
		LOG_WARN(SourceSDDS_i, "Cannot set packets per socket read size while the component is running");
	}

	if (not started()) {
		advanced_optimizations.sdds_pkts_per_bulkio_push = request.sdds_pkts_per_bulkio_push;
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

	advanced_optimizations.socket_read_thread_priority = request.socket_read_thread_priority;

	if (m_socketReaderThread) {
		setPolicyAndPriority(m_socketReaderThread->native_handle(), request.socket_read_thread_priority, "socket reader thread");
	}

	advanced_optimizations.sdds_to_bulkio_thread_priority = request.sdds_to_bulkio_thread_priority;

	if (m_sddsToBulkIOThread) {
		setPolicyAndPriority(m_sddsToBulkIOThread->native_handle(), request.sdds_to_bulkio_thread_priority, "sdds to bulkio thread");
	}
}

void SourceSDDS_i::start() throw (CORBA::SystemException, CF::Resource::StartError) {
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
	if (not m_attach_stream.attached && not attachment_override.enabled) {
		LOG_ERROR(SourceSDDS_i, "Cannot setup the socket reader without either a successful attach or attachment override set");
		return;
	}

	try {
		setupSocketReaderOptions();
	} catch (BadParameterError &e) {
		std::stringstream ss;
		ss << "Failed to setup socket reader options: " << e.what();
		LOG_ERROR(SourceSDDS_i, ss.str());
		throw CF::Resource::StartError(CF::CF_EINVAL, ss.str().c_str());
		return;
	}

	m_socketReaderThread = new boost::thread(boost::bind(&SocketReader::run, boost::ref(m_socketReader), &m_pktbuffer));

	// Attempt to set the affinity of the socket reader thread if the user has told us to.
	if (!advanced_optimizations.socket_read_thread_affinity.empty() && !(advanced_optimizations.socket_read_thread_affinity == "")) {
		setAffinity(m_socketReaderThread->native_handle(), advanced_optimizations.socket_read_thread_affinity);
	}

	advanced_optimizations.socket_read_thread_affinity = getAffinity(m_socketReaderThread->native_handle());
	setPolicyAndPriority(m_socketReaderThread->native_handle(), advanced_optimizations.socket_read_thread_priority, "socket reader thread");

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
	setPolicyAndPriority(m_sddsToBulkIOThread->native_handle(), advanced_optimizations.sdds_to_bulkio_thread_priority, "sdds to bulkio thread");

	// Call the parent start
	SourceSDDS_base::start();
}

void SourceSDDS_i::stop () throw (CF::Resource::StopError, CORBA::SystemException) {
	LOG_DEBUG(SourceSDDS_i, "Stop Called cleaning up");
	destroyBuffersAndJoinThreads();
	LOG_DEBUG(SourceSDDS_i, "Calling parent stop method");
	SourceSDDS_base::stop();
	LOG_DEBUG(SourceSDDS_i, "Finished stopping");
}

/**
 * Sets the IP and port on the class socket reader from either the SDDS port or the properties depending on
 * override settings.
 */
void SourceSDDS_i::setupSocketReaderOptions() throw (BadParameterError) {
	if (attachment_override.enabled) {
		m_socketReader.setConnectionInfo(interface, attachment_override.ip_address, attachment_override.vlan, attachment_override.port);
	} else {
		m_socketReader.setConnectionInfo(interface, m_attach_stream.multicastAddress, m_attach_stream.vlan, m_attach_stream.port);
	}
	m_socketReader.setPktsPerRead(advanced_optimizations.pkts_per_socket_read);
}

char* SourceSDDS_i::attach(const BULKIO::SDDSStreamDefinition& stream, const char* userid) throw (BULKIO::dataSDDS::AttachError, BULKIO::dataSDDS::StreamInputError) {
	LOG_INFO(SourceSDDS_i, "Attach called by: " << userid);
	if (m_attach_stream.attached) {
		LOG_ERROR(SourceSDDS_i, "Can only handle a single attach. Detach current stream: " << m_attach_stream.id);
		throw BULKIO::dataSDDS::AttachError("Can only handle a single attach. Detach current stream first");
	}

	bool restart = false;

	// This can happen if the user has set attachment override to false while the component is running.
	if (started() && !attachment_override.enabled) {
		LOG_WARN(SourceSDDS_i, "Cannot setup connection via attach when already running, will stop, attach, and restart");
		restart = true;
		stop();
	}

	m_attach_stream.attached = true;
	m_attach_stream.id = stream.id;
	m_attach_stream.multicastAddress = stream.multicastAddress;
	m_attach_stream.port = stream.port;
	m_attach_stream.vlan = stream.vlan;

	if (restart) {
		start();
	}

	if (m_attach_stream.id.empty() || m_attach_stream.id == "") {
		m_attach_stream.id = ossie::generateUUID();
	}

	return CORBA::string_dup(m_attach_stream.id.c_str());
}

void SourceSDDS_i::detach(const char* attachId) {

	if (attachId != m_attach_stream.id) {
		LOG_ERROR(SourceSDDS_i, "ATTACHMENT ID (STREAM ID) NOT FOUND FOR: " << attachId);
		throw BULKIO::dataSDDS::DetachError("Detach called on stream not currently running");
	}

	bool restart = false;
	if (started() && !attachment_override.enabled) {
		LOG_WARN(SourceSDDS_i, "Cannot remove in-use connection via detach when already running, will stop, detach, and restart");
		restart = true;
		stop();
	}

	m_sddsToBulkIO.unsetUpstreamSri();
	m_attach_stream.attached = false;

	if (restart) {
		start();
	}
}

void SourceSDDS_i::setupSddsToBulkIOOptions() {
	m_sddsToBulkIO.setPktsPerRead(advanced_optimizations.sdds_pkts_per_bulkio_push);
	advanced_optimizations.sdds_pkts_per_bulkio_push = m_sddsToBulkIO.getPktsPerRead();

	m_sddsToBulkIO.setPushOnTTV(advanced_configuration.push_on_ttv);
	m_sddsToBulkIO.setWaitForTTV(advanced_configuration.wait_on_ttv);
	if (attachment_override.enabled) {
		m_sddsToBulkIO.setEndianness(attachment_override.endianness);
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
    LOG_DEBUG(SourceSDDS_i, "Component has no service function, returning FINISHED");
    return FINISH;
}

