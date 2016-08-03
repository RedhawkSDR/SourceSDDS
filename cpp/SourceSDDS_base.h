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
#ifndef SOURCESDDS_BASE_IMPL_BASE_H
#define SOURCESDDS_BASE_IMPL_BASE_H

#include <boost/thread.hpp>
#include <ossie/Component.h>
#include <ossie/ThreadedComponent.h>

#include <bulkio/bulkio.h>
#include "struct_props.h"

class SourceSDDS_base : public Component, protected ThreadedComponent
{
    public:
        SourceSDDS_base(const char *uuid, const char *label);
        ~SourceSDDS_base();

        void start() throw (CF::Resource::StartError, CORBA::SystemException);

        void stop() throw (CF::Resource::StopError, CORBA::SystemException);

        void releaseObject() throw (CF::LifeCycle::ReleaseError, CORBA::SystemException);

        void loadProperties();

    protected:
        // Member variables exposed as properties
        /// Property: interface
        std::string interface;
        /// Property: advanced_optimizations
        advanced_optimizations_struct advanced_optimizations;
        /// Property: attachment_override
        attachment_override_struct attachment_override;
        /// Property: advanced_configuration
        advanced_configuration_struct advanced_configuration;
        /// Property: status
        status_struct status;

        // Ports
        /// Port: dataSddsIn
        bulkio::InSDDSPort *dataSddsIn;
        /// Port: dataOctetOut
        bulkio::OutOctetPort *dataOctetOut;
        /// Port: dataShortOut
        bulkio::OutShortPort *dataShortOut;
        /// Port: dataFloatOut
        bulkio::OutFloatPort *dataFloatOut;

    private:
};
#endif // SOURCESDDS_BASE_IMPL_BASE_H
