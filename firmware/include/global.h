#ifndef GLOBAL_H
#define GLOBAL_H

#include "types.h"

#include "cc1111.h"

#include "bits.h"

extern __xdata u32 clock;

#define LC_DEVICE_SERIAL_NUMBER 0x13f0

#define LCE_NO_ERROR 0x0
#define LCE_USB_EP5_LEN_TOO_BIG 0x6

// Return Codes
#define RC_NO_ERROR 0x0
#define RC_TX_DROPPED_PACKET 0xec
#define RC_TX_ERROR 0xed
#define RC_RF_BLOCKSIZE_INCOMPAT 0xee
#define RC_RF_MODE_INCOMPAT 0xef
#define RC_ERR_BUFFER_NOT_AVAILABLE 0xfe
#define RC_ERR_BUFFER_SIZE_EXCEEDED 0xff

// USB activities
#define USB_ENABLE_PIN P1_0

// NOP macro which may be defined elsewhere
#ifndef NOP
#define NOP() \
    __asm;    \
    nop;      \
    __endasm;
#endif

// USB data buffer
#define BUFFER_AVAILABLE 0x00
#define BUFFER_FILLING 0xff
#define ERR_BUFFER_SIZE_EXCEEDED -1
#define ERR_BUFFER_NOT_AVAILABLE -2

// Checks
#define IS_XOSC_STABLE() (SLEEP & SLEEP_XOSC_S)

// AES
// defines for specifying desired crypto operations.
// AES_CRYPTO is in two halves:
//    upper 4 bits mirror CC1111 mode (ENCCS_MODE_CBC etc.)
//    lower 4 bits are switches
// AES_CRYPTO[7:4]     ENCCS_MODE...
// AES_CRYPTO[3]       OUTBOUND 0 == OFF, 1 == ON
// AES_CRYPTO[2]       OUTBOUND 0 == Decrypt, 1 == Encrypt
// AES_CRYPTO[1]       INBOUND  0 == OFF, 1 == ON
// AES_CRYPTO[0]       INBOUND  0 == Decrypt, 1 == Encrypt
// bitfields
#define AES_CRYPTO_MODE 0xF0
#define AES_CRYPTO_OUT 0x0C
#define AES_CRYPTO_OUT_ENABLE 0x08
#define AES_CRYPTO_OUT_ON (0x01 << 3)
#define AES_CRYPTO_OUT_OFF (0x00 << 3)
#define AES_CRYPTO_OUT_TYPE 0x04
#define AES_CRYPTO_OUT_DECRYPT (0x00 << 2)
#define AES_CRYPTO_OUT_ENCRYPT (0x01 << 2)
#define AES_CRYPTO_IN 0x03
#define AES_CRYPTO_IN_ENABLE 0x02
#define AES_CRYPTO_IN_ON (0x01 << 1)
#define AES_CRYPTO_IN_OFF (0x00 << 1)
#define AES_CRYPTO_IN_TYPE 0x01
#define AES_CRYPTO_IN_DECRYPT (0x00 << 0)
#define AES_CRYPTO_IN_ENCRYPT (0x01 << 0)
#define AES_CRYPTO_NONE 0x00
// flags
#define AES_DISABLE 0x00
#define AES_ENABLE 0x01
#define AES_DECRYPT 0x00
#define AES_ENCRYPT 0x01

#define SLEEPTIMER 1100
#define PLATFORM_CLOCK_FREQ 24
void usbIntHandler(void) __interrupt(P2INT_VECTOR);
void p0IntHandler(void) __interrupt(P0INT_VECTOR);

#define TX_AMP_EN P2_0
#define RX_AMP_EN P2_4
#define AMP_BYPASS_EN P2_3

#define LE_WORD(x) ((x)&0xFF), ((u8)(((u16)(x)) >> 8))

/* function declarations */
void sleepMillis(int ms);
void t1IntHandler(void) __interrupt(T1_VECTOR); // interrupt handler should trigger on T1 overflow
void clock_init(void);
void io_init(void);
#endif
