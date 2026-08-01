#ifndef TEST_XUARTPS_HW_H
#define TEST_XUARTPS_HW_H

#include <stdint.h>

#define XUARTPS_SR_OFFSET   0x002CU
#define XUARTPS_FIFO_OFFSET 0x0030U
#define XUARTPS_SR_TXEMPTY  0x00000008U
#define XUARTPS_SR_TXFULL   0x00000010U
#define XUARTPS_SR_TACTIVE  0x00000800U

uint32_t XUartPs_ReadReg(uint32_t base_address, uint32_t offset);
void XUartPs_WriteReg(uint32_t base_address,
                      uint32_t offset,
                      uint32_t value);
int XUartPs_IsReceiveData(uint32_t base_address);

#endif
