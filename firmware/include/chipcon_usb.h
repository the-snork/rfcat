#ifndef CHIPCON_USB_H
#define CHIPCON_USB_H

#include "global.h"
#include "chipcon_usbdebug.h"

#define EP0_MAX_PACKET_SIZE 32
#define EP5_MAX_PACKET_SIZE 64
#define EP5OUT_MAX_PACKET_SIZE 64
#define EP5IN_MAX_PACKET_SIZE 64
// EP5OUT_BUFFER_SIZE must match rflib/chipcon_usb.py definition
#define EP5OUT_BUFFER_SIZE 516 // data buffer size + 4

#define EP_STATE_IDLE 0
#define EP_STATE_TX 1
#define EP_STATE_RX 2
#define EP_STATE_STALL 3

#define EP_INBUF_WRITTEN 1
#define EP_OUTBUF_WRITTEN 2
#define EP_OUTBUF_CONTINUED 4

#define USB_STATE_UNCONFIGURED 0
#define USB_STATE_IDLE 1
#define USB_STATE_SUSPEND 2
#define USB_STATE_RESUME 3
#define USB_STATE_RESET 4
#define USB_STATE_WAIT_ADDR 5
#define USB_STATE_BLINK 0xff

#define USBADDR_UPDATE 0x80 // r

#define USB_SERIAL_STRIDX_BYTE 0x03

// Power/Control Register
#define USBPOW_SUSPEND_EN 0x01   // rw
#define USBPOW_SUSPEND 0x02      // r
#define USBPOW_RESUME 0x04       // rw
#define USBPOW_RST 0x08          // r
#define USBPOW_ISO_WAIT_SOF 0x80 // rw

// IN Endpoints and EP0 Interrupt Flags
#define USBIIF_EP0IF 0x01   // r h0
#define USBIIF_INEP1IF 0x02 // r h0
#define USBIIF_INEP2IF 0x04 // r h0
#define USBIIF_INEP3IF 0x08 // r h0
#define USBIIF_INEP4IF 0x10 // r h0
#define USBIIF_INEP5IF 0x20 // r h0

// OUT Endpoints Interrupt Flags
#define USBOIF_OUTEP1IF 0x02 // r h0
#define USBOIF_OUTEP2IF 0x04 // r h0
#define USBOIF_OUTEP3IF 0x08 // r h0
#define USBOIF_OUTEP4IF 0x10 // r h0
#define USBOIF_OUTEP5IF 0x20 // r h0

// USBCIF is all Read/Only
// Common USB Interrupt Flags
#define USBCIF_SUSPENDIF 0x01 // r h0
#define USBCIF_RESUMEIF 0x02  // r h0
#define USBCIF_RSTIF 0x04     // r h0
#define USBCIF_SOFIF 0x08     // r h0

// IN Endpoints and EP0 Interrupt Enable Mask
#define USBIIE_EP0IE 0x01   // rw
#define USBIIE_INEP1IE 0x02 // rw
#define USBIIE_INEP2IE 0x04 // rw
#define USBIIE_INEP3IE 0x08 // rw
#define USBIIE_INEP4IE 0x10 // rw
#define USBIIE_INEP5IE 0x20 // rw

// OUT Endpoint Interrupt Enable Mask
#define USBOIE_EP1IE 0x02 // rw
#define USBOIE_EP2IE 0x04 // rw
#define USBOIE_EP3IE 0x08 // rw
#define USBOIE_EP4IE 0x10 // rw
#define USBOIE_EP5IE 0x20 // rw

// Common USB Interrupt Enable Mask
#define USBCIE_SUSPENDIE 0x01 // rw
#define USBCIE_RESUMEIE 0x02  // rw
#define USBCIE_RSTIE 0x04     // rw
#define USBCIE_SOFIE 0x08     // rw

// EP0 Control and Status
#define USBCS0_OUTPKT_RDY 0x01     // r
#define USBCS0_INPKT_RDY 0x02      // rw h0
#define USBCS0_SENT_STALL 0x04     // rw h1
#define USBCS0_DATA_END 0x08       // rw h0
#define USBCS0_SETUP_END 0x10      // r
#define USBCS0_SEND_STALL 0x20     // rw h0
#define USBCS0_CLR_OUTPKT_RDY 0x40 // rw h0
#define USBCS0_CLR_SETUP_END 0x80  // rw h0

// IN EP 1-5 Control and STatus (low)
#define USBCSIL_INPKT_RDY 0x01    // rw h0
#define USBCSIL_PKT_PRESENT 0x02  // r
#define USBCSIL_UNDERRUN 0x04     // rw
#define USBCSIL_FLUSH_PACKET 0x08 // rw h0
#define USBCSIL_SEND_STALL 0x10   // rw
#define USBCSIL_SENT_STALL 0x20   // rw
#define USBCSIL_CLR_DATA_TOG 0x40 // rw h0

// IN EP 1-5 Control and STatus (high)
#define USBCSIH_IN_DBL_BUF 0x01     // rw
#define USBCSIH_FORCE_DATA_TOG 0x08 // rw
#define USBCSIH_ISO 0x40            // rw
#define USBCSIH_AUTOSET 0x80        // rw

