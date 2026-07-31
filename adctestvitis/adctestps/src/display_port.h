#ifndef DISPLAY_PORT_H
#define DISPLAY_PORT_H

#include <stdint.h>

#include "signal_analysis.h"
#include "xstatus.h"

#define DISPLAY_UART_BAUD_RATE 115200U
#define DISPLAY_UART_BOOT_BAUD_RATE 9600U

/*
 * USART-HMI display connection:
 *   PS UART1 -> EMIO -> ordinary PL I/O
 *   HMI_UART_0_txd (H19) -> display RX
 *   HMI_UART_0_rxd (H20) <- display TX
 *
 * UART0 remains dedicated to the board USB debug serial port.
 */
int display_init(void);
uint32_t display_transmitted_bytes(void);
uint32_t display_received_packets(void);
void display_service(void);
void display_publish_measurement(const MeasurementResult *result);

#endif
