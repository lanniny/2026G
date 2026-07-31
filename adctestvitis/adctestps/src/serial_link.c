#include "serial_link.h"

#include <stddef.h>

#include "xparameters.h"
#include "xuartps.h"

static XUartPs SerialUart;

int serial_link_init(void)
{
    XUartPs_Config *config;
    XUartPsFormat format;
    int status;

#ifdef SDT
    config = XUartPs_LookupConfig(XPAR_XUARTPS_0_BASEADDR);
#else
    config = XUartPs_LookupConfig(XPAR_XUARTPS_0_DEVICE_ID);
#endif
    if (config == NULL) {
        return XST_FAILURE;
    }

    status =
        XUartPs_CfgInitialize(&SerialUart,
                              config,
                              config->BaseAddress);
    if (status != XST_SUCCESS) {
        return status;
    }

    format.BaudRate = SERIAL_LINK_BAUD_RATE;
    format.DataBits = XUARTPS_FORMAT_8_BITS;
    format.Parity = XUARTPS_FORMAT_NO_PARITY;
    format.StopBits = XUARTPS_FORMAT_1_STOP_BIT;

    status = XUartPs_SetDataFormat(&SerialUart, &format);
    if (status != XST_SUCCESS) {
        return status;
    }

    XUartPs_SetOperMode(&SerialUart, XUARTPS_OPER_MODE_NORMAL);
    return XST_SUCCESS;
}
