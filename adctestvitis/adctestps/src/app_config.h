#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/*
 * Periodic-signal analyzer application configuration.
 *
 * The PL sends one 16-bit LTC2208 sample in the low 16 bits of every
 * 32-bit DMA word.
 */
#define APP_SAMPLE_RATE_HZ              4000000.0f
#define APP_FRAME_SAMPLES               8192U
#define APP_FFT_BIN_HZ                  (APP_SAMPLE_RATE_HZ / APP_FRAME_SAMPLES)

#define APP_ANALYSIS_MIN_HZ             10000.0f
#define APP_ANALYSIS_MAX_HZ             500000.0f
#define APP_FUNDAMENTAL_MAX_HZ          250000.0f
#define APP_FUNDAMENTAL_GRID_HZ         500.0f
#define APP_FREQUENCY_ERROR_LIMIT_HZ    1000.0f
#define APP_FREQUENCY_NUMERICAL_MARGIN_HZ 5.0f
#define APP_FUNDAMENTAL_BOUNDARY_TOLERANCE_HZ \
    (APP_FREQUENCY_ERROR_LIMIT_HZ + \
     APP_FREQUENCY_NUMERICAL_MARGIN_HZ)
#define APP_ANALYSIS_SEARCH_MARGIN_HZ \
    (2.0f * APP_FUNDAMENTAL_BOUNDARY_TOLERANCE_HZ)
#define APP_MAX_COMPONENTS              50U
#define APP_MAX_SIGNAL_COMPONENTS       3U
#define APP_MAX_PEAK_CANDIDATES         64U
#define APP_WAVEFORM_POINTS             1024U

#define APP_ADC_BITS                    16U
#define APP_ADC_CODE_MASK               0xffffU
#define APP_ADC_CODE_MAX                65535U
#define APP_ADC_CODE_RANGE              65535.0f
#define APP_ADC_CLIP_LOW_CODE           64U
#define APP_ADC_CLIP_HIGH_CODE          65471U

/*
 * Board-level ADC calibration.
 *
 * The LTC2208 module accepts -4.5 V to +4.5 V (9 Vpp full span) into 50 ohm.
 * Its full-rate CMOS output is offset binary with normal polarity:
 * A = D / 65535 * 9 V - 4.5 V.
 * FRONTEND_GAIN is the voltage gain ahead of the ADC module.
 */
#define APP_DEFAULT_ADC_ZERO_CODE       32767.5f
#define APP_DEFAULT_ADC_SPAN_VPP        9.0f
#define APP_DEFAULT_ADC_POLARITY        1.0f
#define APP_DEFAULT_FRONTEND_GAIN       1.0f

#define APP_NO_SIGNAL_PEAK_V            0.0015f
#define APP_CONTEST_COMPONENT_MIN_PEAK_V 0.0050f
#define APP_COMPONENT_MIN_PEAK_V        \
    (0.50f * APP_CONTEST_COMPONENT_MIN_PEAK_V)
#define APP_FUNDAMENTAL_CANDIDATE_MIN_PEAK_V \
    (0.80f * APP_CONTEST_COMPONENT_MIN_PEAK_V)
#define APP_COMPONENT_RELATIVE_LIMIT    0.0050f
#define APP_LOW_SNR_DB                  20.0f

#define APP_REFRESH_DELAY_US            100000U
#define APP_UART_PRINT_COMPONENT_LIMIT  APP_MAX_COMPONENTS

/*
 * Set to 1 only for MATLAB/raw-capture debugging.  At 115200 baud, sending
 * 16384 payload bytes consumes most of the two-second response budget.
 */
#define APP_UART_STREAM_RAW             0U

#endif
