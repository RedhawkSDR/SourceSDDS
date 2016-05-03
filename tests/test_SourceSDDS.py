import unittest
import ossie.utils.testing
from ossie.cf import CF
import os
import socket
import struct
import sys
import time
from omniORB import any, CORBA
from bulkio.bulkioInterfaces import BULKIO, BULKIO__POA
from ossie.utils.bulkio import bulkio_helpers
import multicast
import unicast
import Sdds
import bulkio_helpers_sdds as bh
from ossie.utils import sb
from bulkio import timestamp

class SDDSSink(BULKIO__POA.dataSDDS):
    def attach(self, stream, userid):
        self.port_lock.acquire()
        try:
            try:
                for connId, port in self.outPorts.items():
                    if port != None: r = port.attach(stream, userid)
                    return r
            except Exception, e:
                msg = "The call to attach failed with %s " % e
                msg += "connection %s instance %s" % (connId, port)
                print(msg)
        finally:
            self.port_lock.release()

    def detach(self, attachId):
        self.port_lock.acquire()
        try:
            try:
                for connId, port in self.outPorts.items():
                    if port != None: port.detach(attachId)
            except Exception, e:
                msg = "The call to detach failed with %s " % e
                msg += "connection %s instance %s" % (connId, port)
                print(msg)
        finally:
            self.port_lock.release()

