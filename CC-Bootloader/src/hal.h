/*
 * CC Bootloader - Hardware Abstraction Layer
 *
 * Fergus Noble (c) 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef _HAL_H_
#define _HAL_H_

#define LED1 P1_1
#define LED2 P1_2
#define LED3 P1_3
#define LED_MASK 0x0E
#define USB_ENABLE P1_0
#define USB_MASK 0x01
#define PIN_DC P2_2
#define TX_AMP_EN P2_0
#define RX_AMP_EN P2_4
#define AMP_BYPASS_EN P2_3

void setup_led(void);
void led_on(void);
void led_off(void);
void setup_button(void);
void setup_gpio(void);

void usb_up(void);
void usb_down(void);

#endif // _HAL_H_
