#include "app_print.h"

#include <math.h>

#include "app_config.h"
#include "xil_printf.h"

#define RAD_TO_DEG_F 57.2957795130823208768f

static uint32_t decimal_divisor(uint32_t decimals)
{
    uint32_t divisor = 1U;

    while (decimals != 0U) {
        divisor *= 10U;
        decimals--;
    }
    return divisor;
}

static void print_scaled(int32_t scaled_value, uint32_t decimals)
{
    uint32_t divisor = decimal_divisor(decimals);
    uint32_t magnitude;
    uint32_t fraction;
    uint32_t place;

    if (scaled_value < 0) {
        outbyte('-');
        magnitude = (uint32_t)(-(int64_t)scaled_value);
    }
    else {
        magnitude = (uint32_t)scaled_value;
    }

    xil_printf("%u", (unsigned int)(magnitude / divisor));
    if (decimals == 0U) {
        return;
    }

    outbyte('.');
    fraction = magnitude % divisor;
    place = divisor / 10U;
    while (place != 0U) {
        outbyte((char)('0' + ((fraction / place) % 10U)));
        place /= 10U;
    }
}

static int32_t rounded_i32(float value)
{
    if (value >= 0.0f) {
        return (int32_t)(value + 0.5f);
    }
    return (int32_t)(value - 0.5f);
}

static void print_flags(uint32_t flags)
{
    uint32_t printed = 0U;

    if (flags == 0U) {
        xil_printf("OK");
        return;
    }

    if ((flags & MEASUREMENT_FLAG_NO_SIGNAL) != 0U) {
        xil_printf("NO SIGNAL");
        printed = 1U;
    }
    if ((flags & MEASUREMENT_FLAG_CLIPPED) != 0U) {
        xil_printf("%sCLIPPED", printed != 0U ? ", " : "");
        printed = 1U;
    }
    if ((flags & MEASUREMENT_FLAG_LOW_SNR) != 0U) {
        xil_printf("%sLOW SNR", printed != 0U ? ", " : "");
        printed = 1U;
    }
    if ((flags & MEASUREMENT_FLAG_UNCALIBRATED) != 0U) {
        xil_printf("%sUNCALIBRATED", printed != 0U ? ", " : "");
        printed = 1U;
    }
    if ((flags & MEASUREMENT_FLAG_ANALYSIS_ERROR) != 0U) {
        xil_printf("%sANALYSIS ERROR", printed != 0U ? ", " : "");
    }
}

void app_print_banner(const Calibration *calibration)
{
    xil_printf("\r\n"
               "====================================================\r\n");
    xil_printf(" ZYNQ + LTC2208 PERIODIC SIGNAL ANALYZER\r\n");
    xil_printf(" USB debug UART: UART0, 115200 baud, 8N1\r\n");
    xil_printf(" HMI UART      : UART1 EMIO, 115200 baud, 8N1\r\n");
    xil_printf("                 TX=H19, RX=H20 (PL BANK35 IO)\r\n");
    xil_printf(" Sample rate   : 4.000 MHz\r\n");
    xil_printf(" Frame length  : 8192 samples\r\n");
    xil_printf(" FFT resolution: 488.281 Hz/bin\r\n");
    xil_printf(" Analysis range: 10.000 kHz to 500.000 kHz\r\n");
    xil_printf(" ADC channel   : INA / Channel A\r\n");

    if (calibration->is_calibrated == 0U) {
        xil_printf(" Calibration   : DEFAULT VALUES (not calibrated)\r\n");
        xil_printf("                 Voltage results are estimates\r\n");
    }
    else {
        xil_printf(" Calibration   : CALIBRATED\r\n");
    }
    xil_printf("====================================================\r\n");
    xil_printf(" Analyzer ready.\r\n");
}

