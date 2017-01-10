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

/**
 * Constructor sets up callbacks for setters and getters of property structs,
 * and the callbacks for attach/detach and SRI change listeners.
 */
SourceSDDS_i::SourceSDDS_i(const char *uuid, const char *label) :
    SourceSDDS_base(uuid, label),
	m_socketReaderThread(NULL),
	m_sddsToBulkIOThread(NULL),
	m_sddsToBulkIO(dataOctetOut, dataShortOut, dataFloatOut)
{
	setPropertyQueryImpl(advanced_configuration, this, &SourceSDDS_i::get_advanced_configuration_struct);
	setPropertyQueryImpl(advanced_optimizations, this, &SourceSDDS_i::get_advanced_optimizations_struct);
	setPropertyQueryImpl(status, this, &SourceSDDS_i::get_status_struct);

	setPropertyConfigureImpl(advanced_configuration, this, &SourceSDDS_i::set_advanced_configuration_struct);
	setPropertyConfigureImpl(advanced_optimizations, this, &SourceSDDS_i::set_advanced_optimization_struct);

	dataSddsIn->setNewAttachDetachCallback(this);
	dataSddsIn->setNewSriListener(this, &SourceSDDS_i::newSriListener);
	dataSddsIn->setSriChangeListener(this, &SourceSDDS_i::newSriListener);

	m_attach_stream.attached = false;
}

/**
 * Nothing to tear down, data buffer is smart pointer based and will fall out of scope if
 * not already cleaned up. REDHAWK framework should handle lifecycle and the stop call will
 * close ports and cleanup.
 */
SourceSDDS_i::~SourceSDDS_i(){}

/**
 * Required method for the SDDS setNewSriListener API. Forwards the received SRI down to the SDDS to
 * BulkIO class so that it can incorporate the SRI into the outputed BulkIO stream.
 */
void SourceSDDS_i::newSriListener(const BULKIO::StreamSRI & newSri) {
	LOG_INFO(SourceSDDS_i, "Received new upstream SRI");
	m_sddsToBulkIO.setUpstreamSri(newSri);
}

/**
 * The getter used for the status_struct. This is registered in the constructor
 * such that the REDHAWK framework will call this method rather than use the query API.
 *
 */
struct status_struct SourceSDDS_i::get_status_struct() {
	struct status_struct retVal;
	int num_listeners;

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

	retVal.input_address = (attachment_override.enabled) ? attachment_override.ip_address:m_attach_stream.multicastAddress;
	retVal.input_port = (attachment_override.enabled) ? attachment_override.port:m_attach_stream.port;
	retVal.input_vlan = (attachment_override.enabled) ? attachment_override.vlan:m_attach_stream.vlan;

	// Not 100% sure why but the queue can actually get about 280 bytes larger than the set max. I guess linux gives 110% har har har (not actually 110%)
	uint64_t rx_queue = get_rx_queue(retVal.input_address, retVal.input_port, num_listeners);
	percent = 100*(float) rx_queue / (float) m_socketReader.getSocketBufferSize();
	ss << std::fixed << rx_queue << " / " << m_socketReader.getSocketBufferSize() << " (" << percent << "%)";
	retVal.udp_socket_buffer_queue = ss.str();
	retVal.num_udp_socket_readers = num_listeners;
	ss.str("");

	retVal.input_stream_id = m_sddsToBulkIO.getStreamId();
	retVal.input_samplerate = m_sddsToBulkIO.getSampleRate();
	retVal.input_endianness = m_sddsToBulkIO.getEndianness();
	retVal.time_slips = m_sddsToBulkIO.getTimeSlips();

	retVal.num_packets_dropped_by_nic = get_rx_dropped(status.interface);

	retVal.interface = status.interface;

	return retVal;
}

/**
 * The getter used for the advanced_configuration_struct. This is registered in the constructor
 * such that the REDHAWK framework will call this method rather than use the query API.
 * Both these values are kept track of in the sdds to bulkIO class and not within the local
 * advanced configuration struct.
 */
struct advanced_configuration_struct SourceSDDS_i::get_advanced_configuration_struct() {
	struct advanced_configuration_struct retVal;
	retVal.push_on_ttv = m_sddsToBulkIO.getPushOnTTV();
	retVal.wait_on_ttv = m_sddsToBulkIO.getWaitOnTTV();
	return retVal;
}

/**
 * The getter used for the advanced_optimizations_struct. This is registered in the constructor
 * such that the REDHAWK framework will call this method rather than use the query API.
 * Most of the variables are kept track of in their respective classes so the class struct
 * may not be up to date.
 */
struct advanced_optimizations_struct SourceSDDS_i::get_advanced_optimizations_struct() {
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

