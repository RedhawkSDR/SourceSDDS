#
# This file is protected by Copyright. Please refer to the COPYRIGHT file
# distributed with this source distribution.
#
# This file is part of REDHAWK rh.SourceSDDS.
#
# REDHAWK rh.SourceSDDS is free software: you can redistribute it and/or modify it under
# the terms of the GNU Lesser General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option) any
# later version.
#
# REDHAWK rh.SourceSDDS is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
# details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see http://www.gnu.org/licenses/.
#
import unittest
import ossie.utils.testing
from ossie.cf import CF
import os
import socket
import struct
import sys
import time
import copy
from omniORB import any, CORBA
from bulkio.bulkioInterfaces import BULKIO, BULKIO__POA
import bulkio
from ossie.utils.bulkio import bulkio_helpers
import multicast
import unicast
import Sdds
import bulkio_helpers_sdds as bh
from ossie.utils import sb
from bulkio import timestamp
import datetime
from ossie.utils.bulkio.bulkio_helpers import compareSRI
from ossie.utils.bulkio import bulkio_data_helpers
import subprocess, signal, os


LITTLE_ENDIAN=1234
BIG_ENDIAN=4321
DEBUG_LEVEL=0

sb.release = getattr(sb,'release',sb.domainless._cleanUpLaunchedComponents)

# TODO: Add unit test for all the different start types eg. 
# - Started with no attach or override then attachment override set true
# - Started with no attach or override then attach comes in
# - Started with attachment override then attachment override occurs etc.

class UTC(datetime.tzinfo):
    """UTC"""

    def utcoffset(self, dt):
        return datetime.timedelta(0)

    def tzname(self, dt):
        return "UTC"

    def dst(self, dt):
        return datetime.timedelta(0)
    
def timedelta_total_seconds(timedelta):
    return (
        timedelta.microseconds + 0.0 +
        (timedelta.seconds + timedelta.days * 24 * 3600) * 10 ** 6) / 10 ** 6