// OUT EP 1-5 Control and Status (low)
#define USBCSOL_OUTPKT_RDY 0x01   // rw
#define USBCSOL_FIFO_FULL 0x02    // r
#define USBCSOL_OVERRUN 0x04      // rw
#define USBCSOL_DATA_ERROR 0x08   // r
#define USBCSOL_FLUSH_PACKET 0x10 // rw
#define USBCSOL_SEND_STALL 0x20   // rw
#define USBCSOL_SENT_STALL 0x40   // rw
#define USBCSOL_CLR_DATA_TOG 0x80 // rw h0

// OUT EP 1-5 Control and Status (high)
#define USBCSOH_OUT_DBL_BUF 0x01 // rw
#define USBCSOH_ISO 0x40         // rw
#define USBCSOH_AUTOCLEAR 0x80   // rw

#define P0IFG_USB_RESUME 0x80 // rw0

SBIT(USBIF, 0xE8, 0); // USB Interrupt Flag

// Request Types (bmRequestType)
#define USB_BM_REQTYPE_TGTMASK 0x1f
#define USB_BM_REQTYPE_TGT_DEV 0x00
#define USB_BM_REQTYPE_TGT_INTF 0x01
#define USB_BM_REQTYPE_TGT_EP 0x02
#define USB_BM_REQTYPE_TGT_OTHER 0x03
#define USB_BM_REQTYPE_TYPEMASK 0x60
#define USB_BM_REQTYPE_TYPE_STD 0x00
#define USB_BM_REQTYPE_TYPE_CLASS 0x20
#define USB_BM_REQTYPE_TYPE_VENDOR 0x40
#define USB_BM_REQTYPE_TYPE_RESERVED 0x60
#define USB_BM_REQTYPE_DIRMASK 0x80
#define USB_BM_REQTYPE_DIR_OUT 0x00
#define USB_BM_REQTYPE_DIR_IN 0x80
// Standard Requests (bRequest)
#define USB_GET_STATUS 0x00
#define USB_CLEAR_FEATURE 0x01
#define USB_SET_FEATURE 0x03
#define USB_SET_ADDRESS 0x05
#define USB_GET_DESCRIPTOR 0x06
#define USB_SET_DESCRIPTOR 0x07
#define USB_GET_CONFIGURATION 0x08
#define USB_SET_CONFIGURATION 0x09
#define USB_GET_INTERFACE 0x0a
#define USB_SET_INTERFACE 0x11
#define USB_SYNCH_FRAME 0x12

// Descriptor Types
#define USB_DESC_DEVICE 0x01
#define USB_DESC_CONFIG 0x02
#define USB_DESC_STRING 0x03
#define USB_DESC_INTERFACE 0x04
#define USB_DESC_ENDPOINT 0x05
#define USB_DESC_DEVICE_QUALIFIER 0x06
#define USB_DESC_OTHER_SPEED_CFG 0x07

// usb_data bits
#define USBD_CIF_SUSPEND (u16)0x1
#define USBD_CIF_RESUME (u16)0x2
#define USBD_CIF_RESET (u16)0x4
#define USBD_CIF_SOFIF (u16)0x8
#define USBD_IIF_EP0IF (u16)0x10
#define USBD_IIF_INEP1IF (u16)0x20
#define USBD_IIF_INEP2IF (u16)0x40
#define USBD_IIF_INEP3IF (u16)0x80
#define USBD_IIF_INEP4IF (u16)0x100
#define USBD_IIF_INEP5IF (u16)0x200
#define USBD_OIF_OUTEP1IF (u16)0x400
#define USBD_OIF_OUTEP2IF (u16)0x800
#define USBD_OIF_OUTEP3IF (u16)0x1000
#define USBD_OIF_OUTEP4IF (u16)0x2000
#define USBD_OIF_OUTEP5IF (u16)0x4000

#define TXDATA_MAX_WAIT 10000

// USB activities
//
#define USB_DISABLE() SLEEP &= ~SLEEP_USB_EN;
#define USB_ENABLE() SLEEP |= SLEEP_USB_EN;
#define USB_RESET() \
    USB_DISABLE();  \
    NOP();          \
    USB_ENABLE();
#define USB_INT_ENABLE() IEN2 |= 0x02;
#define USB_INT_DISABLE() IEN2 &= ~0x02;
#define USB_INT_CLEAR() \
    P2IFG = 0;          \
    P2IF = 0;

#define USB_PULLUP_ENABLE() USB_ENABLE_PIN = 1;
#define USB_PULLUP_DISABLE() USB_ENABLE_PIN = 0;

#define USB_RESUME_INT_ENABLE() P0IE = 1
#define USB_RESUME_INT_DISABLE() P0IE = 0
#define USB_RESUME_INT_CLEAR() \
    P0IFG = 0;                 \
    P0IF = 0
#define PM1() SLEEP |= 1

// formerly from cc1111usb.h
typedef struct
{
    u8 usbstatus;
    u16 event;
    u8 config;
} USB_STATE;