class ComponentTests(ossie.utils.testing.ScaComponentTestCase):
    """Test for all component implementations in SelectionService"""

    # Test #1
    def testScaBasicBehavior(self):
        """Basic test, start, stop and query component"""
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'
        self.comp.advanced_optimizations.buffer_size = 200000
        #######################################################################
        # Verify the basic state of the component
        execparams = self.getPropertySet(kinds=("execparam",), modes=("readwrite", "writeonly"), includeNil=False)
        execparams = dict([(x.id, any.from_any(x.value)) for x in execparams])
        self.launch(execparams)
        self.assertNotEqual(self.comp, None)
        self.assertEqual(self.comp_obj._non_existent(), False)
        self.assertEqual(self.comp_obj._is_a("IDL:CF/Resource:1.0"), True)

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
        self.comp.stop()

        #######################################################################
        # Simulate regular component shutdown
        self.comp.releaseObject()

    # Test #2
    def testUnicastAttachSuccess(self):
        """Attaches to the sdds_in port 10 times making sure that it occurs successfully each time"""
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'
           
        self.comp.advanced_optimizations.buffer_size = 200000
        
        compDataSddsIn = self.comp.getPort('sdds_in')
 
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
                
    # Test #4 TTV become true after 500 packets
    def testUnicastTTVBecomesTrue(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'
        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        
        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1
        
        sink = sb.DataSink()
        
        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='octetIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            print "ATTACH FAILED"
            attachId = ''
        
        # Wait for the attach
        time.sleep(1)
        
        # Start components
        self.comp.start()
        sink.start()
        
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
        data = sink.getData()

        # Validate correct amount of data was received
        self.assertEqual(len(data), 2095104)
        self.assertEqual(self.comp.status.dropped_packets, 0)
        
        sink.stop()
        

    # Test #6
    def testUnicastOctetPort(self):
        """Sends unicast data to the octet port and asserts the data sent equals the data received"""
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataOctetOut_out = self.comp.getPort('octet_out')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1
        self.comp.advanced_optimizations.buffer_size = 200000
        
        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('charTest', BULKIO.SDDS_SB, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='octetIn')
        
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        # Start components
        self.comp.start()
        sink.start()

        # Wait for the attach
        time.sleep(1)

        # Create data
        fakeData = [x % 256 for x in range(1024)]

        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 0, 1], BPS = [0, 1, 0, 0, 0], TTV = 1, TT = 0, FREQ = 60000000)
        p = Sdds.SddsCharPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(3)
        
        # Get data
        data = sink.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 1024)
        # Validate data is correct
        self.assertEqual(data[:256], list(struct.pack('256B', *fakeData[:256])))
        self.assertEqual(self.comp.status.dropped_packets, 0)
        
        sink.stop()
        

    # Test #8
    def testUnicastFloatPort(self):
        """Sends unicast data to the float port and asserts the data sent equals the data received"""
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataFloatOut_out = self.comp.getPort('float_out')
        
        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1
        self.comp.advanced_optimizations.buffer_size = 200000
        
        sink = sb.DataSink()
 
        streamDef = BULKIO.SDDSStreamDefinition('floatTest', BULKIO.SDDS_SF, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='floatIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''
        
        #Start components.
        self.comp.start()
        sink.start()

        # Wait for the attach
        time.sleep(1)

        # Create data
        fakeData = [float(x) for x in range(0, 256)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, DM = [0, 1, 0], BPS = [1, 1, 1, 1, 1], TTV = 1, TT = 0)
        p = Sdds.SddsFloatPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(3)
        
        # Get data
        data = sink.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 256)
        
        # Validate data is correct
        self.assertEqual(fakeData, list(struct.unpack('>256f', struct.pack('256f', *data))))
        self.assertEqual(self.comp.status.dropped_packets, 0)
        
        sink.stop()
        

    # Test #10
    def testUnicastShortPort(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')
        
        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1
        self.comp.advanced_optimizations.buffer_size = 200000
        
        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('shortTest', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''
        
        # Start components
        self.comp.start()
        sink.start()

        #Wait for the attach
        time.sleep(1)

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
        data = sink.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 512)

        # Validate data is correct        
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('512h', *data))))
        
        sink.stop()
        

    # Test #12
    def testUnicastCxBit(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1
        self.comp.advanced_optimizations.buffer_size = 200000
        
        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('shortTest', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')

        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        # Wait for the attach
        time.sleep(1)
        
        # Start components
        self.comp.start()
        sink.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packet and send
        h = Sdds.SddsHeader(0, SF = 0, TTV = 1, TT = 7820928000000000000, CX = 1, DM = [0, 1, 0])
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode()
        self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(3)
        
        # Validate sri mode is correct
        self.assertEqual(sink.sri().mode, 1)
        
        sink.stop()

    # Test #14
    def testUnicastBadFsn(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '\tNote: Warning of dropped packets is expected.'
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1
        self.comp.advanced_optimizations.buffer_size = 200000

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        # Start components
        self.comp.start()
        sink.start()

        # Wait for the attach
        time.sleep(1)

        # Create data
        fakeData = [x for x in range(0, 512)]
        
        # Create packets and send 
        for i in range(0, 2):
            h = Sdds.SddsHeader(1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)

        # Wait for data to be received
        time.sleep(3)
        
        # Get data
        data = sink.getData()
        
        # Validate correct amount of data was received
        self.assertEqual(len(data), 1024)

        #         self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('512h', *data[:512]))))
        self.assertEqual(2*fakeData, list(struct.unpack('>1024H', struct.pack('1024h', *data[:]))))
        self.assertEqual(self.comp.status.dropped_packets, 65535)
        sink.stop()


# Tests needed for 
    def testBufferSizeAdjustment(self):
        
        self.comp.interface = 'lo'
        self.comp.attachment_override.enabled = True

        buf_and_read = ((3300, 100), (2200, 1000), (500, 100), (800, 500))

        for x in buf_and_read:
            self.comp.advanced_optimizations.buffer_size = x[0]
            self.comp.advanced_optimizations.pkts_per_socket_read = x[1]
            time.sleep(0.3)
            self.comp.start()
            self.assertTrue(str(x[0] - x[1]) in self.comp.status.empty_buffers_available, "Expected " + str(x[0] - x[1]) + " empty buffers available but received " + self.comp.status.empty_buffers_available)
            self.comp.stop()

    def testUdpBufferSize(self):
        self.comp.interface = 'lo'
        self.comp.attachment_override.enabled = True
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
        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')

        # Set properties
        self.comp.interface = 'lo'
        
        sdds_pkts_per_push = (1, 2, 4, 8, 16, 32, 64)
        self.comp.advanced_optimizations.buffer_size = 200000

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        for pkts_per_push in sdds_pkts_per_push:
            self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = pkts_per_push
            # Start components
            self.comp.start()
            sink.start()
            data = sink.getData()
            num_sends = 0
            
            while (len(data) == 0 and num_sends < 2*max(sdds_pkts_per_push)):
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
                # Get data
                data = sink.getData()
            
            # Validate correct amount of data was received
            self.assertEqual(len(data), pkts_per_push * 512)
            self.comp.stop()
            sink.stop()
         
         
    def testPushOnTTV(self):
        '''
        Push on TTV will send the packet out if the TTV flag changes
        '''
        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 10
        self.comp.advanced_configuration.push_on_ttv = True

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        # Start components
        self.comp.start()
        sink.start()
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
        
        data = sink.getData(tstamps=True)
        self.assertEqual(num_changes - 1, len(data[1]), "The number of bulkIO pushes (" + str(len(data[1])) + ") does not match the expected (" + str(num_changes-1) + ").")
            
        self.comp.stop()
        sink.stop()

    def testWaitOnTTV(self):
        '''
        Wait on TTV will send nothing unless the pkt contains a TTV flag
        '''
        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 10
        self.comp.advanced_configuration.wait_on_ttv = True

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        # Start components
        self.comp.start()
        sink.start()
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
        
        data = sink.getData(tstamps=True)
        self.assertEqual(num_changes/2, len(data[1]), "The number of bulkIO pushes (" + str(len(data[1])) + ") does not match the expected (" + str(num_changes/2) + ").")
            
        self.comp.stop()
        sink.stop()        
    
    def testShortBigEndianess(self):
        # Get ports
        compDataShortOut_out = self.comp.getPort('short_out')
        compDataSddsIn = self.comp.getPort('sdds_in')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''


        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')

        kw = [CF.DataType("dataRef", ossie.properties.to_tc_value(52651, 'long'))]
        sri = BULKIO.StreamSRI(hversion=1, xstart=0.0, xdelta=1.0, xunits=1, subsize=0, ystart=0.0, ydelta=0.0, yunits=0, mode=0, streamID='defStream', blocking=False, keywords=kw)
        compDataSddsIn.pushSRI(sri,timestamp.now())  
           
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test')
        except:
            attachId = ''
            
        # Start components
        self.comp.start()
        sink.start()
        
        # Create data
        fakeData = [x for x in range(0, 512)]
        
        h = Sdds.SddsHeader(0)
        p = Sdds.SddsShortPacket(h.header, fakeData)
        p.encode() # Defaults to big endian encoding
        self.userver.send(p.encodedPacket)
        time.sleep(0.05)
        data = sink.getData()
        
        self.assertEqual(self.comp.status.input_endianness, "4321", "Status property for endianness is not 4321")
        self.assertEqual(data, fakeData, "Big Endian short did not match expected")

    def testShortLittleEndianess(self):
        # Get ports
        compDataShortOut_out = self.comp.getPort('short_out')
        compDataSddsIn = self.comp.getPort('sdds_in')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')

        kw = []
        sri = BULKIO.StreamSRI(hversion=1, xstart=0.0, xdelta=1.0, xunits=1, subsize=0, ystart=0.0, ydelta=0.0, yunits=0, mode=0, streamID='defStream', blocking=False, keywords=kw)
        compDataSddsIn.pushSRI(sri,timestamp.now())  
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test')
        except:
            attachId = ''
            
        # Start components
        self.comp.start()
        sink.start()
        
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
        data = sink.getData()
        
        self.assertEqual(self.comp.status.input_endianness, "1234", "Status property for endianness is not 1234")
        self.assertEqual(data, fakeData, "Little Endian short did not match expected")
        
    
    def testTimeSlips(self):
        # Get ports
        compDataShortOut_out = self.comp.getPort('short_out')
        compDataSddsIn = self.comp.getPort('sdds_in')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test')
        except:
            attachId = ''
            
        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ms=int(1/sr * 1e9)
        time_ms=0
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ms*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ms = time_ms + 512*xdelta_ms
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")
        
        # Introduce accumulator time slip
        while pktNum < 200:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ms*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ms = time_ms + 512*xdelta_ms + 15 # The additional 15 ps is enough for ana cumulator time slip
            
        self.assertEqual(self.comp.status.time_slips, 1, "There should be one time slip from the accumulator")
        
        # Introduce one jump time slip
        while pktNum < 300:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ms*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum == 245:
                time_ms += 5000
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ms = time_ms + 512*xdelta_ms
            
        self.assertEqual(self.comp.status.time_slips, 2, "There should be one time slip from the jump time slip!")
    
    def testNewYear(self):
        # Get ports
        compDataShortOut_out = self.comp.getPort('short_out')
        compDataSddsIn = self.comp.getPort('sdds_in')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test')
        except:
            attachId = ''
            
        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ms=int(1/sr * 1e9)
        ms_in_year = 1e9*31536000
        time_ms=ms_in_year - xdelta_ms*512*20
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ms*4))
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ms = time_ms + 512*xdelta_ms
            if time_ms > ms_in_year:
                time_ms = time_ms - ms_in_year
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")

    def testNaughtyDevice(self):
        # Get ports
        compDataShortOut_out = self.comp.getPort('short_out')
        compDataSddsIn = self.comp.getPort('sdds_in')

        # Set properties
        self.comp.interface = 'lo'
        
        self.comp.advanced_optimizations.buffer_size = 200000
        self.comp.advanced_optimizations.sdds_pkts_per_bulkio_push = 1

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test')
        except:
            attachId = ''
            
        # Start components
        self.comp.start()

        # Create data
        fakeData = [x for x in range(0, 512)]
        pktNum = 0
        
        sr=1e6
        xdelta_ms=int(1/(sr*2) * 1e9)
        time_ms=0
        
        # No time slips here
        while pktNum < 100:
            # Create data
            h = Sdds.SddsHeader(pktNum, FREQ=(sr*73786976294.838211), TT=(time_ms*4), CX=1)
            p = Sdds.SddsShortPacket(h.header, fakeData)
            p.encode()
            self.userver.send(p.encodedPacket)
            pktNum = pktNum + 1
            
            if pktNum != 0 and pktNum % 32 == 31:
                pktNum = pktNum + 1
                
            time_ms = time_ms + 512*xdelta_ms
            
        self.assertEqual(self.comp.status.time_slips, 0, "There should be no time slips!")    
# TODO: Special MSDD Case
# TODO: Timing between SDDS and BulkIO
# TODO: Merge upstream SRI
# TODO: Socket Reader Thread affinity
# TODO: BULKIO Thread affinity
# TODO: Socket Reader priority
# TODO: BULKIO priority
# TODO: Speed test!
# TODO: Upstream SRI

        
    # Test #16
    # XXX: YLB Take a look at this again, it isnt actually checking any thing.
    def testUnicastAttachFail(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '\tNote: BULKIO.dataSDDS.AttachError is expected.'
        print '###################################################################'
        
        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('short_out')

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
        # Can't catch with assertError. The program exits with an error message
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

        # Wait for the attach
        time.sleep(1)


    def setUp(self):
        ossie.utils.testing.ScaComponentTestCase.setUp(self)

        #Launch the component with the default execparams.
        execparams = self.getPropertySet(kinds = ("execparam",), modes = ("readwrite", "writeonly"), includeNil = False)
        execparams = dict([(x.id, any.from_any(x.value)) for x in execparams])
        self.launch(execparams)

        #Simulate regular component startup.
        #Verify that initialize nor configure throw errors.
        self.comp.initialize()
        configureProps = self.getPropertySet(kinds = ("configure",), modes = ("readwrite", "writeonly"), includeNil = False)
        self.comp.configure(configureProps)

		#Configure multicast properties
        self.comp.buffer_size = 200000  # cannot alter this except in base class loadProperties (requires recompile)
        self.comp.advanced_configuration.pushOnTTValid = False
        self.port = 29495
        self.uni_ip = '127.0.0.1'
        self.multi_ip = '236.0.10.1'
        self.mserver = multicast.multicast_server(self.multi_ip, self.port)
        self.userver = unicast.unicast_server(self.uni_ip, self.port)

    def tearDown(self):
        #Tear down the rest of the object.
        ossie.utils.testing.ScaComponentTestCase.tearDown(self)
        del(self.mserver)
        del(self.userver)
        
        
        # Simulate regular component shutdown
        #sb.stop()
if __name__ == "__main__":
	ossie.utils.testing.main("../SourceSDDS.spd.xml") # By default tests all implementations
