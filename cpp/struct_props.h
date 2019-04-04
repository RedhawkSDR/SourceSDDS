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
#ifndef STRUCTPROPS_H
#define STRUCTPROPS_H

/*******************************************************************************************

    AUTO-GENERATED CODE. DO NOT MODIFY

*******************************************************************************************/

#include <ossie/CorbaUtils.h>
#include <CF/cf.h>
#include <ossie/PropertyMap.h>

struct advanced_optimizations_struct {
    advanced_optimizations_struct ()
    {
        buffer_size = 20000;
        udp_socket_buffer_size = 134217728;
        pkts_per_socket_read = 500;
        sdds_pkts_per_bulkio_push = 1000;
        socket_read_thread_affinity = "";
        sdds_to_bulkio_thread_affinity = "";
        socket_read_thread_priority = -1;
        sdds_to_bulkio_thread_priority = -1;
        check_for_duplicate_sender = false;
    }

    static std::string getId() {
        return std::string("advanced_optimizations");
    }

    static const char* getFormat() {
        return "IIHHssiib";
    }

    CORBA::ULong buffer_size;
    CORBA::ULong udp_socket_buffer_size;
    unsigned short pkts_per_socket_read;
    unsigned short sdds_pkts_per_bulkio_push;
    std::string socket_read_thread_affinity;
    std::string sdds_to_bulkio_thread_affinity;
    CORBA::Long socket_read_thread_priority;
    CORBA::Long sdds_to_bulkio_thread_priority;
    bool check_for_duplicate_sender;
};

