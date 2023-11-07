#include "cc1111rf.h"
#include "cc1111_aes.h"
#include "global.h"
#include "FHSS.h"
#include <string.h>

/* Rx buffers */
volatile __xdata u8 rfRxCurrentBuffer;
volatile __xdata u8 rfrxbuf[BUFFER_AMOUNT][BUFFER_SIZE];
volatile __xdata u16 rfRxCounter[BUFFER_AMOUNT];
volatile __xdata u8 rfRxProcessed[BUFFER_AMOUNT];
volatile __xdata u8 rfRxInfMode = 0;
volatile __xdata u16 rfRxTotalRXLen = 0;
volatile __xdata u16 rfRxLargeLen = 0;

/* Tx buffers */
// point and details about potentially multiple buffers for infinite mode transfers
//
volatile __xdata u8 *__xdata rftxbuf;
volatile __xdata u8 rfTxCurBufIdx = 0;
volatile __xdata u8 rfTxBufCount = 1; // this must be set by the tx routine to match the number of buffer blocks available

volatile __xdata u16 rfTxCounter = 0;
volatile __xdata u16 rfTxRepeatCounter = 0;
volatile __xdata u16 rfTxBufferEnd = 0;
volatile __xdata u16 rfTxRepeatLen = 0;
volatile __xdata u16 rfTxRepeatOffset = 0;
volatile __xdata u16 rfTxTotalTXLen = 0;
volatile __xdata u8 rfTxInfMode = 0;

__xdata u16 txTotal; // debugger to confirm long transmit number of bytes tx'd

// AES
volatile __xdata u8 rfAESMode = AES_CRYPTO_NONE;
// to test crypto between two dongles (KEY & IV will be all zeros directly after boot):
// volatile __xdata u8 rfAESMode = (ENCCS_MODE_CBC | AES_CRYPTO_OUT_ON | AES_CRYPTO_OUT_ENCRYPT | AES_CRYPTO_IN_ON | AES_CRYPTO_IN_DECRYPT);

// amplifier external to CC1111
volatile __xdata u8 rfAmpMode = 0;

volatile u8 rfif;
volatile __xdata u8 rf_status;
volatile __xdata u16 rf_MAC_timer;
volatile __xdata u16 rf_tLastRecv;

__xdata MAC_DATA_t macdata;

/*************************************************************************************************
 * RF helpers                                                                                    *
 ************************************************************************************************/

void setFreq(u32 freq)
{
    u32 num;

    num = freq * (0x10000 / 1000000.0) / PLATFORM_CLOCK_FREQ;
    FREQ2 = num >> 16;
    FREQ1 = (num >> 8) & 0xff;
    FREQ0 = num & 0xff;
}

void resetRFSTATE(void)
{
    // like RFOFF but without changing amplifier configuration
    RFST = RFST_SIDLE;
    while ((MARCSTATE) != MARC_STATE_IDLE)
        ;

    RFST = rf_status;
    while (rf_status != RFST_SIDLE && MARCSTATE == MARC_STATE_IDLE)
        ;
}

// enter RX mode    (this is significant!  don't do lightly or quickly!)
void RxMode(void)
{
    if (rf_status != RFST_SRX)
    {
        MCSM1 &= 0xf0;
        MCSM1 |= 0x0f;
        rf_status = RFST_SRX;

        startRX();
    }
}

// enter TX mode
void TxMode(void)
{
    if (rf_status != RFST_STX)
    {
        MCSM1 &= 0xf0;
        MCSM1 |= 0x0a;

        rf_status = RFST_STX;
        RFTX;
    }
}

// enter IDLE mode  (this is significant!  don't do lightly or quickly!)
void IdleMode(void)
{
    if (rf_status != RFST_SIDLE)
    {
        {
            MCSM1 &= 0xf0;
            RFIM &= ~RFIF_IRQ_DONE;
            RFOFF;

            S1CON &= ~(S1CON_RFIF_0 | S1CON_RFIF_1); // clear RFIF interrupts
            RFIF &= ~RFIF_IRQ_DONE;
        }
        rf_status = RFST_SIDLE;
        // FIXME: make this also adjust radio register settings for "return to" state?
    }
}

