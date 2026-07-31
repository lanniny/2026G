#include <stdint.h>

#include "acquisition.h"
#include "app_config.h"
#include "app_print.h"
#include "calibration.h"
#include "display_port.h"
#include "platform.h"
#include "serial_link.h"
#include "signal_analysis.h"
#include "sleep.h"
#include "xil_printf.h"
#include "xstatus.h"

static uint16_t AdcSamples[APP_FRAME_SAMPLES]
    __attribute__((aligned(64)));
static Calibration SystemCalibration;
static MeasurementResult LatestMeasurement;

int main(void)
{
    uint32_t frame_number = 0U;
    int status;

    init_platform();

    status = serial_link_init();
    if (status != XST_SUCCESS) {
        cleanup_platform();
        return XST_FAILURE;
    }

    calibration_load_defaults(&SystemCalibration);
    signal_analysis_init();
    app_print_banner(&SystemCalibration);

    status = display_init();
    if (status != XST_SUCCESS) {
        xil_printf("WARNING: HMI UART1 is absent from the active XSA; "
                   "USB UART0 output remains enabled\r\n");
    }
    else {
        xil_printf("HMI UART1 initialized: 115200 8N1, "
                   "TX=H19 RX=H20\r\n");
        xil_printf("HMI startup commands queued: %u bytes\r\n",
                   (unsigned int)display_transmitted_bytes());
        xil_printf("HMI startup reply packets : %u\r\n",
                   (unsigned int)display_received_packets());
        if (display_received_packets() == 0U) {
            xil_printf("WARNING: no reply from HMI; verify "
                       "H19(TX)->screen RX, H20(RX)<-screen TX, "
                       "and common GND\r\n");
        }
    }

    status = acquisition_init();
    if (status != XST_SUCCESS) {
        xil_printf("ERROR: acquisition initialization failed\r\n");
        acquisition_print_diagnostics("initialization");
        cleanup_platform();
        return XST_FAILURE;
    }

    acquisition_print_diagnostics("ready");

    while (1) {
        display_service();

        status =
            acquisition_capture(AdcSamples, APP_FRAME_SAMPLES);
        if (status != XST_SUCCESS) {
            xil_printf("ERROR: frame %u capture failed; "
                       "resetting DMA/PL control\r\n",
                       (unsigned int)frame_number);
            if (acquisition_recover() != XST_SUCCESS) {
                xil_printf("ERROR: acquisition recovery failed\r\n");
            }
            usleep(APP_REFRESH_DELAY_US);
            continue;
        }

        status =
            signal_analyze(AdcSamples,
                           &SystemCalibration,
                           &LatestMeasurement);
        if (status != 0) {
            LatestMeasurement.flags |=
                MEASUREMENT_FLAG_ANALYSIS_ERROR;
        }

#if APP_UART_STREAM_RAW
        app_print_raw_frame(frame_number,
                            AdcSamples,
                            APP_FRAME_SAMPLES);
#endif
        app_print_measurement(frame_number, &LatestMeasurement);
        display_publish_measurement(&LatestMeasurement);

        frame_number++;
        usleep(APP_REFRESH_DELAY_US);
    }
}
