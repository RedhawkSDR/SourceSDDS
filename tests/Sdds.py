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
import array
import struct


class SddsHeader:
    '''Represents the SDDS header fields.
    
    The header is 56 octets
    '''
    def __init__(self, FSN,
                 SF = 1, SoS = 1, PP = 0, OF = 0, SS = 0, DM = [0, 1, 0], BPS = [1, 0, 0, 0, 0], CX = 0 ,
                 MSV = 0, TTV = 1, SSV = 1,
                 MSD = 0, TT = 0, TTE = 0,
                 DFDT = 0, FREQ = 125000000):
        #######################################################################
        # The SF (Standard Format) field is used to identify whether or not the
        # packet conforms to the SDDS standard. For SDDS standard packets, the
        # SF bit shall be set to a value of one. The SF bit shall be set to a
        # value of zero for non-standard packets. SDDS standard packets are
        # signal data and parity packets that conform to this ICD. SDDS output
        # formatters will only process SDDS standard packets. Non-standard
        # packets provide an avenue for future Gigabit Ethernet sources to use a
        # different packet structure.
        self.SF = SF # Standard format
        t = [self.SF, ]

        # The value of the SoS field is set to 1 for the first 2^16 packets of a
        # new sequence. The SoS field is set to zero for all other packets in a
        # sequence.
        self.SoS = SoS # Start of Sequence
        t.extend((self.SoS,))

        # The PP (Parity Packet) identifier field, combined with the frame
        # sequence number is used to identify whether the packet contains signal
        # data, or is a parity information according to 
        # PP ID Value     Is Frame Seq. Number     Packet Type
        #                 equal to 31 mod 32?
        #     0                No                     Signal Data
        #     0               Yes                       Invalid
        #     1                No                       Invalid
        #     1               Yes                        Parity

        # ...However:

        # Another legacy feature of the SDDS Packet format was the ability for
        # every thirty-second packet to be a "parity packet" for the immediately
        # preceding thirty-one packets. The intent here was to permit recreation
        # of the occasional lost packet. The technical aspects of implementing
        # such a scheme (~3% extra bandwidth on the routers, ~3% larger send and
        # receive buffers, x32 increase in packet jitter, etc.) and extremely
        # low packet loss on properly-configured networks means that it has
        # never been implemented. As a result of building this capability into
        # the specification, the Frame Sequence Number skips every thirty-second
        # value starting with number 31 (i.e. the counter counts 0, 1, 2, ...,
        # 29, 30, 32, 33, 34, ..., 61, 62, 64, 65, etc.)
        self.PP = PP # Parity packet
        t.extend((self.PP,))

        # The OF (Original Format) field identifies the original format of the
        # data transmitted. If the data was originally offset binary and has
        # been converted to 2's complement, the OF value is set to one.
        # Otherwise, the data is 2's complement and has not been converted and
        # the OF value is set to zero.
        self.OF = OF # Original format
        t.extend((self.OF,))

        # The SS (Spectral Sense) field identifies whether or not the spectral
        # sense has been inverted from the original input. The SS value is set
        # to one if the spectral sense has been inverted. The SS value is set to
        # zero if the spectral sense has not been inverted.
        self.SS = SS # Spectral sense
        t.extend((self.SS,))

        self.dataMode = DM
        t.extend(self.dataMode)

        self.CX = CX
        t.extend((self.CX,))

        t.extend([0, 0]) # padding

        self.bps = BPS # Bits/sample
        t.extend(self.bps)

        # Format Identifier (2 bytes)
        t.reverse()
        f = [x << n for n, x in enumerate(t)]
        self.formatIdentifer = reduce(lambda x, y: x + y, f)

        ####################################################################### 
        # Frame Sequence number (2 bytes)
        self.fsn = FSN

        #######################################################################
        # Time Tag Info

        #The 1-ms valid field indicates the status of the 1-ms pointer and 1-ms
        #delta fields: One indicates the values stored in the 1-ms pointer and
        #1-ms delta fields are valid. Zero indicates the values stored in the
        #1-ms pointer and 1-ms delta fields are not valid.
        self.msv = MSV # 1ms ptr valid
        t = [self.msv, ]

        #The TT valid field indicates the status of the Time Tag information
        #fields: One indicates the values stored in the Time Tag information
        #fields are valid. Zero indicates the values stored in the Time Tag
        #information fields are not valid.
        self.ttv = TTV # Time tag valid
        t.extend((self.ttv,))

        # The SSC valid field indicates the status of the Synchronous Sample
        # Clock (SSC) information fields (see below): One indicates the values
        # stored in the SSC information fields are valid. Zero indicates the
        # values stored in the SSC information fields are not valid.
        self.ssv = SSV # SSC valid
        t.extend((self.ssv,))

        t.extend([0, 0]) # padding

        # When the 1-ms pointer field is valid, the 11-bit field is an integer
        # representing the first sample number in the payload field that
        # occurred after the 1-millisecond event.
        t.extend([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]) # time code 1ms ptr

        # Time Tag Info (2 bytes)
        t.reverse()
        f = [x << n for n, x in enumerate(t)]
        self.ttInfo = reduce(lambda x, y: x + y, f)

        # When the 1-ms delta field is valid, the field is a 16-bit integer
        # counting 250-picosecond clocks between the 1-millisecond event and the
        # occurrence of the SSC clock edge for the sample referenced by the 1-ms
        # pointer value.
        self.timeCode1msDelta = MSD  #Time code 1ms delta (2bytes)

        # The data time tag shall be an eight-byte (64-bit) integer with the
        # least significant bit corresponding to 250-picoseconds. The number
        # contained in this field is the number of 250-picosecond clocks that
        # have occurred since the fixed reference time of 1-January of the
        # current year 00:00:00 UTC.
        self.timeTag = TT  # Time Tag (normal precision, 8 bytes


        # The Time Tag extension field extends the precision of the time tag
        # field by an additional 32 bits. The field is an unsigned integer with
        # the LSB corresponding to 250/2^32 picoseconds.
        self.timeTagExt = TTE  # Time Tag (extended precision, 4 bytes)

        #######################################################################
        # The dF/dT field shall be a 32-bit two's complement number measuring
        # the rate at which the frequency is changing. The LSB value is equal to
        # (2-Hz/Sec) / 2^31. This value will represent the delta between the
        # instantaneous frequency of the last SSC of the packet and the
        # instantaneous frequency of the first SSC of the packet divided by the
        # packet duration.
        self.dfdt = DFDT # df/dt (4 bytes)

        #######################################################################
        # The frequency field shall be a 64-bit signed number containing the
        # frequency of the SSC in two's complement notation with the LSB value
        # equal to (125-MHz) / 2^63. This value will represent the instantaneous
        # frequency of the SSC associated with the first sample of the frame.
        self.freq = FREQ # (8 bytes)

        #######################################################################
        # 24 bytes
        self.ssdAad = [x for x in range(0, 24)]

        # Format Identifier, Frame Sequence, Time Tag information, and SSC
        # information (32B -- HHHHQLLQ)

        # H - Format Identifier
        # H - Frame Sequence
        # Time Tag Info
        #   H - bit map
        #   H - 1 ms Delta
        #   Q - Time tag
        #   L - Time tag ext
        # (Synchronous Sample Clock) SSC Information
        #   L - df/dt
        #   Q - freq
        # Synchronous Serial Data (SSD) + Asynchronous Auxiliary Data (AAD)
        #  24 B is composed of 4 bytes SSD information and 20 bytes AAD info

        self.header = [self.formatIdentifer,
                       self.fsn,
                       self.ttInfo,
                       self.timeCode1msDelta,
                       self.timeTag,
                       self.timeTagExt,
                       self.dfdt,
                       self.freq]
        #  SSD and AAD (24B)
        self.header.extend(self.ssdAad)


class SddsPacket:
    '''The whole SDDS packet.
    
    The 56 octet header plus the 1024 octet data package means the packet is
    always 1080 bytes long.
    '''
    def __init__(self, header, data):
        self.p = header
        self.p.extend(data)


class SddsCharPacket(SddsPacket):
    def __init__(self, header, data):
        SddsPacket.__init__(self, header, data)

    def encode(self):
        self.encodedPacket = struct.pack('>HHHHQLLQ24B1024B', *self.p)


class SddsShortPacket(SddsPacket):
    def __init__(self, header, data):
        SddsPacket.__init__(self, header, data)

    def encode(self):
        self.encodedPacket = struct.pack('>HHHHQLLQ24B512H', *self.p)


class SddsFloatPacket(SddsPacket):
    def __init__(self, header, data):
        SddsPacket.__init__(self, header, data)

    def encode(self):
        self.encodedPacket = struct.pack('>HHHHQLLQ24B256f', *self.p)

if __name__ == '__main__':
    h = SddsHeader(7)
    fakeData = [x for x in range(0, 512)]
    p = SddsPacket(h.header, fakeData)
    p.encode()

    print len(p.encodedPacket)
    print struct.unpack('540H', p.encodedPacket)