/*************************************************************************************************
 * RF init stuff                                                                                 *
 ************************************************************************************************/
void init_RF(void)
{
    // MAC variables
    rf_tLastRecv = 0;

    // PHY variables
    rfRxCounter[FIRST_BUFFER] = 0;
    rfRxCounter[SECOND_BUFFER] = 0;

    // setup TIMER 2  (MAC timer)
    // NOTE:
    // !!! any changes to TICKSPD will change the calculation of MAC timer speed !!!
    //
    // free running mode
    // time freq:
    //
    // TICKSPD = Fref (24mhz for cc1111, 26mhz for cc1110)
    //
    // ********************* ALSO IN appFHSSNIC.c: init_FHSS()  **************************
    CLKCON &= 0xc7;

    T2PR = 0;
    T2CTL |= T2CTL_TIP_64; // 64, 128, 256, 1024
    T2CTL |= T2CTL_TIG;

    // interrupt priority settings.
    // set to "01" == priority 1
    IP0 |= 0; // grp0 is RF/RFTXRX/DMA
    IP1 |= BIT0;

    // RF state
    rf_status = RFST_SIDLE;

    /* clear buffers */
    memset(rfrxbuf, 0, (BUFFER_AMOUNT * BUFFER_SIZE));

    appInitRf();

    /* Setup interrupts */
    RFTXRXIE = 1; // FIXME: should this be something that is enabled/disabled by usb?
    RFIM = 0xd1;  // TXUNF, RXOVF, DONE, SFD  (SFD to mark time of receipt)
    RFIF = 0;
    rfif = 0;
    IEN2 |= IEN2_RFIE;

    /* Put radio into idle state */
    RFOFF;
}

//***********************************************************************/
/** FIXME: how can i fail thee?  let me count the ways... and put them into the contract...
 */
/*
 * FAIL on CCA - return EFAIL_CCA
 * FAIL on wait - return EFAIL_RFST_STATE_TX
 * FAIL on args - return EFAIL_ARGS_FUKT
 */

