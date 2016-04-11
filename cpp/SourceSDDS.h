#ifndef SOURCESDDS_I_IMPL_H
#define SOURCESDDS_I_IMPL_H

#include "SourceSDDS_base.h"
#include "SmartPacketBuffer.h"
#include "SocketReader.h"
#include "SddsToBulkIOProcessor.h"
#include "socketUtils/SourceNicUtils.h"
#include <uuid/uuid.h>

class SourceSDDS_i : public SourceSDDS_base, public bulkio::InSDDSPort::Callback
{
    ENABLE_LOGGING
    public:
        SourceSDDS_i(const char *uuid, const char *label);
        ~SourceSDDS_i();

        void constructor();
        void start() throw (CORBA::SystemException, CF::Resource::StartError);
        void stop () throw (CF::Resource::StopError, CORBA::SystemException);
        int serviceFunction();
        char* attach(const BULKIO::SDDSStreamDefinition& stream, const char* userid) throw (BULKIO::dataSDDS::AttachError, BULKIO::dataSDDS::StreamInputError);
		void detach(const char* attachId);
    private:
        SmartPacketBuffer<SDDSpacket> m_pktbuffer;

        boost::thread *m_socketReaderThread;
        boost::thread *m_sddsToBulkIOThread;

        SocketReader m_socketReader;
        SddsToBulkIOProcessor m_sddsToBulkIO;
        void setupSocketReaderOptions() throw (BadParameterError);
        void setupSddsToBulkIOOptions();
        void destroyBuffersAndJoinThreads();
        struct advanced_configuration_struct get_advanced_configuration_struct();
        struct advanced_optimizations_struct get_advanced_optimizations_struct();
        struct status_struct get_status_struct();
        void set_advanced_configuration_struct(struct advanced_configuration_struct request);
        void set_advanced_optimization_struct(struct advanced_optimizations_struct request);

        struct attach_stream {
            std::string id;
            BULKIO::SDDSDataDigraph dataFormat;
            std::string multicastAddress;
            uint16_t vlan;
            uint16_t port;
            long sampleRate;
            bool timeTagValid;
            bool attached;
        };

        struct attach_stream m_attach_stream;


};

#endif // SOURCESDDS_I_IMPL_H