	retVal.sdds_to_bulkio_thread_priority = advanced_optimizations.sdds_to_bulkio_thread_priority;
	retVal.socket_read_thread_priority = advanced_optimizations.socket_read_thread_priority;
	retVal.check_for_duplicate_sender = advanced_optimizations.check_for_duplicate_sender;

	return retVal;
}

/**
 * The setter used for the advanced_configuration_struct. This is registered in the constructor
 * such that the REDHAWK framework will call this method rather than use the configure API.
 * Neither property can be changed if the component is running. This is likely overkill and could be changed.
 */
void SourceSDDS_i::set_advanced_configuration_struct(struct advanced_configuration_struct request) {
	if (started() && m_sddsToBulkIO.getPushOnTTV() != request.push_on_ttv) {
		LOG_WARN(SourceSDDS_i, "Cannot set push on TTV while thread is running");
	} else {
		m_sddsToBulkIO.setPushOnTTV(request.push_on_ttv);
	}

	if (started() && m_sddsToBulkIO.getWaitOnTTV() != request.wait_on_ttv) {
		LOG_WARN(SourceSDDS_i, "Cannot set wait on TTV while thread is running");
	} else {
		m_sddsToBulkIO.setWaitForTTV(request.wait_on_ttv);
	}
}

/**
 * The setter used for the advanced_optimizations_struct property. This is registered in the constructor
 * such that the REDHAWK framework will call this method rather than use the configure API.
 * Most properties cannot be changed while the component is running, however some can.
 *
 * @param request The requested values for the advanced_optimizations_struct
 */
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

	if (not started()) {
		advanced_optimizations.check_for_duplicate_sender = request.check_for_duplicate_sender;
	} else if (advanced_optimizations.check_for_duplicate_sender != request.check_for_duplicate_sender) {
		LOG_WARN(SourceSDDS_i, "Cannot change the check for single sender property while running");
	}
}

/**
 * Initializes the internal buffers, which allocates memory, then starts both the Socket Reader thread and
 * the SDDS to BulkIO processor thread.  If any errors occur during startup, a StartError is thrown.
 * Thread affinity and priority is set here if the user has elected to set those properties.
 */
void SourceSDDS_i::start() throw (CORBA::SystemException, CF::Resource::StartError) {
	if (started()) {
		LOG_WARN(SourceSDDS_i, "Already started, call to start ignored.");
		return;
	}

	//////////////////////////////////////////
	// Setup the socketReader
	//////////////////////////////////////////
	if (not m_attach_stream.attached && not attachment_override.enabled) {
		LOG_INFO(SourceSDDS_i, "Cannot setup the socket reader without either a successful attach or attachment override set. "
				"Component will start but will be in a holding pattern until attach override set or attach call made.");
	} else {
		_start();
	}

	// Call the parent start
	SourceSDDS_base::start();
}

void SourceSDDS_i::_start() throw (CF::Resource::StartError) {

	std::stringstream errorText;
	// This also destroys all of our buffers
	destroyBuffersAndJoinThreads();

	// Initialize our buffer of packets
	m_pktbuffer.initialize(advanced_optimizations.buffer_size);

	try {
		setupSocketReaderOptions();
	} catch (BadParameterError &e) {
		errorText << "Failed to setup socket reader options: " << e.what();
		LOG_ERROR(SourceSDDS_i, errorText.str());
		throw CF::Resource::StartError(CF::CF_EINVAL, errorText.str().c_str());
	}

	m_socketReaderThread = new boost::thread(boost::bind(&SocketReader::run, boost::ref(m_socketReader), &m_pktbuffer, advanced_optimizations.check_for_duplicate_sender));

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

}

/**
 * Will stop the component and join the Socket Reader and SDDS to BulkIO processor threads.
 * Overridden from the Component API stop but calls the base class stop method as well.
 */
void SourceSDDS_i::stop () throw (CF::Resource::StopError, CORBA::SystemException) {
	LOG_DEBUG(SourceSDDS_i, "Stop Called cleaning up");
	destroyBuffersAndJoinThreads();
	LOG_DEBUG(SourceSDDS_i, "Calling parent stop method");
	SourceSDDS_base::stop();
	LOG_DEBUG(SourceSDDS_i, "Finished stopping");
}

/**
 * Sets the IP and port on the class socket reader from either the SDDS port or the properties depending on
 * override settings. If there is an issue with setting up the network parameters a Bad Parameter Error is thrown
 *
 * @throws BadParameterError is thrown by the underlying setConnectionInfo call in the socketReader class for a number of reasons
 */
void SourceSDDS_i::setupSocketReaderOptions() throw (BadParameterError) {
	if (attachment_override.enabled) {
		m_socketReader.setConnectionInfo(interface, attachment_override.ip_address, attachment_override.vlan, attachment_override.port);
	} else {
		m_socketReader.setConnectionInfo(interface, m_attach_stream.multicastAddress, m_attach_stream.vlan, m_attach_stream.port);
	}
	m_socketReader.setPktsPerRead(advanced_optimizations.pkts_per_socket_read);
	status.interface = m_socketReader.getInterface();
}