class ComponentTests(ossie.utils.testing.ScaComponentTestCase):
    """Test for all component implementations in SelectionService"""

    def setupComponent(self, endianness=BIG_ENDIAN, pkts_per_push=1, sri=None):
        # Set properties
        self.comp.interface = 'lo'
        compDataSddsIn = self.comp.getPort('dataSddsIn')
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = pkts_per_push
        
        if sri is None:        
            kw = [CF.DataType("dataRef", ossie.properties.to_tc_value(endianness, 'long'))]
            sri = BULKIO.StreamSRI(hversion=1, xstart=0.0, xdelta=1.0, xunits=1, subsize=0, ystart=0.0, ydelta=0.0, yunits=0, mode=0, streamID='TestStreamID', blocking=False, keywords=kw)
            
        compDataSddsIn.pushSRI(sri,timestamp.now())  
        
        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        
        # Try to attach
        self.attachId = ''
        
        try:
            self.attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            print "ATTACH FAILED"
            attachId = ''
        
        self.assertTrue(self.attachId != '', "Failed to attach to SourceSDDS component")
        
    def testScaBasicBehavior(self):
        """Basic test, start, stop and query component"""
        self.setupComponent()
        
        #######################################################################
        # Verify the basic state of the component
        self.assertNotEqual(self.comp, None)
        self.assertEqual(self.comp.ref._non_existent(), False)
        self.assertEqual(self.comp.ref._is_a("IDL:CF/Resource:1.0"), True)

        #######################################################################
        # Validate that query returns all expected parameters
        # Query of '[]' should return the following set of properties
        expectedProps = []
        expectedProps.extend(self.getPropertySet(kinds = ("configure", "execparam"), modes = ("readwrite", "readonly"), includeNil = True))
        expectedProps.extend(self.getPropertySet(kinds = ("allocate",), action = "external", includeNil = True))
        props = self.comp.query([])
        props = dict((x.id, any.from_any(x.value)) for x in props)
        # Query may return more than expected, but not less
        for expectedProp in expectedProps:
            self.assertEquals(props.has_key(expectedProp.id), True)

        #######################################################################
        # Verify that all expected ports are available
        for port in self.scd.get_componentfeatures().get_ports().get_uses():
            port_obj = self.comp.getPort(str(port.get_usesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a("IDL:CF/Port:1.0"), True)

        for port in self.scd.get_componentfeatures().get_ports().get_provides():
            port_obj = self.comp.getPort(str(port.get_providesname()))
            self.assertNotEqual(port_obj, None)
            self.assertEqual(port_obj._non_existent(), False)
            self.assertEqual(port_obj._is_a(port.get_repid()), True)

        #######################################################################
        # Make sure start and stop can be called without throwing exceptions
        self.comp.start()
        time.sleep(1)
        self.comp.stop()
        time.sleep(1)
        #######################################################################
        # Simulate regular component shutdown
        self.comp.releaseObject()
        
    def testEmptyInterface(self):
        
        self.setupComponent()
        self.comp.interface = ''
        
        # Get ports
        compDataOctetOut_out = self.comp.getPort('dataOctetOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='octetIn')

        # Start components
        self.comp.start()

        # Create data
        fakeData = [x % 256 for x in range(1024)]

        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 0, 1], BPS = [0, 1, 0, 0, 0], TTV = 1, TT = 0, FREQ = 60000000)
        p = Sdds.SddsCharPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 1024)
        # Validate data is correct
        self.assertEqual([chr(i) for i in data[:256]], list(struct.pack('256B', *fakeData[:256])))
        self.assertEqual(self.comp.status.dropped_packets, 0)
        
        
    def testUnicastAttachSuccess(self):
        """Attaches to the dataSddsIn port 10 times making sure that it occurs successfully each time"""
        
        compDataSddsIn = self.comp.getPort('dataSddsIn')
 
        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        count = 0;
        valid = True
        while(valid):
            try:
                attachId = compDataSddsIn.attach(streamDef, 'test') 
            except Exception as e:
                attachId = ''
            time.sleep(1)
            self.assertNotEqual(attachId, '')
            compDataSddsIn.detach(attachId)
            count = count + 1
            if count == 10:
                valid = False

    def testEOSonDetach(self):
        """Checks that an EOS is sent on Detach"""
        self.setupComponent()
        
        # Get ports
        compDataFloatOut_out = self.comp.getPort('dataShortOut')
        
 
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        
        compDataSddsIn = self.comp.getPort('dataSddsIn')

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], TTV = 1, TT = 0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(3)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))
        
        
        #Send Detach
        compDataSddsIn.detach(self.attachId )
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
                 
        self.assertTrue(stream.eos)

    def testEOSonStop(self):
        """Checks that an EOS is sent on Stop """
        self.setupComponent()
        
        # Get ports
        compDataFloatOut_out = self.comp.getPort('dataShortOut')
        
 
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        
        compDataSddsIn = self.comp.getPort('dataSddsIn')

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], TTV = 1, TT = 0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)


        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))
        
        self.comp.stop()
        
        time.sleep(1)
        # Get data
        data,stream = self.getData()
               
        self.assertTrue(stream.eos)

    def testRestart(self):
        """Checks that an Component will run after being stopped without a new attach """
        self.setupComponent()
        
        # Get ports
        compDataFloatOut_out = self.comp.getPort('dataShortOut')
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        
        compDataSddsIn = self.comp.getPort('dataSddsIn')

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], TTV = 1, TT = 0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)


        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))
        
        self.comp.stop()
        
        time.sleep(1)
        # Get data
        data,stream = self.getData()
         
        self.assertTrue(stream.eos)
        
        #Restart Component and send more data
        self.comp.start()
        self.userver.send(p.encodedPacket)      
        
        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))  

    def testRestartAfterDetach(self):
        """Checks that an Component will run after being detached and reattached without stopping """
        self.setupComponent()
        
        # Get ports
        compDataFloatOut_out = self.comp.getPort('dataShortOut')
 
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        
        compDataSddsIn = self.comp.getPort('dataSddsIn')

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], TTV = 1, TT = 0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))
        
        
        #Send Detach
        compDataSddsIn.detach(self.attachId )
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
                
        self.assertTrue(stream.eos)
        
        #Re-Attach
        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        
        # Try to attach
        self.attachId = ''
        
        try:
            self.attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            print "ATTACH FAILED"
            attachId = ''
        
        self.assertTrue(self.attachId != '', "Failed to attach to SourceSDDS component")
        
        #Resend Data
        self.userver.send(p.encodedPacket)      
        
        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))  

  
    def testUnicastTTVBecomesTrue(self):
        
        self.setupComponent()
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='octetIn')
        self.sink.port.setMaxQueueDepth(2500)

        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x % 256 for x in range(1024)]

        # Create packets and send
        # 2112 assumes every 31st packet is a parity packet.
        for i in range(0, 2112):
            if i != 0 and i % 32 == 31:
                continue  # skip parity packet
            if i <= 500:
                h = Sdds.SddsHeader(i, DM = [0, 0, 1], BPS = [0, 1, 0, 0, 0], TTV = 0, TT = i, FREQ = 60000000)
                p = Sdds.SddsCharPacket(h.header, fakeData)
                p.encode()
                self.userver.send(p.encodedPacket)
            elif i > 500:
                h = Sdds.SddsHeader(i, DM = [0, 0, 1], BPS = [0, 1, 0, 0, 0], TTV = 1, TT = i, FREQ = 60000000)
                p = Sdds.SddsCharPacket(h.header, fakeData)
                p.encode()
                self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(3)
        data,stream = self.getData()
        
        #data = self.sink.getData()

        
        #packet = snk.getPacket()
        
        #print "Got a packet " , len(packet.dataBuffer), packet.T

        #print "Max Queue Depth " ,snk.getMaxQueueDepth()
        #print dir(snk.port)
        

        # Validate correct amount of data was received
        self.assertEqual(len(data), 2095104)
        self.assertEqual(self.comp.status.dropped_packets, 0)
        
        

    def testUnicastOctetPort(self):
        """Sends unicast data to the octet port and asserts the data sent equals the data received"""

        self.setupComponent()
        
        # Get ports
        compDataOctetOut_out = self.comp.getPort('dataOctetOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='octetIn')

        # Start components
        self.comp.start()

        # Create data
        fakeData = [x % 256 for x in range(1024)]

        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 0, 1], BPS = [0, 1, 0, 0, 0], TTV = 1, TT = 0, FREQ = 60000000)
        p = Sdds.SddsCharPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 1024)
        # Validate data is correct
        self.assertEqual([chr(i) for i in data[:256]], list(struct.pack('256B', *fakeData[:256])))
        self.assertEqual(self.comp.status.dropped_packets, 0)
        
        

    def testUnicastFloatPort(self):
        """Sends unicast data to the float port and asserts the data sent equals the data received"""

        self.setupComponent()
        
        # Get ports
        compDataFloatOut_out = self.comp.getPort('dataFloatOut')
 
        # Connect components
        self.comp.connect(self.sink, providesPortName='floatIn')
        
        #Start components.
        self.comp.start()

        # Create data
        fakeData = [float(x) for x in range(0, 256)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], BPS = [1, 1, 1, 1, 1], TTV = 1, TT = 0)
        p = Sdds.SddsFloatPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 256)
        
        # Validate data is correct
        self.assertEqual(fakeData, list(struct.unpack('256f', struct.pack('<256f', *data))))
        self.assertEqual(self.comp.status.dropped_packets, 0)

    def testXDeltaChange(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
            
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/(sr) * 1e9)
        time_ns=0
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4), CX=1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            if pktNum > 50:
                sr=1e6*4
                xdelta_ns=int(1/(sr) * 1e9)
            
        time.sleep(0.5)
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips! However %s reported" % self.comp.status.time_slips)
        
    def testUnicastShortPort(self):

        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], TTV = 1, TT = 0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)
        
        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('>512H', *data))))


    def testUnicastCxBit(self):
        
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, SF = 0, TTV = 1, TT = 7820928000000000000, CX = 1, DM = [0, 1, 0])
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(1)
        
        data,stream = self.getData()
        
        # Validate sri mode is correct
        self.assertEqual(stream.sri.mode, 1)

    def testUnicastBadFsn(self):

        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packets and send 
        for i in range(0, 2):
            h = Sdds.SddsHeader(1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(1)
        
        # Get data
        data,stream = self.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 1024)
        self.assertEqual(2*fakeData, list(struct.unpack('>1024H', struct.pack('>1024H', *data[:]))))
        self.assertEqual(self.comp.status.dropped_packets, 65535)

    def testBufferSizeAdjustment(self):
        self.setupComponent()

        buf_and_read = ((3300, 100), (2200, 1000), (500, 100), (800, 500))

        for x in buf_and_read:
            self.comp.advanced_optimizations.buffer_size = x[0]
            self.comp.advanced_optimizations.pkts_per_socket_read = x[1]
            time.sleep(2)
            self.comp.start()
            diff = str(x[0] - x[1])
            available = self.comp.status.empty_buffers_available
            self.assertTrue(diff in available, "Expected " + diff + " empty buffers available but received " + available)
            self.comp.stop()

    def testUdpBufferSize(self):

        self.setupComponent()
        
        buffer_sizes = (1000, 2000, 3000, 4000, 5000, 6000)
        prev_buffer = 0

        # We cannot know exactly what the kernel will pick after our request, this test will try and increase
        # the buffer size and simply confirm that each run creates a larger buffer size than the one before
        # however depending on system settings this test may fail.
        for udp_buffer_size in buffer_sizes:
            self.comp.advanced_optimizations.udp_socket_buffer_size = udp_buffer_size
            self.comp.start()
            buf_size = [int(s) for s in self.comp.status.udp_socket_buffer_queue.split() if s.isdigit()][1]
            self.assertTrue(buf_size > prev_buffer, "Failure could be false positive, see note in UDP Buffer Size test.")
            prev_buffer = buf_size
            self.comp.stop()
        
    def testSddsPacketsPerBulkIOPush(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        sdds_pkts_per_push = (1, 2, 4, 8, 16, 32, 64)



        for pkts_per_push in sdds_pkts_per_push:
            self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = pkts_per_push

            # Start components
            self.comp.start()

            num_sends = 0
            data = []
            

            sink = sb.StreamSink()
            self.comp.connect(sink, providesPortName='shortIn')
            
            while (not(data) and num_sends < 3*max(sdds_pkts_per_push)):
                # Create data
                fakeData = [x for x in range(0, 512)]
                h = Sdds.SddsHeader(num_sends)
                p = Sdds.SddsShortPacket(h.header, fakeData)
                p.encode()
                self.userver.send(p.encodedPacket)
                num_sends = num_sends + 1
                if num_sends != 0 and num_sends % 32 == 31:
                    num_sends = num_sends + 1
                    
                time.sleep(0.05)               
                packet = sink.port.getPacket()
                if packet.dataBuffer:
                    data = packet.dataBuffer

            # Validate correct amount of data was received
            self.assertEqual(len(data), pkts_per_push * 512)
            self.comp.stop()

    def testPushOnTTV(self):
        '''
        Push on TTV will send the packet out if the TTV flag changes
        '''
        self.setupComponent(pkts_per_push=10)
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        
        self.comp.advanced_configuration.push_on_ttv = True

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        ttv = 0
        pktNum = 0
        num_changes = 0
        
        # We'll do a ttv change every 7th packet
        while pktNum < 100:
            
            if pktNum % 7 == 0:
                ttv = (ttv + 1) % 2 # aka if 0 become 1, if 1 become 0
                num_changes += 1
            
            # Create data
            fakeData = [x for x in range(0, 512)]
            h = Sdds.SddsHeader(pktNum, TTV = ttv)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time.sleep(0.05)
        
        data,stream,tsamps = self.getData(wanttstamps=True)
        self.assertEqual(num_changes - 1, len(tsamps), "The number of bulkIO pushes (" + str(len(tsamps)) + ") does not match the expected (" + str(num_changes-1) + ").")
            
        self.comp.stop()

    def testWaitOnTTV(self):
        '''
        Wait on TTV will send nothing unless the pkt contains a TTV flag
        '''
        self.setupComponent(pkts_per_push=10)
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        self.comp.advanced_configuration.wait_on_ttv = True

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Start components
        self.comp.start()
        ttv = 0
        pktNum = 0
        num_changes = 0
        
        # We'll do a ttv change every 7th packet
        while pktNum < 100:
            
            if pktNum % 7 == 0:
                ttv = (ttv + 1) % 2 # aka if 0 become 1, if 1 become 0
                num_changes += 1
            
            # Create data
            fakeData = [x for x in range(0, 512)]
            h = Sdds.SddsHeader(pktNum, TTV = ttv)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time.sleep(0.05)
        
        data,stream,tsamps = self.getData(wanttstamps=True)
        self.assertEqual(num_changes/2, len(tsamps), "The number of bulkIO pushes (" + str(len(tsamps)) + ") does not match the expected (" + str(num_changes/2) + ").")
            
        self.comp.stop()
    
    def testShortBigEndianness(self):
        self.setupComponent(endianness=BIG_ENDIAN)
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        
        h = Sdds.SddsHeader(0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode() # Defaults to big endian encoding
        self.userver.send(p.encodedPacket)
        time.sleep(0.05)
        data,stream = self.getData()
        
        self.assertEqual(self.comp.status.input_endianness, "4321", "Status property for endianness is not 4321")
        self.assertEqual(data, fakeData, "Big Endian short did not match expected")

    def testShortLittleEndianness(self):
        self.setupComponent(endianness=LITTLE_ENDIAN)
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        # Byte Swap it here to make it little endian on send since encode does big endian (swap)
        fakeData_bs = list(struct.unpack('>512H', struct.pack('@512H', *fakeData)))
        
        # Convert fakeData to BIG ENDIAN
        h = Sdds.SddsHeader(0)
        p = Sdds.SddsShortPacket(h.header, fakeData_bs)
        p.encode() # Defaults to big endian encoding
        self.userver.send(p.encodedPacket)
        time.sleep(0.05)
        data,stream = self.getData()
        
        self.assertEqual(self.comp.status.input_endianness, "1234", "Status property for endianness is not 1234")
        self.assertEqual(data, fakeData, "Little Endian short did not match expected")
        
    
    def testTimeSlips(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/sr * 1e9)
        time_ns=0
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")
        
        # Introduce accumulator time slip
        while pktNum < 200:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns + 15 # The additional 15 ps is enough for ana cumulator time slip
            
        self.assertEqual(self.comp.status.time_slips, 1, "There should be one time slip from the accumulator")
        
        # Introduce one jump time slip
        while pktNum < 300:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum == 245:
                time_ns += 5000
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            
        self.assertEqual(self.comp.status.time_slips, 2, "There should be one time slip from the jump time slip!")
    
    def testNewYear(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
            
        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/sr * 1e9)
        ns_in_year = 1e9*31536000
        time_ns=ns_in_year - xdelta_ns*512*20
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            if time_ns > ns_in_year:
                time_ns = time_ns - ns_in_year
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")

    def testNaughtyDevice(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
            
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/(sr*2) * 1e9)
        time_ns=0
        
        recSRI = False
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4), CX=1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")
        
    def testNonConformingConformingSwitch(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        compDataSddsIn = self.comp.getPort('dataSddsIn')
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
            
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/(sr*2) * 1e9)
        time_ns=0
        
        recSRI = False
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4), CX=1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")
        
        # Stop the component, detach, then re-attach a new stream that is using the conforming sampleRate
        self.comp.stop()
        
        compDataSddsIn.detach(self.attachId)
        
        kw = [CF.DataType("dataRef", ossie.properties.to_tc_value(BIG_ENDIAN, 'long'))]
        sri = BULKIO.StreamSRI(hversion=1, xstart=0.0, xdelta=1.0, xunits=1, subsize=0, ystart=0.0, ydelta=0.0, yunits=0, mode=0, streamID='TestStreamID2', blocking=False, keywords=kw)
            
        compDataSddsIn.pushSRI(sri,timestamp.now())  
        
        streamDef = BULKIO.SDDSStreamDefinition('id2', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        self.attachId = compDataSddsIn.attach(streamDef, 'test') 

        self.comp.start()
        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/(sr) * 1e9)
        time_ns=0
        
        recSRI = False
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4), CX=1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")
        
        
    def testBulkIOTiming(self):
        self.setupComponent()
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        
        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
            
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ns=int(1/(sr) * 1e9)
        time_ns=0
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ns*4), CX=1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ns = time_ns + 512*xdelta_ns
            
        time.sleep(0.5)
        data,stream,tsamps = self.getData(wanttstamps=True)
        
        now = datetime.datetime.utcnow()
        first_of_year = datetime.datetime(now.year, 1, 1, tzinfo=UTC())
        begin_of_time = datetime.datetime(1970, 1, 1, tzinfo=UTC())
        
        seconds_since_new_year = timedelta_total_seconds(first_of_year - begin_of_time)
        
        expected_time_ns = 0
        
        # So we expect the received bulkIO stream to be the start of the year + the time delta from the sample rate.
        for bulkIO_time in tsamps:
            twsec = bulkIO_time[1].twsec
            tfsec = bulkIO_time[1].tfsec
            self.assertEqual(twsec, seconds_since_new_year, "BulkIO time stamp does not match received SDDS time stamp")
            self.assertEqual(tfsec, expected_time_ns/1.0e9)
            expected_time_ns = expected_time_ns + 512*xdelta_ns


    def testUseBulkIOSRI(self):
        
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Here we are using the BULKIO SRI with a modified xdelta and complex flag.
        # Adding other keywords to ensure they are passed through correctly
        kw=[CF.DataType("BULKIO_SRI_PRIORITY", ossie.properties.to_tc_value(1, 'long'))]
        kw.append(CF.DataType("Test_Keyword_string", ossie.properties.to_tc_value('test', 'string')))
        kw.append(CF.DataType("Test_Keyword_long", ossie.properties.to_tc_value(10, 'long')))
        kw.append(CF.DataType("COL_RF", ossie.properties.to_tc_value(100000000, 'double')))
        sri = BULKIO.StreamSRI(hversion=1, xstart=0.0, xdelta=1.234e-9, xunits=1, subsize=0, ystart=0.0, ydelta=0.0, yunits=0, mode=0, streamID='TestStreamID', blocking=False, keywords=kw)
        self.setupComponent(sri=sri)
        
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        
        h = Sdds.SddsHeader(0, CX=1)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode() # Defaults to big endian encoding
        self.userver.send(p.encodedPacket)
        time.sleep(0.05)
        data,stream = self.getData()
        sri_rx = stream.sri
        
        # compareSRI does not work for CF.DataType with keywords so we check those first then zero them out
        for index,keyword in enumerate(sri.keywords):
            self.assertEqual(keyword.id, sri_rx.keywords[index].id, "SRI Keyword ID do not match")
            self.assertEqual(keyword.value.value(), sri_rx.keywords[index].value.value(), "SRI Keyword Value do not match")
            self.assertEqual(keyword.value.typecode().kind(), sri_rx.keywords[index].value.typecode().kind(), "SRI Keyword Type codes do not match")

        sri.keywords = []
        sri_rx.keywords = []
        self.assertTrue(compareSRI(sri, sri_rx), "Attach SRI does not match received SRI")
        
    def testMergeBulkIOSRI(self):
        # Get ports
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Connect components
        self.comp.connect(self.sink, providesPortName='shortIn')
        
        # Here we are using the BULKIO SRI with a modified xdelta and complex flag but the sdds xdelta and cx should merge in
        sri = BULKIO.StreamSRI(hversion=1, xstart=0.0, xdelta=1.234e-9, xunits=1, subsize=0, ystart=0.0, ydelta=0.0, yunits=0, mode=0, streamID='StreamID1234', blocking=False, keywords=[])
        self.setupComponent(sri=sri)
            
        # Start components
        self.comp.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        
        h = Sdds.SddsHeader(0, CX=1)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode() # Defaults to big endian encoding
        self.userver.send(p.encodedPacket)
        time.sleep(0.05)
        data,stream = self.getData()
        sri_rx = stream.sri
        
        self.assertNotEqual(sri_rx.mode, sri.mode, "SDDS Packet Mode should have overridden the supplied SRI")
        self.assertNotEqual(sri_rx.xdelta, sri.xdelta, "SDDS Packet xdelta should have overridden the supplied SRI")
        self.assertEqual(sri_rx.streamID, sri.streamID, "Output SRI StreamID should have been inherited from the SRI")
        
        
    def testSpeed(self):
        print "------------ This test is informational only and should never fail ----------------------"
        
        self.setupComponent(endianness=LITTLE_ENDIAN)

        target_speed = 1000000.0
        run = True
        sleepTime = 2
        target_threshold = 0.7
        speed_bump = 10.0
        last_num_dropped = 0
        top_speed = 0
        
        while (run):
            self.comp.start()
            
            command = ["../cpp/test_utils/sddsShooter", self.uni_ip, str(self.port), str(target_speed), str(sleepTime)]
            p = subprocess.Popen(command, stdout=subprocess.PIPE)
            max_speed_acheived = float(p.stdout.readline())
            
            if (max_speed_acheived / target_speed < target_threshold):
                print "Sdds Shooter could not achieve the target speed of %s Mbps, max it could shoot was %s Mbps" % (str(8*target_speed/1024/1024), str(8*max_speed_acheived/1024/1024))
                run = False
                continue
            
            if self.comp.status.dropped_packets == last_num_dropped:
                target_speed = target_speed * speed_bump
                top_speed = max_speed_acheived
            else:
                last_num_dropped = self.comp.status.dropped_packets
                target_speed = target_speed / speed_bump # Resets us back to our previous speed. 
                speed_bump = speed_bump / 2
                print 'dropped packets'
                run = False
                
            if speed_bump < 1.1:
                run = False
            
            self.comp.stop()
            
        print "Final successful speed hit: %s Mbps" % str(8*top_speed/1024/1024)
            
        
