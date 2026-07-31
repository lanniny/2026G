#include "acquisition.h"

#include <string.h>

#include "app_config.h"
#include "xaxidma.h"
#include "xaxidma_hw.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "xparameters.h"

#define DMA_BASE_ADDR                  XPAR_XAXIDMA_0_BASEADDR
#define GPIO_BASE_ADDR                 XPAR_AXI_GPIO_AD9226_CTRL_BASEADDR

#define GPIO_DATA_OFFSET               0x00U
#define GPIO_TRI_OFFSET                0x04U
#define GPIO2_DATA_OFFSET              0x08U
#define GPIO2_TRI_OFFSET               0x0cU

#define PL_START_MASK                  0x00000001U
#define PL_STATUS_IDLE_MASK            0x01U
#define PL_STATUS_BUSY_MASK            0x02U
#define PL_STATUS_DONE_MASK            0x04U
#define PL_STATUS_AXIS_READY_MASK      0x08U
#define PL_STATUS_OFA_MASK             0x40U
#define PL_STATUS_CKO_SEEN_MASK        0x80U

#define DMA_TIMEOUT_COUNT              10000000U
#define PL_TIMEOUT_COUNT               10000000U
#define DMA_START_TIMEOUT_COUNT        100000U
#define DMA_FRAME_BYTES                (APP_FRAME_SAMPLES * sizeof(uint32_t))

static XAxiDma AxiDma;
static uint32_t DmaRxBuffer[APP_FRAME_SAMPLES]
    __attribute__((aligned(64)));

static void short_delay(void)
{
    volatile uint32_t i;

    for (i = 0U; i < 10000U; i++) {
    }
}

uint32_t acquisition_dma_status(void)
{
    return Xil_In32(DMA_BASE_ADDR +
                    XAXIDMA_RX_OFFSET +
                    XAXIDMA_SR_OFFSET);
}

uint32_t acquisition_pl_status(void)
{
    return Xil_In32(GPIO_BASE_ADDR + GPIO2_DATA_OFFSET) & 0xffU;
}

void acquisition_print_diagnostics(const char *prefix)
{
    uint32_t dma_status = acquisition_dma_status();
    uint32_t dma_control =
        Xil_In32(DMA_BASE_ADDR +
                 XAXIDMA_RX_OFFSET +
                 XAXIDMA_CR_OFFSET);
    uint32_t pl_status = acquisition_pl_status();

    xil_printf("%s DMA_SR=0x%08x DMA_CR=0x%08x PL=0x%02x "
               "idle=%u busy=%u done=%u tready=%u ofa=%u cko=%u\r\n",
               prefix,
               (unsigned int)dma_status,
               (unsigned int)dma_control,
               (unsigned int)pl_status,
               (unsigned int)((pl_status & PL_STATUS_IDLE_MASK) != 0U),
               (unsigned int)((pl_status & PL_STATUS_BUSY_MASK) != 0U),
               (unsigned int)((pl_status & PL_STATUS_DONE_MASK) != 0U),
               (unsigned int)((pl_status &
                               PL_STATUS_AXIS_READY_MASK) != 0U),
               (unsigned int)((pl_status & PL_STATUS_OFA_MASK) != 0U),
               (unsigned int)((pl_status &
                               PL_STATUS_CKO_SEEN_MASK) != 0U));
}

static void gpio_init(void)
{
    Xil_Out32(GPIO_BASE_ADDR + GPIO_TRI_OFFSET, 0x00000000U);
    Xil_Out32(GPIO_BASE_ADDR + GPIO2_TRI_OFFSET, 0xffffffffU);
    Xil_Out32(GPIO_BASE_ADDR + GPIO_DATA_OFFSET, 0x00000000U);
    short_delay();
}

static int wait_for_pl_mask(uint32_t mask)
{
    uint32_t timeout = PL_TIMEOUT_COUNT;

    while ((acquisition_pl_status() & mask) == 0U) {
        if (timeout == 0U) {
            return XST_FAILURE;
        }
        timeout--;
    }

    return XST_SUCCESS;
}

static void pulse_capture_start(void)
{
    Xil_Out32(GPIO_BASE_ADDR + GPIO_DATA_OFFSET, 0x00000000U);
    short_delay();
    Xil_Out32(GPIO_BASE_ADDR + GPIO_DATA_OFFSET, PL_START_MASK);
    short_delay();
    Xil_Out32(GPIO_BASE_ADDR + GPIO_DATA_OFFSET, 0x00000000U);
}

