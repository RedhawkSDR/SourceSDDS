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
        buffer_size = 200000;
        udp_socket_buffer_size = 0;
        pkts_per_socket_read = 500;
        pkts_per_buffer_read = 500;
        socket_read_thread_affinity = "";
        work_thread_affinity = "";
    };

    static std::string getId() {
        return std::string("advanced_optimizations");
    };

    CORBA::ULong buffer_size;
    CORBA::ULong udp_socket_buffer_size;
    unsigned short pkts_per_socket_read;
    unsigned short pkts_per_buffer_read;
    std::string socket_read_thread_affinity;
    std::string work_thread_affinity;
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
    if (props.contains("advanced_optimizations::pkts_per_buffer_read")) {
        if (!(props["advanced_optimizations::pkts_per_buffer_read"] >>= s.pkts_per_buffer_read)) return false;
    }
    if (props.contains("advanced_optimizations::socket_read_thread_affinity")) {
        if (!(props["advanced_optimizations::socket_read_thread_affinity"] >>= s.socket_read_thread_affinity)) return false;
    }
    if (props.contains("advanced_optimizations::work_thread_affinity")) {
        if (!(props["advanced_optimizations::work_thread_affinity"] >>= s.work_thread_affinity)) return false;
    }
    return true;
}

inline void operator<<= (CORBA::Any& a, const advanced_optimizations_struct& s) {
    redhawk::PropertyMap props;
 
    props["advanced_optimizations::buffer_size"] = s.buffer_size;
 
    props["advanced_optimizations::udp_socket_buffer_size"] = s.udp_socket_buffer_size;
 
    props["advanced_optimizations::pkts_per_socket_read"] = s.pkts_per_socket_read;
 
    props["advanced_optimizations::pkts_per_buffer_read"] = s.pkts_per_buffer_read;
 
    props["advanced_optimizations::socket_read_thread_affinity"] = s.socket_read_thread_affinity;
 
    props["advanced_optimizations::work_thread_affinity"] = s.work_thread_affinity;
    a <<= props;
}

inline bool operator== (const advanced_optimizations_struct& s1, const advanced_optimizations_struct& s2) {
    if (s1.buffer_size!=s2.buffer_size)
        return false;
    if (s1.udp_socket_buffer_size!=s2.udp_socket_buffer_size)
        return false;
    if (s1.pkts_per_socket_read!=s2.pkts_per_socket_read)
        return false;
    if (s1.pkts_per_buffer_read!=s2.pkts_per_buffer_read)
        return false;
    if (s1.socket_read_thread_affinity!=s2.socket_read_thread_affinity)
        return false;
    if (s1.work_thread_affinity!=s2.work_thread_affinity)
        return false;
    return true;
}

inline bool operator!= (const advanced_optimizations_struct& s1, const advanced_optimizations_struct& s2) {
    return !(s1==s2);
}

struct attachment_override_struct {
    attachment_override_struct ()
    {
        enabled = false;
        ip_address = "127.0.0.1";
        vlan = 0;
        port = 29495;
    };

    static std::string getId() {
        return std::string("attachment_override");
    };

    bool enabled;
    std::string ip_address;
    unsigned short vlan;
    unsigned short port;
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
    return true;
}

inline void operator<<= (CORBA::Any& a, const attachment_override_struct& s) {
    redhawk::PropertyMap props;
 
    props["attachment_override::enabled"] = s.enabled;
 
    props["attachment_override::ip_address"] = s.ip_address;
 
    props["attachment_override::vlan"] = s.vlan;
 
    props["attachment_override:port"] = s.port;
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
    return true;
}

inline bool operator!= (const attachment_override_struct& s1, const attachment_override_struct& s2) {
    return !(s1==s2);
}

#endif // STRUCTPROPS_H