# TODO: Socket Reader Thread affinity
# TODO: BULKIO Thread affinity
# TODO: Socket Reader priority
# TODO: BULKIO priority

        
    # TODO: Take a look at this again, it isnt actually checking anything.
    def testUnicastAttachFail(self):
        
        # Get ports
        compDataSddsIn = self.comp.getPort('dataSddsIn')
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.buffer_size = 200000
        
        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''
            
        # Start component
        self.comp.start()

        # Wait for the attach
        time.sleep(1)

        # Double attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = 'shouldFail'

        self.assertTrue(attachId == 'shouldFail', "Second attach on port should cause a failure!")
        
    def testTwoInstances(self):
        # ensure log level is set to TRACE
        self.comp.log_level(CF.LogLevels.TRACE)
        
        # launch a second instance (first instance is launched via setUp method
        comp2 = sb.launch(self.spd_file, impl=self.impl)
        comp2.log_level(CF.LogLevels.TRACE)
        comp2.buffer_size = 200000
        comp2.advanced_configuration.pushOnTTValid = False
        port2 = 29495+1
        mserver2 = multicast.multicast_server(self.multi_ip, port2)
        userver2 = unicast.unicast_server(self.uni_ip, port2)
        sink2 = sb.StreamSink()
        sink2.start()
        
        time.sleep(2)
        
        comp2.stop()
        comp2.releaseObject()
        
        

    def getData(self,wanttstamps=False):
        
        out = []
        tstamps = []
        count=0
        stream = None
            
        while True:
            streamdata = None          
            streamdata = self.sink.read(timeout=0)
            if streamdata:    
                newOut = streamdata.data
                tstamps.extend(streamdata.timestamps)
                out.extend(newOut)
                count=0
                stream = copy.copy(streamdata)
                if streamdata.eos:
                    break
            elif count==100:
                break
            time.sleep(.01)
            count+=1
        if wanttstamps:
            return out,stream,tstamps
        else:
            return out,stream

    def setUp(self):
        print "\nRunning test:", self.id()
        ossie.utils.testing.ScaComponentTestCase.setUp(self)
        
        execparams = {"DEBUG_LEVEL" : DEBUG_LEVEL}
        self.comp = sb.launch(self.spd_file, impl=self.impl,properties=execparams)

		#Configure multicast properties
        self.comp.buffer_size = 200000  # cannot alter this except in base class loadProperties (requires recompile)
        self.comp.advanced_configuration.pushOnTTValid = False
        self.port = 29495
        self.uni_ip = '127.0.0.1'
        self.multi_ip = '236.0.10.1'
        self.mserver = multicast.multicast_server(self.multi_ip, self.port)
        self.userver = unicast.unicast_server(self.uni_ip, self.port)
        
        self.sink = sb.StreamSink()
        self.sink.start()

    def tearDown(self):
        #Tear down the rest of the object.
        ossie.utils.testing.ScaComponentTestCase.tearDown(self)
        del(self.mserver)
        del(self.userver)
        
        
        # Simulate regular component shutdown
        #sb.stop()
if __name__ == "__main__":
	ossie.utils.testing.main("../SourceSDDS.spd.xml") # By default tests all implementations
