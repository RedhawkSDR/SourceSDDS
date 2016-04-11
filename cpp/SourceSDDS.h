#ifndef SOURCESDDS_I_IMPL_H
#define SOURCESDDS_I_IMPL_H

#include "SourceSDDS_base.h"
#include "SmartPacketBuffer.h"
#include "SocketReader.h"
#include "SddsToBulkIOProcessor.h"
#include "socketUtils/SourceNicUtils.h"
#include <uuid/uuid.h>

namespace UUID_HELPER {
    inline std::string new_uuid() {
        uuid_t new_random_uuid;
        uuid_generate_random(new_random_uuid);
        char new_random_uuid_str[38];
        uuid_unparse(new_random_uuid, new_random_uuid_str);
        return std::string(new_random_uuid_str);
    }
}

class SourceSDDS_i : public SourceSDDS_base
{
    ENABLE_LOGGING
    public:
        SourceSDDS_i(const char *uuid, const char *label);
        ~SourceSDDS_i();

        void constructor();
        void start() throw (CORBA::SystemException, CF::Resource::StartError);
        void stop () throw (CF::Resource::StopError, CORBA::SystemException);
        int serviceFunction();
        std::string attach(BULKIO::SDDSStreamDefinition stream, std::string userid);
        void detach(std::string attachId);
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
