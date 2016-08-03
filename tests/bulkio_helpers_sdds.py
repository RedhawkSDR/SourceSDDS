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
from bulkio.bulkioInterfaces import BULKIO, BULKIO__POA
from ossie.cf import CF, CF__POA
import time
from ossie.utils import uuid
import threading
from new import classobj
import bulkio

def create_cputime_stamp():
    """
    Generates a PrecisionUTCTime object using the current CPU time.
    
    Output:
        Returns a BULKIO.PrecisionUTCTime object
    """
    ts = time.time()
    return BULKIO.PrecisionUTCTime(BULKIO.TCM_CPU,
                                   BULKIO.TCS_VALID, 0.0,
                                   int(ts), ts - int(ts))
def compareSRI(a, b):
    return (a.hversion == b.hversion) and \
           (a.xstart == b.xstart) and \
           (a.xdelta == b.xdelta) and \
           (a.xunits == b.xunits) and \
           (a.subsize == b.subsize) and \
           (a.ystart == b.ystart) and \
           (a.ydelta == b.ydelta) and \
           (a.yunits == b.yunits) and \
           (a.mode == b.mode) and \
           (a.streamID == b.streamID) and \
           (a.keywords == b.keywords)

class ArraySource:
    """
    Simple class used to push data into a port from a given array of data.
    """
    def __init__(self, porttype):
        """
        Instantiates a new object and generates a default StreamSRI.  The 
        porttype parameter corresponds to the type of data contained in the 
        array of data being sent.  
        
        The porttype is also used in the connectPort() method to narrow the 
        connection
        
        """
        self.port_type = porttype
        self.outPorts = {}
        self.refreshSRI = False
        self.sri = BULKIO.StreamSRI(1, 0.0, 0.001, 1, 200, 0.0, 0.001, 1, 1,
                                    "ArraySourceDef",False, [])
        self.stream_id = str(uuid.uuid4())

        self.port_lock = threading.Lock()

    def connectPort(self, connection, connectionId):
        self.port_lock.acquire()
        try:
            port = connection._narrow(self.port_type)
            self.outPorts[str(connectionId)] = port
            self.refreshSRI = True
        finally:
            self.port_lock.release()

    def disconnectPort(self, connectionId):
        self.port_lock.acquire()
        try:
            self.outPorts.pop(str(connectionId), None)
        finally:
            self.port_lock.release()

    def pushSRI(self, H):
        self.sri = H
        self.port_lock.acquire()
        self.sri = H
        try:
            try:
                for connId, port in self.outPorts.items():
                    if port != None: port.pushSRI(H)
            except Exception, e:
                msg = "The call to pushSRI failed with %s " % e
                msg += "connection %s instance %s" % (connId, port)
                print(msg)
        finally:
            self.port_lock.release()

    def pushPacket(self, data, T, EOS, streamID):
        if self.refreshSRI:
            self.pushSRI(self.sri)

        self.port_lock.acquire()
        try:
            try:
                for connId, port in self.outPorts.items():
                    if port != None: port.pushPacket(data, T, EOS, streamID)
            except Exception, e:
                msg = "The call to pushPacket failed with %s " % e
                msg += "connection %s instance %s" % (connId, port)
                print(msg)
        finally:
            self.port_lock.release()

    def getPort(self):
        """
        Returns a Port object of the type CF__POA.Port.                
        """
        # The classobj generates a class using the following arguments:
        #
        #    name:        The name of the class to generate
        #    bases:       A tuple containing all the base classes to use
        #    dct:         A dictionary containing all the attributes such as
        #                 functions, and class variables
        PortClass = classobj('PortClass',
                             (CF__POA.Port,),
                             {'connectPort':self.connectPort,
                              'disconnectPort':self.disconnectPort})

        # Create a port using the generate Metaclass and return an instance 
        port = PortClass()
        return port._this()

    def run(self, data, sri = None, pktsize = 1024):
        """
        Pushes the data through the connected port.  Each packet of data 
        contains no more than pktsize elements.  Once all the elements have 
        been sent, the method sends an empty list with the EOS set to True to 
        indicate the end of the stream.
        
        Inputs:
            <data>       A list of elements containing the data to push
            <pktsize>    The maximum number of elements to send on each push
        """
        start = 0           # stores the start of the packet
        end = start         # stores the end of the packet
        sz = len(data)
        done = False
        if sri != None:
            self.sri = sri
        self.pushSRI(self.sri)
        while not done:
            chunk = start + pktsize
            # if the next chunk is greater than the file, then grab remaining
            # only, otherwise grab a whole packet size
            if chunk > sz:
                end = sz
                done = True
            else:
                end = chunk

            # there are cases when this happens: array has 6 elements and 
            # pktsize = 2 (end = length - 1 = 5 and start = 6)
            if start > end:
                done = True
                continue

            d = data[start:end]
            start = end

            T = create_cputime_stamp()
            self.pushPacket(d, T, False, self.stream_id)
        T = create_cputime_stamp()
        self.pushPacket([], T, True, self.stream_id)