/**
 * Required method by the Attach Detach Callback API. Used to set the SDDS stream parameters in place of using
 * the attachment_override struct. Note that the supplied stream sample rate IS NOT USED. The sample
 * rate is retrieved directly from the SDDS header or from the provided SRI if the proper keywords
 * are used. (See the relevant documentation for details) If the component had been started and running
 * from an attachment_override value then the componet will stop, setup the new stream, and restart.
 *
 * @param stream A struct containing the stream definition including network parameters and stream ID. Note that the sample rate is NOT USED!
 * @param userid Used only to log who has made the attach called. This value is not used for anything other than logging.
 * @return The attachId which will either be the supplied Stream ID or a unique identifier if no stream ID is provided. This is used during the detach call.
 *
 */
char* SourceSDDS_i::attach(const BULKIO::SDDSStreamDefinition& stream, const char* userid) throw (BULKIO::dataSDDS::AttachError, BULKIO::dataSDDS::StreamInputError) {
	LOG_INFO(SourceSDDS_i, "Attach called by: " << userid);
	if (m_attach_stream.attached) {
		LOG_ERROR(SourceSDDS_i, "Can only handle a single attach. Detach current stream: " << m_attach_stream.id);
		throw BULKIO::dataSDDS::AttachError("Can only handle a single attach. Detach current stream first");
	}

	m_attach_stream.attached = true;
	m_attach_stream.id = stream.id;
	m_attach_stream.multicastAddress = stream.multicastAddress;
	m_attach_stream.port = stream.port;
	m_attach_stream.vlan = stream.vlan;

	if (m_attach_stream.id.empty() || m_attach_stream.id == "") {
		m_attach_stream.id = ossie::generateUUID();
	}

	if (started() && !attachment_override.enabled) {
		LOG_INFO(SourceSDDS_i, "Attempting to start SourceSDDS processing with provided attach values.");
		try {
			_start();
		} catch (CF::Resource::StartError &e) {
			std::stringstream errorText;
			errorText << "Failed to start component with provided attach values, attach has failed with the error: " << e.msg;
			LOG_ERROR(SourceSDDS_i, errorText.str());
			m_attach_stream.attached = false;
			throw BULKIO::dataSDDS::AttachError(errorText.str().c_str());
		}
	}

	return CORBA::string_dup(m_attach_stream.id.c_str());
}

/**
 * Required method by the Attach Detach Callback API. Used to remove an attached SDDS stream.
 * Throws a detach error if there is no stream which matches the attachId given.
 * If the component is running during a valid detach, the component will stop, detach, and attempt to
 * restart. The detach call will also have affect of unsetting any upstream SRI from the SDDS to
 * BulkIO class.
 *
 * @param attachId The unique attach ID which was returned during the matching attach call.
 * @throws DetachError If detach is called on a stream that is not currently attached / active.
 */
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

/**
 * Sets the packets per read, push on ttv, wait on ttv, and data endianness options
 * of the SDDS to BulkIO processor based on the values set in the advanced optimization,
 * advanced configuration, and attachment override structs.
 */
void SourceSDDS_i::setupSddsToBulkIOOptions() {
	m_sddsToBulkIO.setPktsPerRead(advanced_optimizations.sdds_pkts_per_bulkio_push);
	advanced_optimizations.sdds_pkts_per_bulkio_push = m_sddsToBulkIO.getPktsPerRead();

	m_sddsToBulkIO.setPushOnTTV(advanced_configuration.push_on_ttv);
	m_sddsToBulkIO.setWaitForTTV(advanced_configuration.wait_on_ttv);
	if (attachment_override.enabled) {
		m_sddsToBulkIO.setEndianness(attachment_override.endianness);
	}
}

/**
 * Not used.
 */
void SourceSDDS_i::constructor()
{
}

/**
 * Stops the socket reader thread and the SDDS to Bulkio worker threads.
 * After this call the socket will be closed, all memory used by the internal
 * buffer will be freed and any buffered BulkIO packets will be pushed.
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
	m_pktbuffer.shutDown();

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

	LOG_DEBUG(SourceSDDS_i, "Everything should be shutdown and joined");
}

/**
 * This component does not use the standard service function and instead simply returns FINISH.
 * The processing occurs in the socket reader and the SDDS to BulkIO processing threads which
 * are kicked off in the start method.
 *
 * @return Will always return the FINISH (-1) constant.
 */
int SourceSDDS_i::serviceFunction()
{
    LOG_DEBUG(SourceSDDS_i, "Component has no service function, returning FINISHED");
    return FINISH;
}

