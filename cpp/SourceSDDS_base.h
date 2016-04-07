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
        /// Port: sdds_in
        bulkio::InSDDSPort *sdds_in;
        /// Port: octet_out
        bulkio::OutOctetPort *octet_out;
        /// Port: short_out
        bulkio::OutShortPort *short_out;
        /// Port: float_out
        bulkio::OutFloatPort *float_out;

    private:
};
#endif // SOURCESDDS_BASE_IMPL_BASE_H
