#ifndef CC1111USBDEBUG_H
#define CC1111USBDEBUG_H

#include "global.h"

void debug(__code u8* __xdata  text);
void debugx(__xdata u8* __xdata  text);
void debughex(__xdata u8 num);
void debughex16(__xdata u16 num);
void debughex32(__xdata u32 num);

#endif