void app_print_measurement(uint32_t frame_number,
                           const MeasurementResult *result)
{
    uint32_t component_limit = result->component_count;
    uint32_t i;

    xil_printf("\r\n"
               "================ MEASUREMENT #%u ================\r\n",
               (unsigned int)frame_number);

    xil_printf(" Status                 : ");
    print_flags(result->flags);
    xil_printf("  [flags=0x%08x]\r\n",
               (unsigned int)result->flags);

    xil_printf("\r\n ADC acquisition\r\n");
    xil_printf("   Minimum ADC code     : %u\r\n",
               (unsigned int)result->minimum_code);
    xil_printf("   Maximum ADC code     : %u\r\n",
               (unsigned int)result->maximum_code);
    xil_printf("   Clipped samples      : %u / %u\r\n",
               (unsigned int)result->clipped_sample_count,
               (unsigned int)APP_FRAME_SAMPLES);

    if ((result->flags &
         (MEASUREMENT_FLAG_NO_SIGNAL |
          MEASUREMENT_FLAG_ANALYSIS_ERROR)) != 0U) {
        xil_printf("\r\n Signal parameters\r\n");
        xil_printf("   No valid signal parameters are available.\r\n");
        if ((result->flags & MEASUREMENT_FLAG_NO_SIGNAL) != 0U) {
            xil_printf("   Reason: no measurable AC signal was detected.\r\n");
        }
        if ((result->flags & MEASUREMENT_FLAG_ANALYSIS_ERROR) != 0U) {
            xil_printf("   Reason: signal analysis failed.\r\n");
        }
        if ((result->flags & MEASUREMENT_FLAG_CLIPPED) != 0U) {
            xil_printf("   Warning: ADC input is clipped/saturated.\r\n");
        }
        xil_printf("================ END MEASUREMENT =================\r\n");
        return;
    }

    xil_printf("\r\n Main signal parameters\r\n");
    xil_printf("   Fundamental frequency: ");
    print_scaled(rounded_i32(result->fundamental_hz), 3U);
    xil_printf(" kHz\r\n");

    xil_printf("   Fundamental period   : ");
    print_scaled(
        rounded_i32((1000000.0f / result->fundamental_hz) * 1000.0f),
        3U);
    xil_printf(" us\r\n");

    xil_printf("   Peak-to-peak voltage : ");
    print_scaled(rounded_i32(result->peak_to_peak_volts *
                            1000000.0f),
                 3U);
    xil_printf(" mV\r\n");

    xil_printf("   AC RMS voltage       : ");
    print_scaled(rounded_i32(result->rms_ac_volts *
                            1000000.0f),
                 3U);
    xil_printf(" mV\r\n");

    xil_printf("   Total RMS voltage    : ");
    print_scaled(rounded_i32(result->rms_total_volts *
                            1000000.0f),
                 3U);
    xil_printf(" mV\r\n");

    xil_printf("   DC component         : ");
    print_scaled(rounded_i32(result->dc_volts * 1000000.0f), 3U);
    xil_printf(" mV\r\n");

    xil_printf("   Estimated SNR        : ");
    print_scaled(rounded_i32(result->snr_db * 100.0f), 2U);
    xil_printf(" dB\r\n");

    xil_printf("\r\n Raw frame values\r\n");
    xil_printf("   Raw peak-to-peak     : ");
    print_scaled(rounded_i32(result->raw_peak_to_peak_volts *
                            1000000.0f),
                 3U);
    xil_printf(" mV\r\n");

    xil_printf("   Raw AC RMS           : ");
    print_scaled(rounded_i32(result->raw_rms_ac_volts *
                            1000000.0f),
                 3U);
    xil_printf(" mV\r\n");

    if (component_limit > APP_UART_PRINT_COMPONENT_LIMIT) {
        component_limit = APP_UART_PRINT_COMPONENT_LIMIT;
    }

    xil_printf("\r\n Spectral components (%u)\r\n",
               (unsigned int)component_limit);
    xil_printf("   Each component: frequency, peak, RMS, phase\r\n");

    for (i = 0U; i < component_limit; i++) {
        const SignalComponent *component = &result->components[i];

        xil_printf("   H%u: f=",
                   (unsigned int)component->harmonic);
        print_scaled(rounded_i32(component->frequency_hz), 3U);
        xil_printf(" kHz, peak=");
        print_scaled(
            rounded_i32(component->amplitude_peak_v * 1000000.0f),
            3U);
        xil_printf(" mV, RMS=");
        print_scaled(
            rounded_i32(component->amplitude_peak_v *
                        0.7071067811865475f *
                        1000000.0f),
            3U);
        xil_printf(" mV, phase=");
        print_scaled(
            rounded_i32(component->phase_rad *
                        RAD_TO_DEG_F *
                        1000.0f),
            3U);
        xil_printf(" deg\r\n");
    }

    xil_printf("================ END MEASUREMENT =================\r\n");
}

void app_print_raw_frame(uint32_t frame_number,
                         const uint16_t *samples,
                         uint32_t sample_count)
{
    uint32_t i;

    xil_printf("\r\nADC_FRAME iteration=%u samples=%u bytes=%u "
               "sample_rate=4000000 order=LTC2208_LE_U16\r\n",
               (unsigned int)frame_number,
               (unsigned int)sample_count,
               (unsigned int)(sample_count * sizeof(uint16_t)));

    for (i = 0U; i < sample_count; i++) {
        outbyte((char)(samples[i] & 0xffU));
        outbyte((char)((samples[i] >> 8U) & 0xffU));
    }

    xil_printf("ADC_END iteration=%u\r\n",
               (unsigned int)frame_number);
}
