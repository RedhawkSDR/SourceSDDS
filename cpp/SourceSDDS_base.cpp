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
#include "SourceSDDS_base.h"

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

    The following class functions are for the base class for the component class. To
    customize any of these functions, do not modify them here. Instead, overload them
    on the child class

******************************************************************************************/

SourceSDDS_base::SourceSDDS_base(const char *uuid, const char *label) :
    Component(uuid, label),
    ThreadedComponent()
{
    setThreadName(label);

    loadProperties();

    dataSddsIn = new bulkio::InSDDSPort("dataSddsIn");
    addPort("dataSddsIn", "The SDDS Input Port. This port provides an attach() and pushSRI() call to provide the network information to the component about where to find the SDDS network data stream. This port only passes metadata not the data stream itself. ", dataSddsIn);
    dataOctetOut = new bulkio::OutOctetPort("dataOctetOut");
    addPort("dataOctetOut", "BULKIO output for 8 bit fixed point data. ", dataOctetOut);
    dataShortOut = new bulkio::OutShortPort("dataShortOut");
    addPort("dataShortOut", "BULKIO output for 16 bit fixed point data. ", dataShortOut);
    dataFloatOut = new bulkio::OutFloatPort("dataFloatOut");
    addPort("dataFloatOut", "BULKIO output for 32 bit floating point data. ", dataFloatOut);
}

SourceSDDS_base::~SourceSDDS_base()
{
    dataSddsIn->_remove_ref();
    dataSddsIn = 0;
    dataOctetOut->_remove_ref();
    dataOctetOut = 0;
    dataShortOut->_remove_ref();
    dataShortOut = 0;
    dataFloatOut->_remove_ref();
    dataFloatOut = 0;
}

/*******************************************************************************************
    Framework-level functions
    These functions are generally called by the framework to perform housekeeping.
*******************************************************************************************/
void SourceSDDS_base::start() throw (CORBA::SystemException, CF::Resource::StartError)
{
    Component::start();
    ThreadedComponent::startThread();
}

void SourceSDDS_base::stop() throw (CORBA::SystemException, CF::Resource::StopError)
{
    Component::stop();
    if (!ThreadedComponent::stopThread()) {
        throw CF::Resource::StopError(CF::CF_NOTSET, "Processing thread did not die");
    }
}

void SourceSDDS_base::releaseObject() throw (CORBA::SystemException, CF::LifeCycle::ReleaseError)
{
    // This function clears the component running condition so main shuts down everything
    try {
        stop();
    } catch (CF::Resource::StopError& ex) {
        // TODO - this should probably be logged instead of ignored
    }

    Component::releaseObject();
}

void SourceSDDS_base::loadProperties()
{
    addProperty(interface,
                "eth0",
                "interface",
                "interface",
                "readwrite",
                "",
                "external",
                "property");

    addProperty(advanced_optimizations,
                advanced_optimizations_struct(),
                "advanced_optimizations",
                "",
                "readwrite",
                "",
                "external",
                "property");

    addProperty(attachment_override,
                attachment_override_struct(),
                "attachment_override",
                "",
                "readwrite",
                "",
                "external",
                "property");

    addProperty(advanced_configuration,
                advanced_configuration_struct(),
                "advanced_configuration",
                "",
                "readwrite",
                "",
                "external",
                "property");

    addProperty(status,
                status_struct(),
                "status",
                "",
                "readonly",
                "",
                "external",
                "property");

}


