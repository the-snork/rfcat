# firmware errors
LCE_NO_ERROR                            = 0x00
LCE_USB_EP5_LEN_TOO_BIG                 = 0x06

RC_NO_ERROR                             = 0x00
RC_TX_DROPPED_PACKET                    = 0xec
RC_TX_ERROR                             = 0xed
RC_RF_BLOCKSIZE_INCOMPAT                = 0xee
RC_RF_MODE_INCOMPAT                     = 0xef
RC_TEMP_ERR_BUFFER_NOT_AVAILABLE        = 0xfe # temporary error - retry!
RC_ERR_BUFFER_SIZE_EXCEEDED             = 0xff
RC_FAIL_TRANSMIT_LONG                   = 0xffff

# python client only errors
PY_NO_ERROR                             = 0x00
PY_TX_BLOCKSIZE_INCOMPAT                = 0xd0 # block > 255 bytes but with with repeat or offset specified
PY_TX_BLOCKSIZE_TOO_LARGE               = 0xda # block > 65535 (we could make this bigger by using a u32 for size)
