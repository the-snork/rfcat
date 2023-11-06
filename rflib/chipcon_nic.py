#!/usr/bin/env ipython3

from __future__ import print_function
from __future__ import absolute_import
from __future__ import division

from builtins import bytes
from builtins import hex
from builtins import range
from builtins import object
from past.utils import old_div
import re
import sys
import usb
import code
import time
import struct
import pickle
import threading
from .chipcon_usb import *
from .bits import correctbytes, ord23
from .const import *
from binascii import hexlify

def makeFriendlyAscii(instring):
    out = []
    start = 0
    last = -1
    instrlen = len(instring)

    for cidx in range(instrlen):
        if (0x20 < ord23(instring[cidx]) < 0x7f):
            if last < cidx-1:
                out.append( b"." * (cidx-1-last))
                start = cidx
            last = cidx
        else:
            if last == cidx-1:
                out.append( instring[ start:last+1 ] )

    if last != cidx:
        out.append( b"." * (cidx-last) )
    else: # if start == 0:
        out.append( instring[ start: ] )

    return b''.join(out).decode()

class NICxx11(USBDongle):
    '''
    NICxx11 implements radio-specific code for CCxx11 chips (2511, 1111),
    as the xx11 chips keep a relatively consistent radio interface and use
    the same radio concepts (frequency, channels, etc) and functionality
    (AES, Manchester Encoding, etc).
    '''
    def __init__(self, idx=0, debug=False, copyDongle=None, RfMode=RFST_SRX, safemode=False):
        USBDongle.__init__(self, idx, debug, copyDongle, RfMode, safemode=safemode)
        self.max_packet_size = RF_MAX_RX_BLOCK
        self.endec = None
        if hasattr(self, "chipnum"):
            self.mhz = CHIPmhz.get(self.chipnum)
        else:
            self.mhz = 24   # default CC1111

        self.freq_offset_accumulator = 0

    ######## RADIO METHODS #########
    def setRfMode(self, rfmode, parms=b''):
        '''
        sets the radio state to "rfmode", and makes
        '''
        self._rfmode = rfmode
        r = self.send(APP_SYSTEM, SYS_CMD_RFMODE, b"%c" % (self._rfmode) + parms)

    ### set standard radio state to TX/RX/IDLE (TX is pretty much only good for jamming).  TX/RX modes are set to return to whatever state you choose here.
    def setModeTX(self):
        '''
        BOTH: set radio to TX state
        AND:  set radio to return to TX state when done with other states
        '''
        self.setRfMode(RFST_STX)       #FIXME: when firmware makes the change, so must this

    def setModeRX(self):
        '''
        BOTH: set radio to RX state
        AND:  set radio to return to RX state when done with other states
        '''
        self.setRfMode(RFST_SRX)

    def setModeIDLE(self):
        '''
        BOTH: set radio to IDLE state
        AND:  set radio to return to IDLE state when done with other states
        '''
        self.setRfMode(RFST_SIDLE)


    ### send raw state change to radio (doesn't update the return state for after RX/TX occurs)
    def strobeModeTX(self):
        '''
        set radio to TX state (transient)
        '''
        self.poke(X_RFST, b"%c"%RFST_STX)

    def strobeModeRX(self):
        '''
        set radio to RX state (transient)
        '''
        self.poke(X_RFST, b"%c"%RFST_SRX)

    def strobeModeIDLE(self):
        '''
        set radio to IDLE state (transient)
        '''
        self.poke(X_RFST, b"%c"%RFST_SIDLE)

    def strobeModeFSTXON(self):
        '''
        set radio to FSTXON state (transient)
        '''
        self.poke(X_RFST, b"%c"%RFST_SFSTXON)

    def strobeModeCAL(self):
        '''
        set radio to CAL state (will return to whichever state is configured (via setMode* functions)
        '''
        self.poke(X_RFST, b"%c"%RFST_SCAL)

    def strobeModeReturn(self, marcstate=None):
        """
        attempts to return the the correct mode after configuring some radio register(s).
        it uses the marcstate provided (or self.radiocfg.marcstate if none are provided) to determine how to strobe the radio.
        """
        #if marcstate is None:
            #marcstate = self.radiocfg.marcstate
        #if self._debug: print("MARCSTATE: %x   returning to %x" % (marcstate, MARC_STATE_MAPPINGS[marcstate][2]) )
        #self.poke(X_RFST, b"%c"%MARC_STATE_MAPPINGS[marcstate][2])
        self.poke(X_RFST, b"%c" % self._rfmode)




    #### radio config #####
    def getRadioConfig(self):
        bytedef = self.peek(0xdf00, 0x3e)
        self.radiocfg.vsParse(bytedef)
        return bytedef

    def setRadioConfig(self, bytedef = None):
        if bytedef is None:
            bytedef = self.radiocfg.vsEmit()

        statestr, marcstate = self.getMARCSTATE()
        if marcstate != MARC_STATE_IDLE:
            self.strobeModeIDLE()

        self.poke(0xdf00, bytedef)

        self.strobeModeReturn(marcstate)

        self.getRadioConfig()

        return bytedef


    ##### GETTER/SETTERS for Radio Config/Status #####
    ### radio state
    def getMARCSTATE(self, radiocfg=None):
        if radiocfg is None:
            self.getRadioConfig()
            radiocfg=self.radiocfg

        mode = radiocfg.marcstate
        return (MODES.get(mode), mode)

    def setRFRegister(self, regaddr, value, suppress=False):
        '''
        set the radio register 'regaddr' to 'value' (first setting RF state to IDLE, then returning to RX/TX)
            value is always considered a 1-byte value
            if 'suppress' the radio state (RX/TX/IDLE) is not modified
        '''
        if suppress:
            self.poke(regaddr, correctbytes(value))
            return

        marcstate = self.radiocfg.marcstate
        if marcstate != MARC_STATE_IDLE:
            self.strobeModeIDLE()

        self.poke(regaddr, correctbytes(value))

        self.strobeModeReturn(marcstate)
        #if (marcstate == MARC_STATE_RX):
            #self.strobeModeRX()
        #elif (marcstate == MARC_STATE_TX):
            #self.strobeModeTX()
        # if other than these, we can stay in IDLE

    def setFreq(self, freq=902000000, mhz=24, radiocfg=None, applyConfig=True):
        if radiocfg is None:
            radiocfg = self.radiocfg
        else:
            applyConfig = False

        freqmult = old_div((0x10000 / 1000000.0), mhz)
        num = int(freq * freqmult)
        radiocfg.freq2 = num >> 16
        radiocfg.freq1 = (num>>8) & 0xff
        radiocfg.freq0 = num & 0xff

        if (freq > FREQ_EDGE_900 and freq < FREQ_MID_900) or (freq > FREQ_EDGE_400 and freq < FREQ_MID_400) or (freq < FREQ_MID_300):
            # select low VCO
            radiocfg.fscal2 = 0x0A
        elif freq <1e9 and ((freq > FREQ_MID_900) or (freq > FREQ_MID_400) or (freq > FREQ_MID_300)):
            # select high VCO
            radiocfg.fscal2 = 0x2A

        if applyConfig:
            marcstate = radiocfg.marcstate
            if marcstate != MARC_STATE_IDLE:
                self.strobeModeIDLE()
            self.poke(FREQ2, struct.pack("3B", self.radiocfg.freq2, self.radiocfg.freq1, self.radiocfg.freq0))
            self.poke(FSCAL2, struct.pack("B", self.radiocfg.fscal2))

            self.strobeModeReturn(marcstate)
            #if (radiocfg.marcstate == MARC_STATE_RX):
                #self.strobeModeRX()
            #elif (radiocfg.marcstate == MARC_STATE_TX):
                #self.strobeModeTX()

    def getFreq(self, mhz=24, radiocfg=None):
        freqmult = old_div((0x10000 / 1000000.0), mhz)
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        num = (radiocfg.freq2<<16) + (radiocfg.freq1<<8) + radiocfg.freq0
        freq = old_div(num, freqmult)
        return freq, hex(num)

    def getFreqEst(self, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        return radiocfg.freqest

    # set 'standard' power - for more complex power shaping this will need to be done manually
    def setPower(self, power=None, radiocfg=None, invert=False):
        if radiocfg == None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        mod= self.getMdmModulation(radiocfg=radiocfg)

        # we may be only changing PA_POWER, not power levels
        if power is not None:
            if mod == MOD_ASK_OOK and not invert:
                radiocfg.pa_table0= 0x00
                radiocfg.pa_table1= power
            else:
                radiocfg.pa_table0= power
                radiocfg.pa_table1= 0x00
            self.setRFRegister(PA_TABLE0, radiocfg.pa_table0)
            self.setRFRegister(PA_TABLE1, radiocfg.pa_table1)

        radiocfg.frend0 &= ~FREND0_PA_POWER
        if mod == MOD_ASK_OOK:
            radiocfg.frend0 |= 0x01

        self.setRFRegister(FREND0, radiocfg.frend0)

    def getMdmModulation(self, radiocfg=None):
        if radiocfg == None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        mdmcfg2 = radiocfg.mdmcfg2
        mod = (mdmcfg2) & MDMCFG2_MOD_FORMAT
        return mod

    def getMdmChanSpc(self, mhz=24, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        chanspc_m = radiocfg.mdmcfg0
        chanspc_e = radiocfg.mdmcfg1 & 3
        chanspc = 1000000.0 * mhz/pow(2,18) * (256 + chanspc_m) * pow(2, chanspc_e)
        #print "chanspc_e: %x   chanspc_m: %x   chanspc: %f hz" % (chanspc_e, chanspc_m, chanspc)
        return (chanspc)

    def setMdmChanSpc(self, chanspc=None, chanspc_m=None, chanspc_e=None, mhz=24, radiocfg=None):
        '''
        calculates the appropriate exponent and mantissa and updates the correct registers
        chanspc is in kHz.  if you prefer, you may set the chanspc_m and chanspc_e settings
        directly.

        only use one or the other:
        * chanspc
        * chanspc_m and chanspc_e
        '''
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        if (chanspc != None):
            for e in range(4):
                m = int(((old_div(chanspc * pow(2,18), (1000000.0 * mhz * pow(2,e))))-256) +.5)    # rounded evenly
                if m < 256:
                    chanspc_e = e
                    chanspc_m = m
                    break
        if chanspc_e is None or chanspc_m is None:
            raise Exception("ChanSpc does not translate into acceptable parameters.  Should you be changing this?")

        #chanspc = 1000000.0 * mhz/pow(2,18) * (256 + chanspc_m) * pow(2, chanspc_e)
        #print "chanspc_e: %x   chanspc_m: %x   chanspc: %f hz" % (chanspc_e, chanspc_m, chanspc)

        radiocfg.mdmcfg1 &= ~MDMCFG1_CHANSPC_E            # clear out old exponent value
        radiocfg.mdmcfg1 |= chanspc_e
        radiocfg.mdmcfg0 = chanspc_m
        self.setRFRegister(MDMCFG1, (radiocfg.mdmcfg1))
        self.setRFRegister(MDMCFG0, (radiocfg.mdmcfg0))

    def makePktFLEN(self, flen=RF_MAX_TX_BLOCK, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        if flen > EP5OUT_BUFFER_SIZE - 4:
            raise Exception("Packet too large (%d bytes). Maximum fixed length packet is %d bytes." % (flen, EP5OUT_BUFFER_SIZE - 6))

        radiocfg.pktctrl0 &= ~PKTCTRL0_LENGTH_CONFIG
        # if we're sending a large block, pktlen is dealt with by the firmware
        # using 'infinite' mode
        if flen > RF_MAX_TX_BLOCK:
            radiocfg.pktlen = 0x00
        else:
            radiocfg.pktlen = flen
        self.setRFRegister(PKTCTRL0, (radiocfg.pktctrl0))
        self.setRFRegister(PKTLEN, (radiocfg.pktlen))

    def getPktLEN(self):
        '''
        returns (pktlen, pktctrl0)
        '''
        return (self.radiocfg.pktlen, self.radiocfg.pktctrl0 & PKTCTRL0_LENGTH_CONFIG)

    def setEnablePktCRC(self, enable=True, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        crcE = (0,1)[enable]<<2
        crcM = ~(1<<2)
        radiocfg.pktctrl0 &= crcM
        radiocfg.pktctrl0 |= crcE
        self.setRFRegister(PKTCTRL0, (radiocfg.pktctrl0))

    def setEnablePktDataWhitening(self, enable=True, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        dwEnable = (0,1)[enable]<<6
        radiocfg.pktctrl0 &= ~PKTCTRL0_WHITE_DATA
        radiocfg.pktctrl0 |= dwEnable
        self.setRFRegister(PKTCTRL0, (radiocfg.pktctrl0))

    def setPktPQT(self, num=3, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        num &=  7
        num <<= 5
        numM = ~(7<<5)
        radiocfg.pktctrl1 &= numM
        radiocfg.pktctrl1 |= num
        self.setRFRegister(PKTCTRL1, (radiocfg.pktctrl1))

    def getEnableMdmManchester(self, radiocfg=None):
        if radiocfg == None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        mdmcfg2 = radiocfg.mdmcfg2
        mchstr = (mdmcfg2>>3) & 0x01
        return mchstr

    def setEnableMdmFEC(self, enable=True, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        fecEnable = (0,1)[enable]<<7
        radiocfg.mdmcfg1 &= ~MFMCFG1_FEC_EN
        radiocfg.mdmcfg1 |= fecEnable
        self.setRFRegister(MDMCFG1, (radiocfg.mdmcfg1))

    def getEnableMdmFEC(self, radiocfg=None):
        if radiocfg == None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        mdmcfg1 = radiocfg.mdmcfg1
        fecEnable = (mdmcfg1>>7) & 0x01
        return fecEnable

    def getEnableMdmDCFilter(self, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        dcfEnable = (radiocfg.mdmcfg2>>7) & 0x1
        return dcfEnable

    def setMdmChanBW(self, bw, mhz=24, radiocfg=None):
        '''
        For best performance, the channel filter
        bandwidth should be selected so that the
        signal bandwidth occupies at most 80% of the
        channel filter bandwidth. The channel centre
        tolerance due to crystal accuracy should also
        be subtracted from the signal bandwidth. The
        following example illustrates this:

            With the channel filter bandwidth set to 500
            kHz, the signal should stay within 80% of 500
            kHz, which is 400 kHz. Assuming 915 MHz
            frequency and +/-20 ppm frequency uncertainty
            for both the transmitting device and the
            receiving device, the total frequency
            uncertainty is +/-40 ppm of 915 MHz, which is
            +/-37 kHz. If the whole transmitted signal
            bandwidth is to be received within 400 kHz, the
            transmitted signal bandwidth should be
            maximum 400 kHz - 2*37 kHz, which is 326
            kHz.

        DR:1.2kb Dev:5.1khz Mod:GFSK RXBW:63kHz sensitive     fsctrl1:06 mdmcfg:e5 a3 13 23 11 dev:16 foc/bscfg:17/6c agctrl:03 40 91 frend:56 10
        DR:1.2kb Dev:5.1khz Mod:GFSK RXBW:63kHz lowpower      fsctrl1:06 mdmcfg:e5 a3 93 23 11 dev:16 foc/bscfg:17/6c agctrl:03 40 91 frend:56 10    (DEM_DCFILT_OFF)
        DR:2.4kb Dev:5.1khz Mod:GFSK RXBW:63kHz sensitive     fsctrl1:06 mdmcfg:e6 a3 13 23 11 dev:16 foc/bscfg:17/6c agctrl:03 40 91 frend:56 10
        DR:2.4kb Dev:5.1khz Mod:GFSK RXBW:63kHz lowpower      fsctrl1:06 mdmcfg:e6 a3 93 23 11 dev:16 foc/bscfg:17/6c agctrl:03 40 91 frend:56 10    (DEM_DCFILT_OFF)
        DR:38.4kb Dev:20khz Mod:GFSK RXBW:94kHz sensitive     fsctrl1:08 mdmcfg:ca a3 13 23 11 dev:36 foc/bscfg:16/6c agctrl:43 40 91 frend:56 10    (IF changes, Deviation)
        DR:38.4kb Dev:20khz Mod:GFSK RXBW:94kHz lowpower      fsctrl1:08 mdmcfg:ca a3 93 23 11 dev:36 foc/bscfg:16/6c agctrl:43 40 91 frend:56 10    (.. DEM_DCFILT_OFF)

        DR:250kb Dev:129khz Mod:GFSK RXBW:600kHz sensitive    fsctrl1:0c mdmcfg:1d 55 13 23 11 dev:63 foc/bscfg:1d/1c agctrl:c7 00 b0 frend:b6 10    (IF_changes, Deviation)

        DR:500kb            Mod:MSK  RXBW:750kHz sensitive    fsctrl1:0e mdmcfg:0e 55 73 43 11 dev:00 foc/bscfg:1d/1c agctrl:c7 00 b0 frend:b6 10    (IF_changes, Modulation of course, Deviation has different meaning with MSK)
        '''
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        chanbw_e = None
        chanbw_m = None
        for e in range(4):
            m = int(((old_div(mhz*1000000.0, (bw *pow(2,e) * 8.0 ))) - 4) + .5)        # rounded evenly
            if m < 4:
                chanbw_e = e
                chanbw_m = m
                break
        if chanbw_e is None:
            raise Exception("ChanBW does not translate into acceptable parameters.  Should you be changing this?")

        bw = 1000.0*mhz / (8.0*(4+chanbw_m) * pow(2,chanbw_e))
        #print "chanbw_e: %x   chanbw_m: %x   chanbw: %f kHz" % (e, m, bw)

        radiocfg.mdmcfg4 &= ~(MDMCFG4_CHANBW_E | MDMCFG4_CHANBW_M)
        radiocfg.mdmcfg4 |= ((chanbw_e<<6) | (chanbw_m<<4))
        self.setRFRegister(MDMCFG4, (radiocfg.mdmcfg4))

        # from http://www.cs.jhu.edu/~carlson/download/datasheets/ask_ook_settings.pdf
        if bw > 102e3:
            self.setRFRegister(FREND1, 0xb6)
        else:
            self.setRFRegister(FREND1, 0x56)

        if bw > 325e3:
            self.setRFRegister(TEST2, 0x88)
            self.setRFRegister(TEST1, 0x31)
        else:
            self.setRFRegister(TEST2, 0x81)
            self.setRFRegister(TEST1, 0x35)

    def getMdmChanBW(self, mhz=24, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        chanbw_e = (radiocfg.mdmcfg4 >> 6) & 0x3
        chanbw_m = (radiocfg.mdmcfg4 >> 4) & 0x3
        bw = 1000000.0*mhz / (8.0*(4+chanbw_m) * pow(2,chanbw_e))
        #print "chanbw_e: %x   chanbw_m: %x   chanbw: %f hz" % (chanbw_e, chanbw_m, bw)
        return bw

    def setMdmDRate(self, drate, mhz=24, radiocfg=None):
        '''
        set the baud of data being modulated through the radio
        '''
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        drate_e = None
        drate_m = None
        for e in range(16):
            m = int((old_div(drate * pow(2,28), (pow(2,e)* (mhz*1000000.0)))-256) + .5)        # rounded evenly
            if m < 256:
                drate_e = e
                drate_m = m
                break
        if drate_e is None:
            raise Exception("DRate does not translate into acceptable parameters.  Should you be changing this?")

        drate = 1000000.0 * mhz * (256+drate_m) * pow(2,drate_e) / pow(2,28)
        if self._debug: print("drate_e: %x   drate_m: %x   drate: %f Hz" % (drate_e, drate_m, drate))

        radiocfg.mdmcfg3 = drate_m
        radiocfg.mdmcfg4 &= ~MDMCFG4_DRATE_E
        radiocfg.mdmcfg4 |= drate_e
        self.setRFRegister(MDMCFG3, (radiocfg.mdmcfg3))
        self.setRFRegister(MDMCFG4, (radiocfg.mdmcfg4))

    def getMdmDRate(self, mhz=24, radiocfg=None):
        '''
        get the baud of data being modulated through the radio
        '''
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        drate_e = radiocfg.mdmcfg4 & 0xf
        drate_m = radiocfg.mdmcfg3

        drate = 1000000.0 * mhz * (256+drate_m) * pow(2,drate_e) / pow(2,28)
        #print "drate_e: %x   drate_m: %x   drate: %f hz" % (drate_e, drate_m, drate)
        return drate


    def setMdmDeviatn(self, deviatn, mhz=24, radiocfg=None):
        '''
        configure the deviation settings for the given modulation scheme
        '''
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        dev_e = None
        dev_m = None
        for e in range(8):
            m = int((old_div(deviatn * pow(2,17), (pow(2,e)* (mhz*1000000.0)))-8) + .5)        # rounded evenly
            if m < 8:
                dev_e = e
                dev_m = m
                break
        if dev_e is None:
            raise Exception("Deviation does not translate into acceptable parameters.  Should you be changing this?")

        dev = 1000000.0 * mhz * (8+dev_m) * pow(2,dev_e) / pow(2,17)
        #print "dev_e: %x   dev_m: %x   deviatn: %f Hz" % (e, m, dev)

        radiocfg.deviatn = (dev_e << 4) | dev_m
        self.setRFRegister(DEVIATN, radiocfg.deviatn)

    def getMdmDeviatn(self, mhz=24, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        dev_e = radiocfg.deviatn >> 4
        dev_m = radiocfg.deviatn & DEVIATN_DEVIATION_M
        dev = 1000000.0 * mhz * (8+dev_m) * pow(2,dev_e) / pow(2,17)
        return dev

    def setMdmSyncWord(self, word, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        radiocfg.sync1 = word >> 8
        radiocfg.sync0 = word & 0xff
        self.setRFRegister(SYNC1, (radiocfg.sync1))
        self.setRFRegister(SYNC0, (radiocfg.sync0))

    def getMdmSyncMode(self, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        return radiocfg.mdmcfg2 & MDMCFG2_SYNC_MODE

    def setMdmSyncMode(self, syncmode=SYNCM_15_of_16, radiocfg=None):
        if radiocfg==None:
            self.getRadioConfig()
            radiocfg = self.radiocfg

        radiocfg.mdmcfg2 &= ~MDMCFG2_SYNC_MODE
        radiocfg.mdmcfg2 |= syncmode
        self.setRFRegister(MDMCFG2, (radiocfg.mdmcfg2))

    ##### RADIO XMIT/RECV and UTILITY FUNCTIONS #####
    # set repeat & offset to optionally repeat tx of a section of the data block. repeat of 65535 means 'forever'
    def RFxmit(self, data, repeat=0, offset=0):
        # encode, if necessary
        if self.endec is not None:
            data = self.endec.encode(data)

        if len(data) > RF_MAX_TX_BLOCK:
            if repeat or offset:
                return PY_TX_BLOCKSIZE_INCOMPAT
            return self.RFxmitLong(data, doencoding=False)

        # calculate wait time
        waitlen = len(data)
        waitlen += repeat * (len(data) - offset)
        wait = USB_TX_WAIT * ((old_div(waitlen, RF_MAX_TX_BLOCK)) + 1)
        self.send(APP_NIC, NIC_XMIT, b"%s" % struct.pack("<HHH",len(data),repeat,offset)+data, wait=wait)

    def RFxmitLong(self, data, doencoding=True):
        # encode, if necessary
        if self.endec is not None and doencoding:
            data = self.endec.encode(data)

        if len(data) > RF_MAX_TX_LONG:
            return PY_TX_BLOCKSIZE_TOO_LARGE

        datalen = len(data)

        # calculate wait time
        waitlen = len(data)
        wait = USB_TX_WAIT * ((old_div(waitlen, RF_MAX_TX_BLOCK)) + 1)


        # load chunk buffers
        chunks = []
        for x in range(old_div(datalen, RF_MAX_TX_CHUNK)):
            chunks.append(data[x * RF_MAX_TX_CHUNK:(x + 1) * RF_MAX_TX_CHUNK])
        if datalen % RF_MAX_TX_CHUNK:
            chunks.append(data[-(datalen % RF_MAX_TX_CHUNK):])

        preload = old_div(RF_MAX_TX_BLOCK, RF_MAX_TX_CHUNK)
        retval, ts = self.send(APP_NIC, NIC_LONG_XMIT, b"%s" % struct.pack("<HB",datalen,preload)+data[:RF_MAX_TX_CHUNK * preload], wait=wait*preload)
        #sys.stderr.write('=' + repr(retval))
        error = struct.unpack(b"<B", retval[0:1])[0]
        if error:
            return error

        chlen = len(chunks)
        for chidx in range(preload, chlen):
            chunk = chunks[chidx]
            error = RC_TEMP_ERR_BUFFER_NOT_AVAILABLE
            while error == RC_TEMP_ERR_BUFFER_NOT_AVAILABLE:
                retval,ts = self.send(APP_NIC, NIC_LONG_XMIT_MORE, b"%s" % struct.pack("B", len(chunk))+chunk, wait=wait)
                error = struct.unpack(b"<B", retval[0:1])[0]
            if error:
                return error
                #if error == RC_TEMP_ERR_BUFFER_NOT_AVAILABLE:
                #    sys.stderr.write('.')
            #sys.stderr.write('+')
        # tell dongle we've finished
        retval,ts = self.send(APP_NIC, NIC_LONG_XMIT_MORE, b"%s" % struct.pack("B", 0), wait=wait)
        return struct.unpack("<b", retval[0:1])[0]

    # set blocksize to larger than 255 to receive large blocks or 0 to revert to normal
    def RFrecv(self, timeout=USB_RX_WAIT, blocksize=None):
        if not blocksize == None:
            if blocksize > EP5OUT_BUFFER_SIZE:
                raise Exception("Blocksize too large. Maximum %d" % EP5OUT_BUFFER_SIZE)
            self.send(APP_NIC, NIC_SET_RECV_LARGE, b"%s" % struct.pack("<H",blocksize))
        data = self.recv(APP_NIC, NIC_RECV, timeout)
        # decode, if necessary
        if self.endec is not None:
            # strip off timestamp, process data, then reapply timestamp to continue
            msg, ts = data
            msg = self.endec.decode(msg)
            data = msg, ts

        return data

    def RFlisten(self):
        '''
        just sit and dump packets as they come in
        kinda like discover() but without changing any of the communications settings
        '''
        print("Entering RFlisten mode...  packets arriving will be displayed on the screen")
        print("(press Enter to stop)")
        while not keystop():

            try:
                y, t = self.RFrecv()
                print("(%5.3f) Received:  %s  | %s" % (t, hexlify(y).decode(), makeFriendlyAscii(y)))

            except ChipconUsbTimeoutException:
                pass
            except KeyboardInterrupt:
                print("Please press <enter> to stop")

        sys.stdin.read(1)

    def lowball(self, level=1, sync=0xaaaa, length=250, pqt=0, crc=False, fec=False, datawhite=False):
        '''
        this configures the radio to the lowest possible level of filtering, potentially allowing complete radio noise to come through as data.  very useful in some circumstances.
        level == 0 changes the Sync Mode to SYNCM_NONE (wayyy more garbage)
        level == 1 (default) sets the Sync Mode to SYNCM_CARRIER (requires a valid carrier detection for the data to be considered a packet)
        level == 2 sets the Sync Mode to SYNCM_CARRIER_15_of_16 (requires a valid carrier detection and 15 of 16 bits of SYNC WORD match for the data to be considered a packet)
        level == 3 sets the Sync Mode to SYNCM_CARRIER_16_of_16 (requires a valid carrier detection and 16 of 16 bits of SYNC WORD match for the data to be considered a packet)
        '''
        if hasattr(self, '_last_radiocfg') and len(self._last_radiocfg):
            print('not saving radio state.  already have one saved.  use lowballRestore() to restore the saved config and the next time you run lowball() the radio config will be saved.')
        else:
            self._last_radiocfg = self.getRadioConfig()

        self.makePktFLEN(length)
        self.setEnablePktCRC(crc)
        self.setEnableMdmFEC(fec)
        self.setEnablePktDataWhitening(datawhite)
        self.setMdmSyncWord(sync)
        self.setPktPQT(pqt)

        if (level == 3):
            self.setMdmSyncMode(SYNCM_CARRIER_16_of_16)
        elif (level == 2):
            self.setMdmSyncMode(SYNCM_15_of_16)
        elif (level == 1):
            self.setMdmSyncMode(SYNCM_CARRIER)
        else:
            self.setMdmSyncMode(SYNCM_NONE)


    def lowballRestore(self):
        if not hasattr(self, '_last_radiocfg'):
            raise Exception("lowballRestore requires that lowball have been executed first (it saves radio config state!)")
        self.setRadioConfig(self._last_radiocfg)
        self._last_radiocfg = b''
