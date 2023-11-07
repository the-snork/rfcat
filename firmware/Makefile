# set BOOTLOADER_SIZE to match your bootloader
#
BOOTLOADER_SIZE = 0x1400
LDFLAGS_FLASH = --code-loc $(BOOTLOADER_SIZE)

CC=sdcc
CFLAGS=-Iinclude 
LFLAGS=--xram-loc 0xF000 

apps1111 = cc1111rf.rel global.rel cc1111_aes.rel
libsusb  = chipcon_usb.rel chipcon_usbdebug.rel chipcon_dma.rel bootloader.rel

all: 	
	make clean RfCatYS1CCBootloader 

chipcon_usb.rel: chipcon_usb.c include/*.h
	#./new_serial.py
	$(CC) $(CFLAGS) -c chipcon_usb.c

%.rel: %.c include/*.h
	$(CC) $(CFLAGS) -c $<

RfCatYS1CCBootloader: 		clean RfCatYS1CCBootloader.hex

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
	cc-tool -n CC1111 -e -w CCBootloader/CCBootloader-rfcat-ys1.hex
	cc-tool -n CC1111 -v CCBootloader/CCBootloader-rfcat-ys1.hex

clean:
	@printf "\n==== CLEANING STUFF UP ====\n"
	rm -f *.ihx *.rel *.hex *.sym *.asm *.lst *.lnk *.lk *.map *.mem *.rst
