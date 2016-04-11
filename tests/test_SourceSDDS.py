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
        self.comp.advanced_optimizations.bufferSize = 200000
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
           
        self.comp.advanced_optimizations.bufferSize = 200000
        
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
        self.comp.advanced_optimizations.bufferSize = 200000
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
        compDataOctetOut_out = self.comp.getPort('dataOctetOut')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_configuration.corbaTransferSize = 1024
        self.comp.advanced_optimizations.bufferSize = 200000
        
        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('charTest', BULKIO.SDDS_SB, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='octetIn')
        
        # Start components
        self.comp.start()
        sink.start()
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

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
        self.assertEqual(self.comp.status.packetsMissing, 0)
        
        sink.stop()
        

    # Test #8
    def testUnicastFloatPort(self):
        """Sends unicast data to the float port and asserts the data sent equals the data received"""
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataFloatOut_out = self.comp.getPort('dataFloatOut')
        
        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_configuration.corbaTransferSize = 1024
        self.comp.advanced_optimizations.bufferSize = 200000
        
        sink = sb.DataSink()
 
        streamDef = BULKIO.SDDSStreamDefinition('floatTest', BULKIO.SDDS_SF, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='floatIn')
        
        #Start components.
        self.comp.start()
        sink.start()
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

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
        self.assertEqual(self.comp.status.packetsMissing, 0)
        
        sink.stop()
        

    # Test #10
    def testUnicastShortPort(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '###################################################################'

        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('dataShortOut')
        
        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_configuration.corbaTransferSize = 1024
        self.comp.advanced_optimizations.bufferSize = 200000
        
        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('shortTest', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''
        
        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')
        
        # Start components
        self.comp.start()
        sink.start()
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

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
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_configuration.corbaTransferSize = 1024
        self.comp.advanced_optimizations.bufferSize = 200000
        
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
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_configuration.corbaTransferSize = 2048
        self.comp.advanced_optimizations.bufferSize = 200000

        sink = sb.DataSink()

        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Connect components
        self.comp.connect(sink, providesPortName='shortIn')

        # Start components
        self.comp.start()
        sink.start()
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

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

        # Validate data is correct
        # The first packet filled, the second is empty
        # XXX: YLB - Why is the second packet empty? There was good data there we've just dropped. How should we deal with dropped packets?  Pad with zeros? In this case that would create a ton of data (65536 additional packets)
        # XXX: Why not push all the data we have, bulkIO is time stamped so if we are collecting data, find we are missing a packet, we should stop buffering, send what we have, then start buffering again. (This was the advice from drwille)

        print ("YLB YLB YLB Take a look at this test, I dont think its correct")
        self.assertEqual(fakeData, list(struct.unpack('>512H', struct.pack('512h', *data[:512]))))
        self.assertEqual(list(struct.unpack('>512H', struct.pack('512h', *data[512:1024]))), [0] * 512)
        self.assertEqual(self.comp.status.packetsMissing, 65536)
        sink.stop()
        
        
    # Test #16
    # XXX: YLB Take a look at this again, it isnt actually checking any thing.
    def testUnicastAttachFail(self):
        print '###################################################################'
        print 'RUNNING TEST:',self.id()
        print '\tNote: BULKIO.dataSDDS.AttachError is expected.'
        print '###################################################################'
        
        # Get ports
        compDataSddsIn = self.comp.getPort('sdds_in')
        compDataShortOut_out = self.comp.getPort('dataShortOut')

        # Set properties
        self.comp.interface = 'lo'
        self.comp.advanced_optimizations.bufferSize = 200000
        
        streamDef = BULKIO.SDDSStreamDefinition('id', BULKIO.SDDS_SI, self.uni_ip, 0, self.port, 8000, True, 'testing')
        attachId = ''

        # Start component
        self.comp.start()
        
        # Try to attach
        try:
            attachId = compDataSddsIn.attach(streamDef, 'test') 
        except:
            attachId = ''

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
        self.comp.bufferSize = 200000  # cannot alter this except in base class loadProperties (requires recompile)
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
