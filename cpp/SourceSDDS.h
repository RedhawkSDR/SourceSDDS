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
#ifndef SOURCESDDS_I_IMPL_H
#define SOURCESDDS_I_IMPL_H

#include "SourceSDDS_base.h"
#include "SmartPacketBuffer.h"
#include "SocketReader.h"
#include "SddsToBulkIOProcessor.h"
#include "socketUtils/SourceNicUtils.h"
#include <uuid/uuid.h>
#define NOT_SET 3

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
		void newSriListener(const BULKIO::StreamSRI & newSri);
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
        void _start() throw (CF::Resource::StartError);
        struct attach_stream {
            std::string id;
            std::string multicastAddress;
            uint16_t vlan;
            uint16_t port;
            bool attached;
        };

        struct attach_stream m_attach_stream;


};

#endif // SOURCESDDS_I_IMPL_H
