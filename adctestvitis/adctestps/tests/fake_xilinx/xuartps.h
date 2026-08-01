#ifndef TEST_XUARTPS_H
#define TEST_XUARTPS_H

#include <stdint.h>

typedef struct {
    uint16_t DeviceId;
    uint32_t BaseAddress;
    uint32_t InputClockHz;
    int ModemPinsConnected;
} XUartPs_Config;

typedef struct {
    XUartPs_Config Config;
} XUartPs;

typedef struct {
    uint32_t BaudRate;
    uint32_t DataBits;
    uint32_t Parity;
    uint32_t StopBits;
} XUartPsFormat;

#define XUARTPS_FORMAT_8_BITS    8U
#define XUARTPS_FORMAT_NO_PARITY 0U
#define XUARTPS_FORMAT_1_STOP_BIT 1U
#define XUARTPS_OPER_MODE_NORMAL 0U

XUartPs_Config *XUartPs_LookupConfig(uint16_t device_id);
int XUartPs_CfgInitialize(XUartPs *instance,
                          XUartPs_Config *config,
                          uint32_t effective_address);
int XUartPs_SetDataFormat(XUartPs *instance, XUartPsFormat *format);
void XUartPs_SetOperMode(XUartPs *instance, uint8_t mode);
void XUartPs_SetFifoThreshold(XUartPs *instance, uint8_t threshold);
void XUartPs_SetRecvTimeout(XUartPs *instance, uint8_t timeout);
int XUartPs_SetBaudRate(XUartPs *instance, uint32_t baud_rate);

#endif
