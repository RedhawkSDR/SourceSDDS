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

        // Ports
        /// Port: sdds_in
        bulkio::InSDDSPort *sdds_in;

    private:
};
#endif // SOURCESDDS_BASE_IMPL_BASE_H