class ArraySink:
    """
    Simple class used to receive data from a port and store it in an X-Midas
    file.  It uses the SRI to generate the header.  The file is created the 
    first time the SRI is pushed if the file does not exists.  If the file is 
    present, then it does not alter the file.
    """
    def __init__(self, porttype):
        """
        Instantiates a new object responsible for writing data from the port 
        into an array.
        
        It is important to notice that the porttype is a BULKIO__POA type and
        not a BULKIO type.  The reason is because it is used to generate a 
        Port class that will be returned when the getPort() is invoked.  The
        returned class is the one acting as a server and therefore must be a
        Portable Object Adapter rather and a simple BULKIO object.
        
        Inputs:
            <porttype>        The BULKIO__POA data type
        """
        self.port_type = porttype
        self.sri = BULKIO.StreamSRI(1, 0.0, 0.001, 1, 200, 0.0, 0.001, 1,
                                    1, "ArraySinkDef", False, [])
        self.data = []
        self.port_lock = threading.Lock()

    def pushSRI(self, H):
        """
        Stores the SteramSRI object regardless that there is no need for it

        Input:
            <H>    The StreamSRI object containing the information required to
                   generate the header file
        """
        self.sri = H

    def pushPacket(self, data, ts, EOS, stream_id):
        """
        Appends the data to the end of the array.
        
        Input:
            <data>        The actual data to append to the array
            <ts>          The timestamp
            <EOS>         Flag indicating if this is the End Of the Stream
            <stream_id>   The unique stream id
        """
        self.ts = ts
        self.port_lock.acquire()
        try:
            for item in data:
                self.data.append(item)
        finally:
            self.port_lock.release()

    def getPort(self):
        """
        Returns a Port object of the same type as the one specified as the 
        porttype argument during the object instantiation.  It uses the 
        classobj from the new module to generate a class on runtime.

        The classobj generates a class using the following arguments:
        
            name:        The name of the class to generate
            bases:       A tuple containing all the base classes to use
            dct:         A dictionary containing all the attributes such as
                         functions, and class variables
        
        It is important to notice that the porttype is a BULKIO__POA type and
        not a BULKIO type.  The reason is because it is used to generate a 
        Port class that will be returned when the getPort() is invoked.  The
        returned class is the one acting as a server and therefore must be a
        Portable Object Adapter rather and a simple BULKIO object.
                
        """
        # The classobj generates a class using the following arguments:
        #
        #    name:        The name of the class to generate
        #    bases:       A tuple containing all the base classes to use
        #    dct:         A dictionary containing all the attributes such as
        #                 functions, and class variables
        PortClass = classobj('PortClass',
                             (self.port_type,),
                             {'pushPacket':self.pushPacket,
                              'pushSRI':self.pushSRI})

        # Create a port using the generate Metaclass and return an instance 
        port = PortClass()
        return port._this()


class XmlArraySink(ArraySink):
    "This sub-class exists to override pushPacket for dataXML ports."
    def __init__(self, porttype):
        ArraySink.__init__(self, porttype)

    def pushPacket(self, data, EOS, stream_id):
        """
        Overrides ArraySink.pushPacket(). The difference is that there is no
        <ts> timestamp.
         
        Appends the data to the end of the array.
        
        Input:
            <data>        The actual data to append to the array
            <EOS>         Flag indicating if this is the End Of the Stream
            <stream_id>   The unique stream id
        """
        self.port_lock.acquire()
        try:
            for item in data:
                self.data.append(item)
        finally:
            self.port_lock.release()


class XmlArraySource(ArraySource):
    "This sub-class exists to override pushPacket for dataXML ports."
    def __init__(self, porttype):
        ArraySource.__init__(self, porttype)

    def pushPacket(self, data, EOS, streamID):
        """
        Overrides ArraySource.pushPacket(). The difference is that there is no
        <ts> timestamp or sri
         
        Appends the data to the end of the array.
        
        Input:
            <data>        The actual data to append to the array
            <EOS>         Flag indicating if this is the End Of the Stream
            <stream_id>   The unique stream id
        """
        self.port_lock.acquire()
        try:
            try:
                for connId, port in self.outPorts.items():
                    if port != None: port.pushPacket(data, EOS, streamID)
            except Exception, e:
                msg = "The call to pushPacket failed with %s " % e
                msg += "connection %s instance %s" % (connId, port)
                print(msg)
        finally:
            self.port_lock.release()

    def run(self, data, sri = None, pktsize = 1024):
        """
        Overrides ArraySource.run() to conform to dataXML. The difference
        is that there is no timestamp or SRI.
        
        
        Pushes the data through the connected port.  Each packet of data 
        contains no more than pktsize elements.  Once all the elements have 
        been sent, the method sends an empty list with the EOS set to True to 
        indicate the end of the stream.
        
        Inputs:
            <data>       A list of elements containing the data to push
            <pktsize>    The maximum number of elements to send on each push
        """
        start = 0           # stores the start of the packet
        end = start         # stores the end of the packet
        sz = len(data)
        eos = False

        while not eos:
            chunk = start + pktsize
            # if the next chunk is greater than the file, then grab remaining
            # only, otherwise grab a whole packet size
            if chunk > sz:
                end = sz
                eos = True
            else:
                end = chunk

            # there are cases when this happens: array has 6 elements and 
            # pktsize = 2 (end = length - 1 = 5 and start = 6)
            if start > end:
                eos = True
                continue

            d = data[start:end]
            start = end

            self.pushPacket(d, eos, self.stream_id)


class SddsArraySource(ArraySource):
    """
    Simple class used to push data into a port from a given array of data.
    """
    def __init__(self, porttype):
        ArraySource.__init__(self, porttype)
        self.sriChanged = False

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


class SddsArraySink(ArraySink):
    def __init__(self, porttype):
        ArraySink.__init__(self, porttype)

