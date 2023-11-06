#
# CC Debugger
# Fergus Noble (c) 2011
#

CC = sdcc
AS = sdas8051

CFLAGS = --model-small --opt-code-size

LDFLAGS_FLASH = \
	--out-fmt-ihx \
	--code-loc 0x0000 --code-size 0x1400 \
	--xram-loc 0xf000 --xram-size 0x300 \
	--iram-size 0x100

ASFLAGS = -plosgff

ifdef DEBUG
CFLAGS += --debug
endif


SRC = \
	src/main.c \
	src/usb.c \
	src/flash.c \
	src/intel_hex.c \
	src/hal.c \
	src/usb_descriptors.c 

ASM_SRC = src/start.asm

REL = $(SRC:.c=.c.rel)

ASM_ADB = $(ASM_SRC:.asm=.adb)
ASM_LNK = $(ASM_SRC:.asm=.lnk)
ASM_LST = $(ASM_SRC:.asm=.lst)
ASM_REL = $(ASM_SRC:.asm=.rel)
ASM_RST = $(ASM_SRC:.asm=.rst)
ASM_SYM = $(ASM_SRC:.asm=.sym)

%.c.rel : %.c
	$(CC) -c $(CFLAGS) -o$*.c.rel $<

%.rel : %.asm
	$(AS) -c $(ASFLAGS) $<

all: CCBootloader-rfcat-ys1.hex

CCBootloader-rfcat-ys1.hex: $(REL) $(ASM_REL)
	$(CC) $(LDFLAGS_FLASH) $(CFLAGS) -o CCBootloader-rfcat-ys1.hex $(ASM_REL) $(REL)

clean:
	rm -f src/*.c.*
	rm -f $(ASM_ADB) $(ASM_LNK) $(ASM_LST) $(ASM_REL) $(ASM_RST) $(ASM_SYM)
	rm -f *.cdb *.lnk *.lk *.map *.mem

