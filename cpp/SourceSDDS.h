#ifndef SOURCESDDS_I_IMPL_H
#define SOURCESDDS_I_IMPL_H

#include "SourceSDDS_base.h"
#include "SmartPacketBuffer.h"
#include "SocketReader.h"

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
        SocketReader m_socketReader;
        boost::thread *m_socketReaderThread;
        boost::thread *m_sddsToBulkioThread;
        void setupConnectionOptions();
        void stopAndJoinThreads();

};

#endif // SOURCESDDS_I_IMPL_H
