#!/usr/bin/env python3

from __future__ import print_function

import sys
from rflib.intelhex import IntelHex

WRITEBACK = True
if len(sys.argv) > 1:
    WRITEBACK = False
    ser = int(sys.argv[1])
else:
    try:
        serbytes = open(".serial", 'rb').read()
        ser = int(serbytes, 16) + 1
    except (IOError, ValueError):
        ser = 0

print(("[--- new serial number: %.4x ---]" % ser), file=sys.stderr)

if WRITEBACK:
    open(".serial", 'wb').write(b"%.13x" % ser)

sertxt = ''
sertmp = "%.13x" % ser
for c in sertmp:
    sertxt += "%s\x00" % c

ihy=IntelHex('CCBootloader/CCBootloader-rfcat-ys1.hex')
ihy.puts(0x13e0, "@las\x1c\x03" + sertxt)
ihy.write_hex_file('CCBootloader/CCBootloader-rfcat-ys1-serial.hex')