inline bool operator>>= (const CORBA::Any& a, advanced_optimizations_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("advanced_optimizations::buffer_size")) {
        if (!(props["advanced_optimizations::buffer_size"] >>= s.buffer_size)) return false;
    }
    if (props.contains("advanced_optimizations::udp_socket_buffer_size")) {
        if (!(props["advanced_optimizations::udp_socket_buffer_size"] >>= s.udp_socket_buffer_size)) return false;
    }
    if (props.contains("advanced_optimizations::pkts_per_socket_read")) {
        if (!(props["advanced_optimizations::pkts_per_socket_read"] >>= s.pkts_per_socket_read)) return false;
    }
    if (props.contains("advanced_optimizations::sdds_pkts_per_bulkio_push")) {
        if (!(props["advanced_optimizations::sdds_pkts_per_bulkio_push"] >>= s.sdds_pkts_per_bulkio_push)) return false;
    }
    if (props.contains("advanced_optimizations::socket_read_thread_affinity")) {
        if (!(props["advanced_optimizations::socket_read_thread_affinity"] >>= s.socket_read_thread_affinity)) return false;
    }
    if (props.contains("advanced_optimizations::work_thread_affinity")) {
        if (!(props["advanced_optimizations::work_thread_affinity"] >>= s.sdds_to_bulkio_thread_affinity)) return false;
    }
    if (props.contains("advanced_optimizations::socket_read_thread_priority")) {
        if (!(props["advanced_optimizations::socket_read_thread_priority"] >>= s.socket_read_thread_priority)) return false;
    }
    if (props.contains("advanced_optimizations::sdds_to_bulkio_thread_priority")) {
        if (!(props["advanced_optimizations::sdds_to_bulkio_thread_priority"] >>= s.sdds_to_bulkio_thread_priority)) return false;
    }
    if (props.contains("advanced_optimizations::check_for_duplicate_sender")) {
        if (!(props["advanced_optimizations::check_for_duplicate_sender"] >>= s.check_for_duplicate_sender)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const advanced_optimizations_struct& s) {
    redhawk::PropertyMap props;
 
    props["advanced_optimizations::buffer_size"] = s.buffer_size;
 
    props["advanced_optimizations::udp_socket_buffer_size"] = s.udp_socket_buffer_size;
 
    props["advanced_optimizations::pkts_per_socket_read"] = s.pkts_per_socket_read;
 
    props["advanced_optimizations::sdds_pkts_per_bulkio_push"] = s.sdds_pkts_per_bulkio_push;
 
    props["advanced_optimizations::socket_read_thread_affinity"] = s.socket_read_thread_affinity;
 
    props["advanced_optimizations::work_thread_affinity"] = s.sdds_to_bulkio_thread_affinity;
 
    props["advanced_optimizations::socket_read_thread_priority"] = s.socket_read_thread_priority;
 
    props["advanced_optimizations::sdds_to_bulkio_thread_priority"] = s.sdds_to_bulkio_thread_priority;
 
    props["advanced_optimizations::check_for_duplicate_sender"] = s.check_for_duplicate_sender;
    a <<= props;
}

inline bool operator== (const advanced_optimizations_struct& s1, const advanced_optimizations_struct& s2) {
    if (s1.buffer_size!=s2.buffer_size)
        return false;
    if (s1.udp_socket_buffer_size!=s2.udp_socket_buffer_size)
        return false;
    if (s1.pkts_per_socket_read!=s2.pkts_per_socket_read)
        return false;
    if (s1.sdds_pkts_per_bulkio_push!=s2.sdds_pkts_per_bulkio_push)
        return false;
    if (s1.socket_read_thread_affinity!=s2.socket_read_thread_affinity)
        return false;
    if (s1.sdds_to_bulkio_thread_affinity!=s2.sdds_to_bulkio_thread_affinity)
        return false;
    if (s1.socket_read_thread_priority!=s2.socket_read_thread_priority)
        return false;
    if (s1.sdds_to_bulkio_thread_priority!=s2.sdds_to_bulkio_thread_priority)
        return false;
    if (s1.check_for_duplicate_sender!=s2.check_for_duplicate_sender)
        return false;
    return true;
}

inline bool operator!= (const advanced_optimizations_struct& s1, const advanced_optimizations_struct& s2) {
    return !(s1==s2);
}

namespace enums {
    // Enumerated values for attachment_override
    namespace attachment_override {
        // Enumerated values for attachment_override:endianness
        namespace endianness {
            static const std::string Little_Endian = "1234";
            static const std::string Big_Endian = "4321";
        }
    }
}

struct attachment_override_struct {
    attachment_override_struct ()
    {
        enabled = false;
        ip_address = "127.0.0.1";
        vlan = 0;
        port = 29495;
        endianness = "4321";
    }

    static std::string getId() {
        return std::string("attachment_override");
    }

    static const char* getFormat() {
        return "bsHHs";
    }

    bool enabled;
    std::string ip_address;
    unsigned short vlan;
    unsigned short port;
    std::string endianness;
};

inline bool operator>>= (const CORBA::Any& a, attachment_override_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("attachment_override::enabled")) {
        if (!(props["attachment_override::enabled"] >>= s.enabled)) return false;
    }
    if (props.contains("attachment_override::ip_address")) {
        if (!(props["attachment_override::ip_address"] >>= s.ip_address)) return false;
    }
    if (props.contains("attachment_override::vlan")) {
        if (!(props["attachment_override::vlan"] >>= s.vlan)) return false;
    }
    if (props.contains("attachment_override:port")) {
        if (!(props["attachment_override:port"] >>= s.port)) return false;
    }
    if (props.contains("attachment_override:endianness")) {
        if (!(props["attachment_override:endianness"] >>= s.endianness)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const attachment_override_struct& s) {
    redhawk::PropertyMap props;
 
    props["attachment_override::enabled"] = s.enabled;
 
    props["attachment_override::ip_address"] = s.ip_address;
 
    props["attachment_override::vlan"] = s.vlan;
 
    props["attachment_override:port"] = s.port;
 
    props["attachment_override:endianness"] = s.endianness;
    a <<= props;
}

inline bool operator== (const attachment_override_struct& s1, const attachment_override_struct& s2) {
    if (s1.enabled!=s2.enabled)
        return false;
    if (s1.ip_address!=s2.ip_address)
        return false;
    if (s1.vlan!=s2.vlan)
        return false;
    if (s1.port!=s2.port)
        return false;
    if (s1.endianness!=s2.endianness)
        return false;
    return true;
}

inline bool operator!= (const attachment_override_struct& s1, const attachment_override_struct& s2) {
    return !(s1==s2);
}

struct advanced_configuration_struct {
    advanced_configuration_struct ()
    {
        push_on_ttv = false;
        wait_on_ttv = false;
    }

    static std::string getId() {
        return std::string("advanced_configuration");
    }

    static const char* getFormat() {
        return "bb";
    }

    bool push_on_ttv;
    bool wait_on_ttv;
};

inline bool operator>>= (const CORBA::Any& a, advanced_configuration_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("advanced_configuration::push_on_ttv")) {
        if (!(props["advanced_configuration::push_on_ttv"] >>= s.push_on_ttv)) return false;
    }
    if (props.contains("advanced_configuration::wait_on_ttv")) {
        if (!(props["advanced_configuration::wait_on_ttv"] >>= s.wait_on_ttv)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const advanced_configuration_struct& s) {
    redhawk::PropertyMap props;
 
    props["advanced_configuration::push_on_ttv"] = s.push_on_ttv;
 
    props["advanced_configuration::wait_on_ttv"] = s.wait_on_ttv;
    a <<= props;
}

inline bool operator== (const advanced_configuration_struct& s1, const advanced_configuration_struct& s2) {
    if (s1.push_on_ttv!=s2.push_on_ttv)
        return false;
    if (s1.wait_on_ttv!=s2.wait_on_ttv)
        return false;
    return true;
}

inline bool operator!= (const advanced_configuration_struct& s1, const advanced_configuration_struct& s2) {
    return !(s1==s2);
}

namespace enums {
    // Enumerated values for status
    namespace status {
        // Enumerated values for status::input_endianness
        namespace input_endianness {
            static const std::string Little_Endian = "1234";
            static const std::string Big_Endian = "4321";
        }
    }
}

struct status_struct {
    status_struct ()
    {
        expected_sequence_number = 0;
        dropped_packets = 0;
        bits_per_sample = 0;
        empty_buffers_available = "";
        buffers_to_work = "";
        udp_socket_buffer_queue = "";
        num_udp_socket_readers = 0;
        input_address = "";
        input_port = 0;
        input_vlan = 0;
        input_endianness = "4321";
        input_samplerate = 0;
        input_stream_id = "";
        time_slips = 0LL;
        num_packets_dropped_by_nic = 0;
        interface = "";
    }

    static std::string getId() {
        return std::string("status");
    }

    static const char* getFormat() {
        return "HIHsssisiisdslis";
    }

    unsigned short expected_sequence_number;
    CORBA::ULong dropped_packets;
    unsigned short bits_per_sample;
    std::string empty_buffers_available;
    std::string buffers_to_work;
    std::string udp_socket_buffer_queue;
    CORBA::Long num_udp_socket_readers;
    std::string input_address;
    CORBA::Long input_port;
    CORBA::Long input_vlan;
    std::string input_endianness;
    double input_samplerate;
    std::string input_stream_id;
    CORBA::LongLong time_slips;
    CORBA::Long num_packets_dropped_by_nic;
    std::string interface;
};

inline bool operator>>= (const CORBA::Any& a, status_struct& s) {
    CF::Properties* temp;
    if (!(a >>= temp)) return false;
    const redhawk::PropertyMap& props = redhawk::PropertyMap::cast(*temp);
    if (props.contains("status::expected_sequence_number")) {
        if (!(props["status::expected_sequence_number"] >>= s.expected_sequence_number)) return false;
    }
    if (props.contains("status::dropped_packets")) {
        if (!(props["status::dropped_packets"] >>= s.dropped_packets)) return false;
    }
    if (props.contains("status::bits_per_sample")) {
        if (!(props["status::bits_per_sample"] >>= s.bits_per_sample)) return false;
    }
    if (props.contains("status::empty_buffers_available")) {
        if (!(props["status::empty_buffers_available"] >>= s.empty_buffers_available)) return false;
    }
    if (props.contains("status::buffers_to_work")) {
        if (!(props["status::buffers_to_work"] >>= s.buffers_to_work)) return false;
    }
    if (props.contains("status::udp_socket_buffer_queue")) {
        if (!(props["status::udp_socket_buffer_queue"] >>= s.udp_socket_buffer_queue)) return false;
    }
    if (props.contains("status::num_udp_socket_readers")) {
        if (!(props["status::num_udp_socket_readers"] >>= s.num_udp_socket_readers)) return false;
    }
    if (props.contains("status::input_address")) {
        if (!(props["status::input_address"] >>= s.input_address)) return false;
    }
    if (props.contains("status::input_port")) {
        if (!(props["status::input_port"] >>= s.input_port)) return false;
    }
    if (props.contains("status::input_vlan")) {
        if (!(props["status::input_vlan"] >>= s.input_vlan)) return false;
    }
    if (props.contains("status::input_endianness")) {
        if (!(props["status::input_endianness"] >>= s.input_endianness)) return false;
    }
    if (props.contains("status::input_samplerate")) {
        if (!(props["status::input_samplerate"] >>= s.input_samplerate)) return false;
    }
    if (props.contains("status::input_stream_id")) {
        if (!(props["status::input_stream_id"] >>= s.input_stream_id)) return false;
    }
    if (props.contains("status::time_slips")) {
        if (!(props["status::time_slips"] >>= s.time_slips)) return false;
    }
    if (props.contains("status::num_packets_dropped_by_nic")) {
        if (!(props["status::num_packets_dropped_by_nic"] >>= s.num_packets_dropped_by_nic)) return false;
    }
    if (props.contains("status::interface")) {
        if (!(props["status::interface"] >>= s.interface)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const status_struct& s) {
    redhawk::PropertyMap props;
 
    props["status::expected_sequence_number"] = s.expected_sequence_number;
 
    props["status::dropped_packets"] = s.dropped_packets;
 
    props["status::bits_per_sample"] = s.bits_per_sample;
 
    props["status::empty_buffers_available"] = s.empty_buffers_available;
 
    props["status::buffers_to_work"] = s.buffers_to_work;
 
    props["status::udp_socket_buffer_queue"] = s.udp_socket_buffer_queue;
 
    props["status::num_udp_socket_readers"] = s.num_udp_socket_readers;
 
    props["status::input_address"] = s.input_address;
 
    props["status::input_port"] = s.input_port;
 
    props["status::input_vlan"] = s.input_vlan;
 
    props["status::input_endianness"] = s.input_endianness;
 
    props["status::input_samplerate"] = s.input_samplerate;
 
    props["status::input_stream_id"] = s.input_stream_id;
 
    props["status::time_slips"] = s.time_slips;
 
    props["status::num_packets_dropped_by_nic"] = s.num_packets_dropped_by_nic;
 
    props["status::interface"] = s.interface;
    a <<= props;
}

inline bool operator== (const status_struct& s1, const status_struct& s2) {
    if (s1.expected_sequence_number!=s2.expected_sequence_number)
        return false;
    if (s1.dropped_packets!=s2.dropped_packets)
        return false;
    if (s1.bits_per_sample!=s2.bits_per_sample)
        return false;
    if (s1.empty_buffers_available!=s2.empty_buffers_available)
        return false;
    if (s1.buffers_to_work!=s2.buffers_to_work)
        return false;
    if (s1.udp_socket_buffer_queue!=s2.udp_socket_buffer_queue)
        return false;
    if (s1.num_udp_socket_readers!=s2.num_udp_socket_readers)
        return false;
    if (s1.input_address!=s2.input_address)
        return false;
    if (s1.input_port!=s2.input_port)
        return false;
    if (s1.input_vlan!=s2.input_vlan)
        return false;
    if (s1.input_endianness!=s2.input_endianness)
        return false;
    if (s1.input_samplerate!=s2.input_samplerate)
        return false;
    if (s1.input_stream_id!=s2.input_stream_id)
        return false;
    if (s1.time_slips!=s2.time_slips)
        return false;
    if (s1.num_packets_dropped_by_nic!=s2.num_packets_dropped_by_nic)
        return false;
    if (s1.interface!=s2.interface)
        return false;
    return true;
}

inline bool operator!= (const status_struct& s1, const status_struct& s2) {
    return !(s1==s2);
}

#endif // STRUCTPROPS_H
