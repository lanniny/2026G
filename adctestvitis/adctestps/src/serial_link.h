#ifndef SERIAL_LINK_H
#define SERIAL_LINK_H

#include "xstatus.h"

#define SERIAL_LINK_BAUD_RATE 115200U

/*
 * Configure the PS UART connected to the board USB-UART connector as
 * 115200 baud, 8 data bits, no parity, one stop bit.
 */
int serial_link_init(void);

#endif