u8 transmit(__xdata u8 *__xdata buf, __xdata u16 len, __xdata u16 repeat, __xdata u16 offset)
{
    __xdata u16 countdown;
    __xdata u8 encoffset = 0;
    __xdata u8 original_pktlen = PKTLEN;

    while (MARCSTATE == MARC_STATE_TX)
    {
        usbProcessEvents();
    }

    // Set up repeat / large blocks
    rfTxInfMode = 0;
    rfTxRepeatCounter = repeat;
    rfTxRepeatOffset = offset;
    rfTxBufferEnd = len;
    rfTxRepeatLen = len - offset;
    // calculate total bytes to be transmitted including repeat
    rfTxTotalTXLen = len + (rfTxRepeatLen * repeat);

    // If len is zero, assume first byte is the length
    // if we're in FIXED mode, skip the first byte
    // if we're in VARIABLE mode, make sure we copy the length byte + packet
    if (len == 0)
    {
        len = buf[0];

        switch (PKTCTRL0 & PKTCTRL0_LENGTH_CONFIG)
        {
        case PKTCTRL0_LENGTH_CONFIG_VAR:
            len++; // we need to send the length byte too...
            break;
        case PKTCTRL0_LENGTH_CONFIG_FIX:
            buf++; // skip sending the length byte
            PKTLEN = len;
            break;
        default:
            break;
        }
    }
    else
    {
        // If len is nonzero, use that as the length, and make sure the tx buffer is setup appropriately
        // if we're in FIXED mode, all is well
        // if we're in VARIABLE mode, must insert that length byte first.
        switch (PKTCTRL0 & PKTCTRL0_LENGTH_CONFIG)
        {
        case PKTCTRL0_LENGTH_CONFIG_VAR:
            // shuffle buffer up 1 byte to make room for length
            byte_shuffle(buf, len, 1);
            buf[0] = (u8)len;
            break;
        case PKTCTRL0_LENGTH_CONFIG_FIX:
            // if we're repeating we need to implement 'infinite' mode
            // see ti document 'SLAU259C' http://www.ti.com/litv/pdf/slau259c
            // note that repeat length of 0xFF means 'forever'
            if (repeat)
            {
                // PKTLEN must be correctly configured for the final blocksize after we exit infinite mode
                // ISR will trigger exit once rfTxTotalTXLen < 256
                PKTLEN = (u8)(rfTxTotalTXLen % 256);
                PKTCTRL0 &= ~PKTCTRL0_LENGTH_CONFIG;
                // only set radio into infinite mode if we need more than max
                if (rfTxTotalTXLen > RF_MAX_TX_BLOCK)
                    PKTCTRL0 |= PKTCTRL0_LENGTH_CONFIG_INF;
                // but we still need logical infinite mode either way
                rfTxInfMode = 1;
            }
            else
                PKTLEN = len;
            break;
        default:
            break;
        }
    }

    /* If DMA transfer, disable rxtx interrupt */
    RFTXRXIE = 1;

    // CRYPTO if required //
    if (rfAESMode & AES_CRYPTO_OUT_ENABLE)
    {
        if ((PKTCTRL0 & PKTCTRL0_LENGTH_CONFIG) == PKTCTRL0_LENGTH_CONFIG_VAR)
            encoffset = 1;
        // pad and set new length
        len = padAES(buf + encoffset, len);
        // do the encrypt or decrypt
        if ((rfAESMode & AES_CRYPTO_OUT_TYPE) == AES_CRYPTO_OUT_ENCRYPT)
            encAES(buf + encoffset, buf + encoffset, len, (rfAESMode & AES_CRYPTO_MODE));
        else
            decAES(buf + encoffset, buf + encoffset, len, (rfAESMode & AES_CRYPTO_MODE));
        // packet length may have changed due to padding so reset
        if (encoffset)
        {
            // if we are in CBC-MAC mode, only transmit the MAC or we will send
            // part of our plaintext (as we are encrypting in-place)!
            if ((rfAESMode & AES_CRYPTO_MODE) == ENCCS_MODE_CBCMAC)
                buf[0] = 16;
            else
                buf[0] = (u8)len;
        }
        else
        {
            if ((rfAESMode & AES_CRYPTO_MODE) == ENCCS_MODE_CBCMAC)
                PKTLEN = 16;
            else
                PKTLEN = (u8)len;
        }
    }

    // point tx buffer at userdata //
    rftxbuf = buf;

    // Reset byte pointer //
    rfTxCounter = 0;

    /* Put radio into tx state */
    SET_TX_AMP;

    RFST = RFST_STX;

    // wait until we're safely in TX mode
    countdown = 60000;
    while (MARCSTATE != MARC_STATE_TX && --countdown)
    {
        // FIXME: if we never end up in TX, why not?  seeing it in RX atm...  what's setting it there?  we can't have missed the whole tx!  we're not *that* slow!  although if other interrupts occurred?
        usbProcessEvents();
    }

    while (MARCSTATE == MARC_STATE_TX)
    {
        usbProcessEvents();
    }

    // reset PKTLEN as we may have messed with it
    PKTLEN = original_pktlen;

    return 1;
}

// prepare for RF RX
void startRX(void)
{
    RFTXRXIE = 1;

    /* Clear rx buffer */
    memset(rfrxbuf, 0, BUFFER_SIZE);

    /* Set both byte counters to zero */
    rfRxCounter[FIRST_BUFFER] = 0;
    rfRxCounter[SECOND_BUFFER] = 0;

    /*
     * Process flags, set first flag to false in order to let the ISR write bytes into the buffer,
     *  The second buffer should flag processed on initialize because it is empty.
     */
    rfRxProcessed[FIRST_BUFFER] = RX_UNPROCESSED;
    rfRxProcessed[SECOND_BUFFER] = RX_PROCESSED;

    /* Set first buffer as current buffer */
    rfRxCurrentBuffer = 0;

    S1CON &= ~(S1CON_RFIF_0 | S1CON_RFIF_1);
    RFIF &= ~RFIF_IRQ_DONE;

    RFRX;

    RFIM |= RFIF_IRQ_DONE;
}