typedef struct
{
    u8 *INbuf;
    u16 INbytesleft;
    u8 *OUTbuf;
    volatile u16 OUTlen;
    u8 OUTapp;
    u8 OUTcmd;
    u16 OUTbytesleft;
    volatile u8 flags;
    u8 epstatus;
    __xdata u8 *dptr;
} USB_EP_IO_BUF;

typedef struct USB_Device_Desc_Type
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 bcdUSB;            // cc1111 supports USB v2.0
    uint8 bDeviceClass;       // 0 (each interface defines), 0xff (vendor-specified class code), or a valid class code
    uint8 bDeviceSubClass;    // assigned by USB org
    uint8 bDeviceProtocol;    // assigned by USB org;
    uint8 MaxPacketSize;      // for EP0, 8,16,32,64;
    uint16 idVendor;          // assigned by USB org
    uint16 idProduct;         // assigned by vendor
    uint16 bcdDevice;         // device release number
    uint8 iManufacturer;      // index of the mfg string descriptor
    uint8 iProduct;           // index of the product string descriptor
    uint8 iSerialNumber;      // index of the serial number string descriptor
    uint8 bNumConfigurations; // number of possible configs...  i wonder if the host obeys this?
} USB_Device_Desc;

typedef struct USB_Config_Desc_Type
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 wTotalLength;
    uint8 bNumInterfaces;
    uint8 bConfigurationValue;
    uint8 iConfiguration; // index of String Descriptor describing this configuration
    uint8 bmAttributes;
    uint8 bMaxPower; // 2mA increments, 0xfa;
} USB_Config_Desc;

typedef struct USB_Interface_Desc_Type
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint8 bInterfaceNumber;
    uint8 bAlternateSetting;
    uint8 bNumEndpoints;
    uint8 bInterfaceClass;
    uint8 bInterfaceSubClass;
    uint8 bInterfaceProtocol;
    uint8 iInterface;
} USB_Interface_Desc;

typedef struct USB_Endpoint_Desc_Type
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint8 bEndpointAddress;
    uint8 bmAttributes; // 0-1 Xfer Type (0;        Isoc, 2;
    uint16 wMaxPacketSize;
    uint8 bInterval; // Update interval in Frames (for isochronous, ignored for Bulk and Control)
} USB_Endpoint_Desc;

typedef struct USB_LANGID_Desc_Type
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 wLANGID0; // wLANGID[0]  0x0409;
    uint16 wLANGID1; // wLANGID[1]  0x0c09;
    uint16 wLANGID2; // wLANGID[1]  0x0407;
} USB_LANGID_Desc;

typedef struct USB_String_Desc_Type
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint16 *bString;
} USB_String_Desc;

typedef struct USB_Request_Type
{
    uint8 bmRequestType;
    uint8 bRequest;
    uint16 wValue;
    uint16 wIndex;
    uint16 wLength;
} USB_Setup_Header;

// extern global variables
extern __code u8 USBDESCBEGIN[];
extern __code u8 USBDESCWCID[];
extern USB_STATE usb_data;
extern __xdata u8 usb_ep0_OUTbuf[EP0_MAX_PACKET_SIZE]; // these get pointed to by the above structure
extern __xdata u8 usb_ep5_OUTbuf[EP5OUT_BUFFER_SIZE];  // these get pointed to by the above structure
extern __xdata USB_EP_IO_BUF ep0;
extern __xdata USB_EP_IO_BUF ep5;

extern __xdata u8 ep0req;

// provided by cc1111usb.c
void clock_init(void);
int txdata(u8 app, u8 cmd, u16 len, __xdata u8 *dataptr);
int setup_send_ep0(u8 *__xdata payload, u16 length);
int setup_sendx_ep0(__xdata u8 *__xdata payload, u16 length);
u16 usb_recv_ep0OUT(void);

u16 usb_recv_epOUT(u8 epnum, USB_EP_IO_BUF *__xdata epiobuf);
void initUSB(void);
void usb_up(void);
void usb_down(void);
void waitForUSBsetup(void);
// export as this *must* be in main loop.
void usbProcessEvents(void);

void registerCb_ep0Vendor(int (*callback)(USB_Setup_Header *pReq));
void registerCb_ep5(int (*callback)(void));

void appReturn(__xdata u8 len, __xdata u8 *__xdata response);

#define CMD_PEEK 0x80
#define CMD_POKE 0x81
#define CMD_PING 0x82
#define CMD_BUILDTYPE 0x86
#define CMD_BOOTLOADER 0x87
#define CMD_RFMODE 0x88
#define CMD_COMPILER 0x89
#define CMD_PARTNUM 0x8e
#define CMD_RESET 0x8f
#define CMD_DEVICE_SERIAL_NUMBER 0x91

#define EP0_CMD_WCID 0xfd

#define DEBUG_CMD_STRING 0xf0
#define DEBUG_CMD_HEX 0xf1
#define DEBUG_CMD_HEX16 0xf2
#define DEBUG_CMD_HEX32 0xf3

#endif
