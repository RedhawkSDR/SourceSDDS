# REDHAWK rh.SourceSDDS

## Table of Contents

* [Description](#description)
* [Branches and Tags](#branches-and-tags)
* [REDHAWK Version Compatibility](#redhawk-version-compatibility)
* [Installation Instructions](#installation-instructions)
* [Design](#design)
* [Asset Use](#asset-use)
* [Unimplemented Optimizations](#unimplemented-optimizations)
* [Copyrights](#copyrights)
* [License](#license)

## Description

The rh.SourceSDDS will consume a single SDDS formatted multicast or unicast UDP stream and output it via the cooresponding bulkIO port. The component provides a number of status properties including buffer montioring of both kernel space socket and internal component buffers. Source IP and port information may either be expressed via the attachment override property or via the bulkIO SDDS ports attach call. See the [properties](#properties) and [SRI](#sri) section for details on how to configure the components advanced optimizations and the list of SRI keywords checked for within the component.

## Branches and Tags

All REDHAWK core assets use the same branching and tagging policy. Upon release,
the `master` branch is rebased with the specific commit released, and that
commit is tagged with the asset version number. For example, the commit released
as version 1.0.0 is tagged with `1.0.0`.

Development branches (i.e. `develop` or `develop-X.X`, where *X.X* is an asset
version number) contain the latest unreleased development code for the specified
version. If no version is specified (i.e. `develop`), the branch contains the
latest unreleased development code for the latest released version.

## REDHAWK Version Compatibility

| Asset Version | Minimum REDHAWK Version Required |
| ------------- | -------------------------------- |
| 2.x           | 2.1                              |
| 1.x           | 2.0                              |

## Installation Instructions

To build from source, run the `build.sh` script found at the top level
directory. To install to $SDRROOT, run `build.sh install`. Note: root privileges
(`sudo`) may be required to install.

## Design

The design goals for this component were to provide a clean, easy to follow, SourceSDDS implementation that could not only ingest at the expected data rates but also provide status metrics for the data flow, multi-cast configuration debugging, and test cases to profile the max ingest speed. 

The dataflow and source code can be broken up into four distict sections; component logic, socket reader, internal buffers, and the SDDS to bulkIO processor. The component class has no service loop and instead starts two threads on start; the socket reader and the SDDS to BulkIO processor. The socket reader thread pulls a user defined number of SDDS packets off the socket at a time and places them into the shared buffer for the SDDS to BulkIO thread to consume and push
out the BulkIO ports.

## Asset Use

SourceSDDS ingests network SDDS and outputs BULKIO data as Octet, Short, or Float. The component receives information on how and where to consume the network data either from receiving and attach() and pushSRI() call in the dataSddsIn port or by using the attachment_override property. The component has a property for the network "interface" indicating which network interface on the host computer must be used in order to receive the network data. If the interface is left blank the component will try to resolve the correct interface based on the IP address and VLAN of the desired incoming data. Once the component has acquired the network stream it will remove the the SDDS network headers package the data as BULKIO and push it out the appropriate BULKIO port.    

#### Properties

Properties and their descriptions are below, struct props are shown with their struct properties in a table below:

**_advanced_optimizations_** - A set of optimizations that may help adjust throughput performance. The defaults values work well for most systems.

| Struct Property      | Description  |
| ------------- | -----|
| buffer_size | The maximum number of elements (SDDS Packets) which can be held within the internal buffer. If there is down stream back pressure this buffer will start to fill first and provide pressure on the socket buffer if full.  Current fullness is displayed within status struct |
| udp_socket_buffer_size | The socket buffer size requested via a call to setsockopt. Once the socket is opened, the user provided value will be replaced with the true value returned by the kernel. Note that the actual value set will depend on system configuration; in addition, the kernel will double the value to allow space for bookkeeping overhead. |
| pkts_per_socket_read | The maximum number of SDDS packets read per read of the socket. The recvmmsg system call is used to read multiple UDP packets per system call, and a non-blocking socket used so at most, pkts_per_socket_read will be read.|
| sdds_pkts_per_bulkio_push | The number of SDDS packets to aggregate per BulkIO pushpacket call. Note that situations such as a TTV change, or packet drops may cause push packets to occur before the desired size is achieved. Increasing this value will improve throughput performance but impact latency. It also has an affect on timing precision as only the first SDDS packet in the group's time stamp is preserved in the BulkIO call.|
| socket_read_thread_affinity | Set using the same bitmask syntax (eg. FFFFFFFF) as taskset and limits the CPU affinity of the thread which reads from the socket to only the specified CPUs. If externally set, this property will update to reflect the actual thread affinity|
| sdds_to_bulkio_thread_affinity | Set using the same bitmask syntax (eg. FFFFFFFF) as taskset and limits the CPU affinity of the thread which consumes packets from the internal buffer, and makes the call to pushpacket|
| socket_read_thread_priority | If set to non-zero, the scheduler type for the socket reader thread will be set to Round Robin and the priority set to the provided value using the pthread_setschedparam call. Note that rtprio privileges will need to be given to user running the component and that in most cases, this feature is not needed to keep up with data rates.|
| sdds_to_bulkio_thread_priority | If set to non-zero, the scheduler type for the SDDS to BulkIO processor thread will be set to Round Robin and the priority set to the provided value using the pthread_setschedparam call. Note that rtprio privileges will need to be given to user running the component and that in most cases, this feature is not needed to keep up with data rates.|
| check_for_duplicate_sender | If true, the source address of each SDDS packet will be checked and a warning printed if two different hosts are sending packets on the same multicast address. This is used primarily to debug the network configuration and can impact performance so is disabled by default.|

**_attachment_override_** - Used in place of the SDDS Port to establish a multicast or unicast connection to a specific host and port. If enabled, this will overrule calls to attach however any SRI received from the attach port will be used.


| Struct Property      | Description  |
| ------------- | -----|
| enabled | Denotes if the attachment override values should be used. |
| ip_address | For the unicast case this is the IP address of the network interface to bind to where the address of 0.0.0.0 is acceptable. For the multicast case this is the multicast group to join. |
| vlan | VLAN of the interface carrying the SDDS traffic. Ignored if set to 0. |
| port | Source port of SDDS traffic (default SDDS port is: 29495) |
| endianness | The endianness (Big or Little) of the data portion of the SDDS packet. Defaults to Network Byte Order (Big Endian) |

**_interface_** - The network interface you intend to be present or blank if no check is needed. Do not include the VLAN in the interface name. (eg. For eth0.28 the interface should be set to "eth0" NOT "eth0.28"). 

**_advanced_configuration_** - Configuration options that affect when and how to forward SDDS packets to BulkIO

| Struct Property      | Description  |
| ------------- | -----|
| push_on_ttv | If set to true, a push packet will occur on any state change of the SDDS Time Tag Valid (TTV) flag. Eg. If TTV goes from True to False, all currently buffered data will be sent with a push packet and the next packet will start with the TTV False data. The TCS_INVALID flag will be set in the BulkIO timing field if the TTV flag is false. |
| wait_on_ttv | If set to true, no BulkIO packets will be pushed unless the SDDS Time Tag Valid (TTV) flag is set to true. Any packets missed due to invalid Time Tag will be counted as dropped / missed packets. |

**_status_** - A read only status structure to monitor the components performance as well as dropped packets and timing slips.

| Struct Property      | Description  |
| ------------- | -----|
| expected_sequence_number | The next SDDS sequence number expected. Useful to confirm SDDS packets are being received.  |
| dropped_packets | The number of lost SDDS packets. For simplicity, the calculation includes the optional checksum packets in the lost SDDS packet count (sent every 31 packets) so it may not reflect the exact number of dropped packets if checksum packets are not used (and they never are). |
| bits_per_sample | The size (in bits) of the SDDS sample datatype which is derived from the bps field in the SDDS header. Values map from: (8 -> Byte), (16 -> Short), (32 -> Float) |
| empty_buffers_available | The number of empty SDDS buffers in the internal buffer that are available to the socket reader. Note empty_buffers_available + buffers_to_work may be less than the total buffer size as the socket reader pops off pkts_per_socket_read and the BulkIO thread pops sdds_pkts_per_bulkio_push.|
| buffers_to_work | The number of full SDDS buffers in the internal buffer that need to be converted to BulkIO by the SDDS to BulkIO processor. Note empty_buffers_available + buffers_to_work may be less than the total buffer size as the socket reader pops off pkts_per_socket_read and the BulkIO thread pops sdds_pkts_per_bulkio_push.|
| udp_socket_buffer_queue | The current size of the kernels UDP buffer for the specific IP and port in use by this component. The data is parsed from /proc/net/udp. Note that multiple consumers may read from the same IP and socket and will appear to have unique lines the /proc/net/udp file however; the kernel keeps a *single* buffer for all consumers so this property reflects the max value of "fullness" as the slowest process will cause all processes to miss packets. |
| num_udp_socket_readers | The number of consumers on this socket. The data is parsed from /proc/net/udp. |
| input_address | The current host IP address in use either via the attachment override or attach call. |
| input_port | The current host port in use either via the attachment override or attach call. |
| input_vlan | The current host vlan in use either via the attachment override or attach call. |
| input_endianness | The endianness set either via SRI keywords or attachment override. |
| input_samplerate | The current samplerate, derived from either the SRI or the frequency field of the SDDS packet. The sample rate supplied by the attach call is ignored!|
| input_stream_id | The stream id set via SRI. A default is used if no stream ID is passed via SRI.|
| time_slips | The number of time slips which have occurred. A time slip could be either a single time slip event or an accumulated time slip. A single time slip event is defined as the SDDS timestamps between two SDDS packets exceeding a one sample delta. (eg. there was one sample time lag or lead between consecutive packets)  An accumulated time slip is defined as the absolute value of the time error accumulator exceeding 0.000001 seconds. The time error accumulator is a running total of the delta between the expected (1/sample_rate) and actual time stamps and should always hover around zero. |
| num_packets_dropped_by_nic | Read from /sys/class/\[interface\]/statistics/rx_dropped, indicates the number of packets received by the network device that are not forwarded to the upper layers for packet processing. This is NOT an indication of full buffers but instead a hint that something may be missconfigured as the NIC is receiving packets it does not know what to do with. See the network driver for the exact meaning of this value. |
| interface | The network interface currently in use by the component for consuming data from the network. |

#### SRI

SRI can be fed into the SDDS port for the purpose of overriding the SDDS header, setting a stream ID, and passing along keywords. By default, the xdelta/sample rate is derived from the SDDS header. The sample rate supplied with the attach call is always ignored. Optionally, you may override the xdelta via keywords. Below is the list of keywords that are read by this component and its response.

* BULKIO_SRI_PRIORITY or use_BULKIO_SRI or sddsPacketAlt - Used to override the xdelta and real/complex mode found in the SDDS Packet header in place of the xdelta and mode found in the supplied SRI.
* dataRef or DATA_REF_STR - Used to set the endianness of the SDDS data portion. A string value of "43981" or "1234" will map to little endian while "52651" or "4321" will map to big endian.

## Unimplemented Optimizations

This is a short list of additional optimizations which were considered but not implemented. Generally the reason for not implementing them was a choice of code simplicity / maintainability over increased performance. The current performance seems fast enough and I was hesitant to add the additional complexity if there was no driving factor. If in the future there is a driving factor behind increasing performance further, here is where I would start.

* **Wait free, lock free queue** - Thread contention is likely the largest factor holding back speed. In C++11 and boost 1.55 there are wait free, lock free queues which could be used.  A possible C++0x compliant queue can be found [here ](http://www.drdobbs.com/parallel/writing-lock-free-code-a-corrected-queue/210604448) but is untested. Using a lock free queue would allow the socket reader thread to more quickly get back to reading the socket as packets are generally dropped when one cannot service the socket fast enough.

* **Plain pointers over smart pointers** - Smart pointers are used to simplify memory management however smart pointers are twice the size of a standard pointer and when dropped out of scope cause a write to the internal reference counter.

* **Circular buffer over deque** - deques are pretty fast as they allocate memory in chunks but they still require some memory allocation on the fly and are not contiguous which may impact cache performance. If you ditch the smart pointers, it may make sense to ditch the deque in place of a circular buffer. Some limited testing was done using a circular buffer with smart pointers but no obvious performance difference was seen.

* **Reduce number of memcopies in SDDS to BulkIO thread** - Currently, a memcopy occurs pulling the data portion of the SDDS Packet out and into the BulkIO packet. This memcopy could be avoided if the SDDS Data is copied into a contiguous portion of memory right off of the socket. This is possible with two changes. The pool of SDDS Packets data portions would need to be constructed in a contiguous block; this would require changes to the SmartPacketBuffer. To get the socket to write into two different memory blocks a second iovec would be made. Then one could directly point to to the internal buffer on the push packet as long as the push packet did not span the end of the memory block.

## Copyrights

This work is protected by Copyright. Please refer to the
[Copyright File](COPYRIGHT) for updated copyright information.

## License

REDHAWK rh.SourceSDDS is licensed under the GNU Lesser General Public License
(LGPL).