static int dma_reset(void)
{
    uint32_t timeout = DMA_TIMEOUT_COUNT;

    XAxiDma_Reset(&AxiDma);
    while (!XAxiDma_ResetIsDone(&AxiDma)) {
        if (timeout == 0U) {
            return XST_FAILURE;
        }
        timeout--;
    }

    XAxiDma_IntrDisable(&AxiDma,
                        XAXIDMA_IRQ_ALL_MASK,
                        XAXIDMA_DEVICE_TO_DMA);
    XAxiDma_IntrAckIrq(&AxiDma,
                       XAXIDMA_IRQ_ALL_MASK,
                       XAXIDMA_DEVICE_TO_DMA);
    return XST_SUCCESS;
}

int acquisition_init(void)
{
    XAxiDma_Config *config;
    int status;

    gpio_init();

    config = XAxiDma_LookupConfig(DMA_BASE_ADDR);
    if (config == NULL) {
        return XST_FAILURE;
    }

    status = XAxiDma_CfgInitialize(&AxiDma, config);
    if (status != XST_SUCCESS) {
        return status;
    }

    if (XAxiDma_HasSg(&AxiDma)) {
        return XST_FAILURE;
    }

    return dma_reset();
}

int acquisition_recover(void)
{
    int status;

    /*
     * A pulse while PL is busy is defined as an abort request.  Abort before
     * resetting DMA, otherwise a pending AXIS beat can keep the PL in its
     * STREAM_SEND state forever.
     */
    if ((acquisition_pl_status() & PL_STATUS_IDLE_MASK) == 0U) {
        pulse_capture_start();
        status = wait_for_pl_mask(PL_STATUS_IDLE_MASK);
        if (status != XST_SUCCESS) {
            acquisition_print_diagnostics("PL abort timeout");
            return status;
        }
    }

    gpio_init();
    return dma_reset();
}

int acquisition_capture(uint16_t *samples, uint32_t sample_count)
{
    uint32_t timeout;
    uint32_t i;
    int status;

    if (samples == NULL || sample_count != APP_FRAME_SAMPLES) {
        return XST_INVALID_PARAM;
    }

    status = wait_for_pl_mask(PL_STATUS_IDLE_MASK);
    if (status != XST_SUCCESS) {
        acquisition_print_diagnostics("PL idle timeout");
        return status;
    }

    if ((acquisition_pl_status() & PL_STATUS_CKO_SEEN_MASK) == 0U) {
        acquisition_print_diagnostics("LTC2208 CKO not detected");
        return XST_FAILURE;
    }

    memset(DmaRxBuffer, 0, sizeof(DmaRxBuffer));
    Xil_DCacheFlushRange((UINTPTR)DmaRxBuffer, DMA_FRAME_BYTES);
    XAxiDma_IntrAckIrq(&AxiDma,
                       XAXIDMA_IRQ_ALL_MASK,
                       XAXIDMA_DEVICE_TO_DMA);

    status = XAxiDma_SimpleTransfer(&AxiDma,
                                    (UINTPTR)DmaRxBuffer,
                                    DMA_FRAME_BYTES,
                                    XAXIDMA_DEVICE_TO_DMA);
    if (status != XST_SUCCESS) {
        acquisition_print_diagnostics("DMA start failed");
        return status;
    }

    /*
     * Do not trigger PL until S2MM has actually left Halted state.  This also
     * gives a precise diagnostic if the DMA run/stop write is being ignored.
     */
    timeout = DMA_START_TIMEOUT_COUNT;
    while ((acquisition_dma_status() & XAXIDMA_HALTED_MASK) != 0U) {
        if (timeout == 0U) {
            acquisition_print_diagnostics("DMA did not start");
            return XST_FAILURE;
        }
        timeout--;
    }

    pulse_capture_start();

    timeout = DMA_TIMEOUT_COUNT;
    while (XAxiDma_Busy(&AxiDma, XAXIDMA_DEVICE_TO_DMA)) {
        uint32_t dma_status = acquisition_dma_status();

        if ((dma_status & XAXIDMA_ERR_ALL_MASK) != 0U) {
            acquisition_print_diagnostics("DMA transfer error");
            return XST_FAILURE;
        }

        if (timeout == 0U) {
            acquisition_print_diagnostics("DMA transfer timeout");
            return XST_FAILURE;
        }
        timeout--;
    }

    if (wait_for_pl_mask(PL_STATUS_DONE_MASK) != XST_SUCCESS) {
        acquisition_print_diagnostics("PL done timeout");
        return XST_FAILURE;
    }

    Xil_DCacheInvalidateRange((UINTPTR)DmaRxBuffer, DMA_FRAME_BYTES);

    for (i = 0U; i < APP_FRAME_SAMPLES; i++) {
        samples[i] =
            (uint16_t)(DmaRxBuffer[i] & APP_ADC_CODE_MASK);
    }

    return XST_SUCCESS;
}
