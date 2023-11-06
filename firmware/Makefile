# set BOOTLOADER_SIZE to match your bootloader
#
BOOTLOADER_SIZE = 0x1400
LDFLAGS_FLASH = --code-loc $(BOOTLOADER_SIZE)
USB_DEVICE_SERIAL_NUMBER="`./new_serial.py`"

CC=sdcc
RFLIB_VERSION=`../revision.sh`
CFLAGS=-Iinclude -DBUILD_VERSION=$(RFLIB_VERSION)
CFLAGSold=--no-pack-iram $(CF)
LFLAGS=--xram-loc 0xF000 

apps1111 = cc1111rf.rel global.rel cc1111_aes.rel
libsusb  = chipcon_usb.rel chipcon_usbdebug.rel chipcon_dma.rel bootloader.rel

all: 	
	make clean RfCatYS1 
	make clean RfCatYS1CCBootloader 

ys1fw: RfCatYS1


# helper targets 
chipcon_usb.rel: chipcon_usb.c include/*.h
	#./new_serial.py
	$(CC) $(CFLAGS) -DUSB_DEVICE_SERIAL_NUMBER=$(USB_DEVICE_SERIAL_NUMBER) -c chipcon_usb.c

%.rel: %.c include/*.h
	$(CC) $(CFLAGS) -c $<

# firmware build targets

## primary RfCat build targets ##
RfCatYS1: 					clean RfCatYS1.hex
RfCatYS1CCBootloader: 		clean RfCatYS1CCBootloader.hex

RfCatYS1.hex: $(libsusb) $(apps1111)
	@printf "\n\n==RfCatYS1.hex building==\n"
	sdcc $(CFLAGS) $(LFLAGS) appFHSSNIC.c $(libsusb) $(apps1111)
	packihx <appFHSSNIC.ihx >bins/RfCatYS1.hex

RfCatYS1CCBootloader.hex: appFHSSNIC.c $(libsusb) $(apps1111)
	@printf "\n\n==RfCatYS1CCBootloader.hex building==\n"
	sdcc $(CFLAGS) $(LFLAGS) $(LDFLAGS_FLASH) appFHSSNIC.c $(libsusb) $(apps1111)
	packihx <appFHSSNIC.ihx >bins/RfCatYS1CCBootloader.hex

installRfCatYS1CCBootloader: RfCatYS1CCBootloader.hex
	if [ ! -c /dev/RFCAT_BL_YS1 ] ; then ../rfcat --bootloader --force -S && sleep 1 ; fi ;
	rfcat_bootloader /dev/RFCAT_BL_YS1 erase_all
	rfcat_bootloader /dev/RFCAT_BL_YS1 download bins/RfCatYS1CCBootloader.hex
	rfcat_bootloader /dev/RFCAT_BL_YS1 verify bins/RfCatYS1CCBootloader.hex && rfcat_bootloader /dev/RFCAT_BL_YS1 run

installys1bootloadercctool: 
	./bootloader_serial.py $(USBSERNUM)
	cc-tool -n CC1111 -e -w CCBootloader/CCBootloader-rfcat-ys1-serial.hex
	cc-tool -n CC1111 -v CCBootloader/CCBootloader-rfcat-ys1-serial.hex

clean:
	@printf "\n==== CLEANING STUFF UP ====\n"
	rm -f *.ihx *.rel *.hex *.sym *.asm *.lst *.lnk *.lk *.map *.mem *.rst