/* End Repeater mode... */

void rfTxRxIntHandler(void) __interrupt(RFTXRX_VECTOR) // interrupt handler should transmit or receive the next byte
{
    // Clear interrupt - this must be done *BEFORE* reading RFD
    RFTXRXIF = 0;

    if (MARCSTATE == MARC_STATE_RX)
    { // Receive Byte
        // maintain infinite mode
        if (rfRxInfMode)
            if (rfRxTotalRXLen-- < 256)
                PKTCTRL0 &= ~PKTCTRL0_LENGTH_CONFIG;
        rf_status = RFST_SRX;
        rfrxbuf[rfRxCurrentBuffer][rfRxCounter[rfRxCurrentBuffer]++] = RFD;
        if (rfRxCounter[rfRxCurrentBuffer] >= BUFFER_SIZE || rfRxCounter[rfRxCurrentBuffer] == 0)
        {
            rfRxCounter[rfRxCurrentBuffer] = BUFFER_SIZE - 1;
        }
        // restart infinite mode?
        if (!rfRxTotalRXLen && rfRxInfMode)
        {
            rfRxTotalRXLen = rfRxLargeLen;
            PKTLEN = (u8)(rfRxTotalRXLen % 256);
            PKTCTRL0 &= ~PKTCTRL0_LENGTH_CONFIG;
            PKTCTRL0 |= PKTCTRL0_LENGTH_CONFIG_INF;
        }
    }

    else if (MARCSTATE == MARC_STATE_TX)
    { // Transmit Byte
        // maintain infinite mode
        if (rfTxInfMode)
        {
            // rfTxCounter will always be the index in the current buffer
            // rfTxBufferEnd will always be the length of the current buffer
            //
            //
            // DEBUGGING
            macdata.tLastHop++;
            //
            if (rfTxCounter == rfTxBufferEnd)
            {
                if (rfTxRepeatCounter)
                {
                    if (rfTxRepeatCounter != 0xff)
                        rfTxRepeatCounter--;
                    rfTxCounter = rfTxRepeatOffset;
                }
                else
                {
                    // arbitrary length packets flowing from one buffer to another
                    // first we mark the first byte of the current block
                    rftxbuf[(rfTxCurBufIdx * rfTxBufferEnd)] = BUFFER_AVAILABLE;

                    if (++rfTxCurBufIdx == rfTxBufCount)
                    {
                        rfTxCurBufIdx = 0;
                    }

                    if (rftxbuf[(rfTxCurBufIdx * rfTxBufferEnd)] == BUFFER_AVAILABLE)
                    {
                        // we should bail here, because the next buffer is empty, so we've had a usb buff fill underrun
                        macdata.mac_state = MAC_STATE_NONHOPPING;
                        resetRFSTATE();
                    }

                    // reset buffer index to the 2nd byte of next buffer (first byte = buflen)
                    rfTxCounter = 1;
                }
            }
            // radio to leave infinite mode?
            if (rfTxTotalTXLen-- == 255)
            {
                PKTCTRL0 &= ~PKTCTRL0_LENGTH_CONFIG;
            }
        }
        // maintain counter for non-infinite mode
        else
            rfTxTotalTXLen--;
        rf_status = RFST_STX;
        // rftxbuf is a pointer, not a static buffer, could be an array
        RFD = rftxbuf[(rfTxCurBufIdx * rfTxBufferEnd) + rfTxCounter++];
        txTotal++;
    }
}

