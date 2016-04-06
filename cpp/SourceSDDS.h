#ifndef SOURCESDDS_I_IMPL_H
#define SOURCESDDS_I_IMPL_H

#include "SourceSDDS_base.h"
#include "SmartPacketBuffer.h"
#include "SocketReader.h"
#include "SddsToBulkIOProcessor.h"

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
    private:
        SmartPacketBuffer<SDDSpacket> m_pktbuffer;

        boost::thread *m_socketReaderThread;
        boost::thread *m_sddsToBulkIOThread;

        SocketReader m_socketReader;
        SddsToBulkIOProcessor m_sddsToBulkIO;
        void setupConnectionOptions();
        void destroyBuffersAndJoinThreads();

};

#endif // SOURCESDDS_I_IMPL_H