void rfIntHandler(void) __interrupt(RF_VECTOR) // interrupt handler should trigger on rf events
{
    u8 encoffset = 0;
    // which events trigger this interrupt is determined by RFIM (set in init_RF())
    // note: S1CON should be cleared before handling the RFIF flags.
    S1CON &= ~(S1CON_RFIF_0 | S1CON_RFIF_1);

    // store the data from RFIF for main loop code to access and deal with.
    rfif |= RFIF;

    if (RFIF & RFIF_IRQ_SFD)
    {
        // mark the last time we received a packet.  this will be used for MAC layer decisions in
        // some protocols like FHSS
        rf_tLastRecv = T2CT | (rf_MAC_timer << 8);
        RFIF &= ~RFIF_IRQ_SFD;
    }

    // FIXME: if (RFIF & RFIF_CCA)  ...  communicate back to transmit function that we never made it into TX...
    //
    if (RFIF & (RFIF_IRQ_DONE | RFIF_IRQ_RXOVF | RFIF_IRQ_TIMEOUT))
    {
        // we want *all zee bytezen!*
        if (rf_status == RFST_STX)
        { // FIXME: if this, we have a state engine problem.  RXOVF should not be set when RFST_STX!
            rfif &= ~(RFIF_IRQ_DONE | RFIF_IRQ_RXOVF | RFIF_IRQ_TIMEOUT);
        }
        else
        {

            // FIXME: rfRxCurrentBuffer is used for both recv and sending on.... this should be separate.
            if (rfRxProcessed[!rfRxCurrentBuffer] == RX_PROCESSED)
            {
                // EXPECTED RESULT - RX complete.
                //
                /* CRYPTO if required */
                if (rfAESMode & AES_CRYPTO_IN_ENABLE)
                {
                    if ((PKTCTRL0 & PKTCTRL0_LENGTH_CONFIG) == PKTCTRL0_LENGTH_CONFIG_VAR)
                        encoffset = 1;
                    if ((rfAESMode & AES_CRYPTO_IN_TYPE) == AES_CRYPTO_IN_ENCRYPT)
                        encAES(&rfrxbuf[rfRxCurrentBuffer][encoffset], &rfrxbuf[rfRxCurrentBuffer][encoffset], rfRxCounter[rfRxCurrentBuffer] - encoffset, (rfAESMode & AES_CRYPTO_MODE));
                    else
                        decAES(&rfrxbuf[rfRxCurrentBuffer][encoffset], &rfrxbuf[rfRxCurrentBuffer][encoffset], rfRxCounter[rfRxCurrentBuffer] - encoffset, (rfAESMode & AES_CRYPTO_MODE));
                }
                /* Clear processed buffer */
                /* Switch current buffer */
                rfRxCurrentBuffer ^= 1;
                rfRxCounter[rfRxCurrentBuffer] = 0;
                /* Set both buffers to unprocessed */
                rfRxProcessed[FIRST_BUFFER] = RX_UNPROCESSED;
                rfRxProcessed[SECOND_BUFFER] = RX_UNPROCESSED;
            }
            else
            {
                // contingency - Packet Not Handled!
                /* Main app didn't process previous packet yet, drop this one */
                rfRxCounter[rfRxCurrentBuffer] = 0;
            }
        }
        RFIF &= ~(RFIF_IRQ_DONE | RFIF_IRQ_TIMEOUT); // OVF needs to be handled next...
    }

    // contingency - RX Overflow
    if (RFIF & RFIF_IRQ_RXOVF)
    {
        //  RX overflow, only way to get out of this is to restart receiver //
        resetRFSTATE();
        RFIF &= ~RFIF_IRQ_RXOVF;
    }
    // contingency - TX Underflow
    if (RFIF & RFIF_IRQ_TXUNF)
    {
        // Put radio into idle state //
        resetRFSTATE();
        RFIF &= ~RFIF_IRQ_TXUNF;
    }
}

// move data within a buffer
void byte_shuffle(__xdata u8 *__xdata buf, __xdata u16 len, __xdata u16 offset)
{
    while (len--)
        buf[len + offset] = buf[len];
}
