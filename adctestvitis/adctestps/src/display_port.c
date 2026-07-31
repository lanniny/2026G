#include "display_port.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_config.h"
#include "sleep.h"
#include "xparameters.h"
#include "xuartps.h"
#include "xuartps_hw.h"

#define HMI_PAGE_TIME           0U
#define HMI_PAGE_SPECTRUM       1U

#define HMI_PLOT_LEFT           45
#define HMI_PLOT_TOP            110
#define HMI_PLOT_WIDTH          506
#define HMI_PLOT_HEIGHT         265
#define HMI_PLOT_BOTTOM         (HMI_PLOT_TOP + HMI_PLOT_HEIGHT - 1)
#define HMI_WAVE_SEGMENTS       48U
#define HMI_SCREEN_MAX_HZ       510000.0f

#define HMI_COLOR_BLACK         0U
#define HMI_COLOR_BLUE          31U
#define HMI_COLOR_GREEN         2016U
#define HMI_COLOR_CYAN          2047U
#define HMI_COLOR_ORANGE        64512U
#define HMI_COLOR_WHITE         65535U
#define HMI_REFRESH_TIME_FRAMES 20U
#define HMI_REFRESH_FFT_FRAMES  10U

#define HMI_UART1_BASEADDR       0xE0001000U
#define HMI_UART1_INPUT_CLOCK_HZ 100000000U
#define HMI_UART1_REF_CLOCK_ID   24U
#define HMI_UART_POLL_INTERVAL_US 10U
#define HMI_UART_FIFO_TIMEOUT_US  20000U
#define HMI_UART_DONE_TIMEOUT_US  250000U
#define HMI_POWER_ON_DELAY_US      2000000U

typedef struct {
    XUartPs uart;
    uint8_t initialized;
    uint8_t current_page;
    uint8_t period_count;
    uint8_t hold;
    uint8_t redraw_required;
    uint8_t rx_packet[16];
    uint32_t rx_length;
    uint32_t rx_ff_count;
    uint32_t refresh_divider;
    uint32_t transmitted_bytes;
    uint32_t received_packets;
} DisplayState;

static DisplayState Display;

static int display_uart_init(void)
{
#ifndef XPAR_XUARTPS_1_BASEADDR
    static XUartPs_Config fallback_config = {
#ifndef SDT
        1U,
#else
        "xlnx,xuartps",
#endif
        HMI_UART1_BASEADDR,
        HMI_UART1_INPUT_CLOCK_HZ,
        0,
#if defined(XCLOCKING) || defined(SDT)
        HMI_UART1_REF_CLOCK_ID,
#endif
#if defined(SDT)
        0U,
        0U
#endif
    };
#endif
    XUartPs_Config *config;
    XUartPsFormat format;
    int status;

#ifdef XPAR_XUARTPS_1_BASEADDR
#ifdef SDT
    config = XUartPs_LookupConfig(XPAR_XUARTPS_1_BASEADDR);
#else
    config = XUartPs_LookupConfig(XPAR_XUARTPS_1_DEVICE_ID);
#endif
#else
    /*
     * Vitis 2025.2 on Windows can fail to regenerate xparameters.h when its
     * optional dtc helper is absent.  The active XSA has already verified
     * PS UART1 at this address, so keep a polled-driver fallback rather than
     * silently compiling out all display traffic.
     */
    config = &fallback_config;
#endif
    if (config == NULL) {
        return XST_FAILURE;
    }

    status = XUartPs_CfgInitialize(&Display.uart,
                                   config,
                                   config->BaseAddress);
    if (status != XST_SUCCESS) {
        return status;
    }

    /*
     * The supplied TJC HMI starts Program.s at 9600 baud and page0 then
     * changes it to 115200.  Start at 9600 so Zynq can force that transition
     * even when the page-load event has not run yet.
     */
    format.BaudRate = DISPLAY_UART_BOOT_BAUD_RATE;
    format.DataBits = XUARTPS_FORMAT_8_BITS;
    format.Parity = XUARTPS_FORMAT_NO_PARITY;
    format.StopBits = XUARTPS_FORMAT_1_STOP_BIT;
    status = XUartPs_SetDataFormat(&Display.uart, &format);
    if (status != XST_SUCCESS) {
        return status;
    }

    XUartPs_SetOperMode(&Display.uart, XUARTPS_OPER_MODE_NORMAL);
    XUartPs_SetFifoThreshold(&Display.uart, 1U);
    XUartPs_SetRecvTimeout(&Display.uart, 8U);
    return XST_SUCCESS;
}

static int display_wait_status(uint32_t required_set,
                               uint32_t required_clear,
                               uint32_t timeout_us)
{
    uint32_t elapsed_us;

    for (elapsed_us = 0U;
         elapsed_us < timeout_us;
         elapsed_us += HMI_UART_POLL_INTERVAL_US) {
        uint32_t status =
            XUartPs_ReadReg(Display.uart.Config.BaseAddress,
                            XUARTPS_SR_OFFSET);

        if ((status & required_set) == required_set &&
            (status & required_clear) == 0U) {
            return XST_SUCCESS;
        }
        usleep(HMI_UART_POLL_INTERVAL_US);
    }

    Display.initialized = 0U;
    return XST_FAILURE;
}

static int display_wait_tx_done(uint32_t timeout_us)
{
    return display_wait_status(XUARTPS_SR_TXEMPTY,
                               XUARTPS_SR_TACTIVE,
                               timeout_us);
}

static int hmi_write_byte(uint8_t value)
{
    int status;

    if (!Display.initialized) {
        return XST_FAILURE;
    }

    status = display_wait_status(0U,
                                 XUARTPS_SR_TXFULL,
                                 HMI_UART_FIFO_TIMEOUT_US);
    if (status != XST_SUCCESS) {
        return status;
    }

    XUartPs_WriteReg(Display.uart.Config.BaseAddress,
                     XUARTPS_FIFO_OFFSET,
                     value);
    Display.transmitted_bytes++;
    return XST_SUCCESS;
}

static int hmi_write(const char *text)
{
    const uint8_t terminator[3] = {0xFFU, 0xFFU, 0xFFU};
    size_t index;
    int status;

    if (!Display.initialized || text == NULL) {
        return XST_FAILURE;
    }

    for (index = 0U; text[index] != '\0'; index++) {
        status = hmi_write_byte((uint8_t)text[index]);
        if (status != XST_SUCCESS) {
            return status;
        }
    }
    for (index = 0U; index < sizeof(terminator); index++) {
        status = hmi_write_byte(terminator[index]);
        if (status != XST_SUCCESS) {
            return status;
        }
    }
    return XST_SUCCESS;
}

static void hmi_command(const char *format, ...)
{
    char command[160];
    va_list arguments;

    va_start(arguments, format);
    (void)vsnprintf(command, sizeof(command), format, arguments);
    va_end(arguments);
    command[sizeof(command) - 1U] = '\0';
    (void)hmi_write(command);
}

static int32_t round_to_i32(float value)
{
    if (value >= 0.0f) {
        return (int32_t)(value + 0.5f);
    }
    return (int32_t)(value - 0.5f);
}

static void format_fixed(char *output,
                         size_t output_size,
                         float value,
                         float unit_scale,
                         uint32_t decimals)
{
    static const int32_t powers[] = {1, 10, 100, 1000};
    int32_t power;
    int32_t scaled;
    uint32_t magnitude;
    uint32_t whole;
    uint32_t fraction;
    const char *sign;

    if (decimals > 3U) {
        decimals = 3U;
    }

    power = powers[decimals];
    scaled = round_to_i32(value * unit_scale * (float)power);
    sign = scaled < 0 ? "-" : "";
    magnitude = scaled < 0 ?
        (uint32_t)(-(int64_t)scaled) : (uint32_t)scaled;
    whole = magnitude / (uint32_t)power;
    fraction = magnitude % (uint32_t)power;

    if (decimals == 0U) {
        (void)snprintf(output, output_size, "%s%lu",
                       sign, (unsigned long)whole);
    }
    else {
        (void)snprintf(output, output_size, "%s%lu.%0*lu",
                       sign,
                       (unsigned long)whole,
                       (int)decimals,
                       (unsigned long)fraction);
    }
}

#define SEGMENT_A (1U << 0)
#define SEGMENT_B (1U << 1)
#define SEGMENT_C (1U << 2)
#define SEGMENT_D (1U << 3)
#define SEGMENT_E (1U << 4)
#define SEGMENT_F (1U << 5)
#define SEGMENT_G (1U << 6)

static uint32_t seven_segment_mask(char value)
{
    switch (value) {
    case '0':
        return SEGMENT_A | SEGMENT_B | SEGMENT_C |
               SEGMENT_D | SEGMENT_E | SEGMENT_F;
    case '1':
        return SEGMENT_B | SEGMENT_C;
    case '2':
        return SEGMENT_A | SEGMENT_B | SEGMENT_D |
               SEGMENT_E | SEGMENT_G;
    case '3':
        return SEGMENT_A | SEGMENT_B | SEGMENT_C |
               SEGMENT_D | SEGMENT_G;
    case '4':
        return SEGMENT_B | SEGMENT_C | SEGMENT_F | SEGMENT_G;
    case '5':
        return SEGMENT_A | SEGMENT_C | SEGMENT_D |
               SEGMENT_F | SEGMENT_G;
    case '6':
        return SEGMENT_A | SEGMENT_C | SEGMENT_D |
               SEGMENT_E | SEGMENT_F | SEGMENT_G;
    case '7':
        return SEGMENT_A | SEGMENT_B | SEGMENT_C;
    case '8':
        return SEGMENT_A | SEGMENT_B | SEGMENT_C |
               SEGMENT_D | SEGMENT_E | SEGMENT_F | SEGMENT_G;
    case '9':
        return SEGMENT_A | SEGMENT_B | SEGMENT_C |
               SEGMENT_D | SEGMENT_F | SEGMENT_G;
    case '-':
        return SEGMENT_G;
    default:
        return 0U;
    }
}

static void hmi_fill(int x,
                     int y,
                     int width,
                     int height,
                     uint32_t color)
{
    if (width > 0 && height > 0) {
        hmi_command("fill %d,%d,%d,%d,%u",
                    x, y, width, height, color);
    }
}

static void draw_seven_segment_digit(int x,
                                     int y,
                                     int width,
                                     int height,
                                     int thickness,
                                     uint32_t color,
                                     char value)
{
    uint32_t mask;
    int half;
    int vertical_height;

    if (value == '.') {
        hmi_fill(x, y + height - thickness,
                 thickness, thickness, color);
        return;
    }

    mask = seven_segment_mask(value);
    half = height / 2;
    vertical_height = half - thickness;

    if ((mask & SEGMENT_A) != 0U) {
        hmi_fill(x + thickness, y,
                 width - 2 * thickness, thickness, color);
    }
    if ((mask & SEGMENT_B) != 0U) {
        hmi_fill(x + width - thickness, y + thickness,
                 thickness, vertical_height, color);
    }
    if ((mask & SEGMENT_C) != 0U) {
        hmi_fill(x + width - thickness, y + half,
                 thickness, vertical_height, color);
    }
    if ((mask & SEGMENT_D) != 0U) {
        hmi_fill(x + thickness, y + height - thickness,
                 width - 2 * thickness, thickness, color);
    }
    if ((mask & SEGMENT_E) != 0U) {
        hmi_fill(x, y + half,
                 thickness, vertical_height, color);
    }
    if ((mask & SEGMENT_F) != 0U) {
        hmi_fill(x, y + thickness,
                 thickness, vertical_height, color);
    }
    if ((mask & SEGMENT_G) != 0U) {
        hmi_fill(x + thickness, y + half - thickness / 2,
                 width - 2 * thickness, thickness, color);
    }
}

/*
 * The supplied HMI project has no runtime font resource: xstr returns
 * protocol error 0x05 for every font ID.  Draw numeric fields with fill
 * primitives so the measurement display is independent of HMI fonts.
 */
static void hmi_numeric_text(int x,
                             int y,
                             int width,
                             int height,
                             uint32_t color,
                             const char *text)
{
    size_t index;
    size_t length;
    int thickness;
    int digit_width;
    int dot_width;
    int spacing;
    int total_width = 0;
    int cursor;

    hmi_fill(x, y, width, height, HMI_COLOR_BLACK);
    if (text == NULL) {
        return;
    }

    length = strlen(text);
    thickness = height >= 30 ? 3 : 2;
    spacing = thickness;
    digit_width = (height - 4) / 2;
    dot_width = thickness;

    for (index = 0U; index < length; index++) {
        total_width += text[index] == '.' ? dot_width : digit_width;
        if (index + 1U < length) {
            total_width += spacing;
        }
    }

    while (total_width > width - 4 && digit_width > 6) {
        total_width = 0;
        digit_width--;
        for (index = 0U; index < length; index++) {
            total_width +=
                text[index] == '.' ? dot_width : digit_width;
            if (index + 1U < length) {
                total_width += spacing;
            }
        }
    }

    cursor = x + (width - total_width) / 2;
    for (index = 0U; index < length; index++) {
        int character_width =
            text[index] == '.' ? dot_width : digit_width;
        draw_seven_segment_digit(cursor,
                                 y + 2,
                                 character_width,
                                 height - 4,
                                 thickness,
                                 color,
                                 text[index]);
        cursor += character_width + spacing;
    }
}

static void draw_time_parameters(const MeasurementResult *result)
{
    char value[24];

    if ((result->flags & MEASUREMENT_FLAG_NO_SIGNAL) != 0U) {
        hmi_numeric_text(607, 102, 135, 35, HMI_COLOR_WHITE, "--");
        hmi_numeric_text(607, 188, 135, 35, HMI_COLOR_WHITE, "--");
        hmi_numeric_text(607, 274, 135, 35, HMI_COLOR_WHITE, "--");
    }
    else {
        format_fixed(value, sizeof(value),
                     result->peak_to_peak_volts, 1000.0f, 2U);
        hmi_numeric_text(607, 102, 135, 35,
                         HMI_COLOR_WHITE, value);

        format_fixed(value, sizeof(value),
                     result->rms_ac_volts, 1000.0f, 2U);
        hmi_numeric_text(607, 188, 135, 35,
                         HMI_COLOR_WHITE, value);

        format_fixed(value, sizeof(value),
                     result->fundamental_hz, 0.001f, 3U);
        hmi_numeric_text(607, 274, 135, 35,
                         HMI_COLOR_WHITE, value);
    }

    if ((result->flags &
         (MEASUREMENT_FLAG_NO_SIGNAL |
          MEASUREMENT_FLAG_CLIPPED |
          MEASUREMENT_FLAG_ANALYSIS_ERROR)) != 0U) {
        hmi_fill(626, 390, 151, 25, HMI_COLOR_ORANGE);
    }
    else {
        hmi_fill(626, 390, 151, 25, HMI_COLOR_GREEN);
    }
}

static void draw_time_waveform(const MeasurementResult *result)
{
    float minimum;
    float maximum;
    float span;
    uint32_t point;
    int previous_x = HMI_PLOT_LEFT;
    int previous_y = HMI_PLOT_TOP + HMI_PLOT_HEIGHT / 2;

    hmi_command("picq %d,%d,%d,%d,0",
                HMI_PLOT_LEFT, HMI_PLOT_TOP,
                HMI_PLOT_WIDTH, HMI_PLOT_HEIGHT);

    if ((result->flags & MEASUREMENT_FLAG_NO_SIGNAL) != 0U) {
        return;
    }

    minimum = result->waveform_one_period[0];
    maximum = minimum;
    for (point = 1U; point < APP_WAVEFORM_POINTS; point++) {
        float sample = result->waveform_one_period[point];
        if (sample < minimum) {
            minimum = sample;
        }
        if (sample > maximum) {
            maximum = sample;
        }
    }

    span = maximum - minimum;
    if (span < 1.0e-9f) {
        return;
    }

    for (point = 0U; point <= HMI_WAVE_SEGMENTS; point++) {
        uint32_t waveform_index =
            (point * (uint32_t)Display.period_count *
             APP_WAVEFORM_POINTS / HMI_WAVE_SEGMENTS) %
            APP_WAVEFORM_POINTS;
        float normalized =
            (result->waveform_one_period[waveform_index] - minimum) /
            span;
        int x = HMI_PLOT_LEFT +
            (int)(point * HMI_PLOT_WIDTH / HMI_WAVE_SEGMENTS);
        int y = HMI_PLOT_BOTTOM -
            10 - (int)(normalized * (float)(HMI_PLOT_HEIGHT - 20));

        if (point != 0U) {
            hmi_command("line %d,%d,%d,%d,%u",
                        previous_x, previous_y, x, y,
                        HMI_COLOR_CYAN);
        }
        previous_x = x;
        previous_y = y;
    }
}

static void draw_component_table(const MeasurementResult *result)
{
    static const int row_y[3] = {132, 194, 252};
    uint32_t index;
    char frequency[20];
    char amplitude[20];

    for (index = 0U; index < 3U; index++) {
        if (index < result->component_count &&
            (result->flags & MEASUREMENT_FLAG_NO_SIGNAL) == 0U) {
            format_fixed(frequency, sizeof(frequency),
                         result->components[index].frequency_hz,
                         0.001f, 2U);
            format_fixed(amplitude, sizeof(amplitude),
                         result->components[index].amplitude_peak_v,
                         1000.0f, 1U);
        }
        else {
            (void)strcpy(frequency, "--");
            (void)strcpy(amplitude, "--");
        }

        hmi_numeric_text(607, row_y[index], 82, 27,
                         HMI_COLOR_WHITE, frequency);
        hmi_numeric_text(730, row_y[index], 36, 27,
                         HMI_COLOR_WHITE, amplitude);
    }
}

static void draw_spectrum(const MeasurementResult *result)
{
    static const uint32_t colors[3] = {
        HMI_COLOR_BLUE, HMI_COLOR_ORANGE, HMI_COLOR_GREEN
    };
    float maximum_amplitude = 0.0f;
    uint32_t count = result->component_count;
    uint32_t index;

    if (count > 3U) {
        count = 3U;
    }

    hmi_command("picq %d,%d,%d,%d,1",
                HMI_PLOT_LEFT, HMI_PLOT_TOP,
                HMI_PLOT_WIDTH, HMI_PLOT_HEIGHT);

    if ((result->flags & MEASUREMENT_FLAG_NO_SIGNAL) != 0U) {
        return;
    }

    for (index = 0U; index < count; index++) {
        if (result->components[index].amplitude_peak_v >
            maximum_amplitude) {
            maximum_amplitude =
                result->components[index].amplitude_peak_v;
        }
    }
    if (maximum_amplitude < 1.0e-12f) {
        return;
    }

    for (index = 0U; index < count; index++) {
        const SignalComponent *component = &result->components[index];
        int x = HMI_PLOT_LEFT +
            (int)(component->frequency_hz *
                  (float)HMI_PLOT_WIDTH / HMI_SCREEN_MAX_HZ);
        int height = 8 +
            (int)(component->amplitude_peak_v /
                  maximum_amplitude *
                  (float)(HMI_PLOT_HEIGHT - 18));
        int top = HMI_PLOT_BOTTOM - height;
        int offset;

        if (x < HMI_PLOT_LEFT) {
            x = HMI_PLOT_LEFT;
        }
        if (x > HMI_PLOT_LEFT + HMI_PLOT_WIDTH) {
            x = HMI_PLOT_LEFT + HMI_PLOT_WIDTH;
        }

        for (offset = -1; offset <= 1; offset++) {
            hmi_command("line %d,%d,%d,%d,%u",
                        x + offset, HMI_PLOT_BOTTOM,
                        x + offset, top,
                        colors[index]);
        }
    }
}

static void draw_hold_state(void)
{
    hmi_fill(590, 379, 193, 37,
             Display.hold ? HMI_COLOR_ORANGE : HMI_COLOR_GREEN);
}

static void process_touch(uint16_t x, uint16_t y, uint8_t event)
{
    if (event != 0U) {
        return;
    }

    if (Display.current_page == HMI_PAGE_TIME) {
        if (x >= 590U && x <= 683U &&
            y >= 332U && y <= 381U) {
            Display.current_page = HMI_PAGE_SPECTRUM;
            Display.redraw_required = 1U;
            Display.refresh_divider = 0U;
        }
        else if (x >= 690U && x <= 783U &&
                 y >= 332U && y <= 381U) {
            Display.period_count =
                Display.period_count == 1U ? 3U : 1U;
            Display.redraw_required = 1U;
            Display.refresh_divider = 0U;
        }
    }
    else {
        if (x >= 590U && x <= 683U &&
            y >= 316U && y <= 365U) {
            Display.current_page = HMI_PAGE_TIME;
            Display.hold = 0U;
            Display.redraw_required = 1U;
            Display.refresh_divider = 0U;
        }
        else if (x >= 690U && x <= 783U &&
                 y >= 316U && y <= 365U) {
            Display.hold = Display.hold ? 0U : 1U;
            draw_hold_state();
            if (!Display.hold) {
                Display.redraw_required = 1U;
                Display.refresh_divider = 0U;
            }
        }
    }

    /* Read back the active page after the HMI executes its button event. */
    hmi_write("sendme");
}

static void process_packet(const uint8_t *packet, uint32_t length)
{
    if (length == 6U && packet[0] == 0x67U) {
        uint16_t x =
            ((uint16_t)packet[1] << 8) | (uint16_t)packet[2];
        uint16_t y =
            ((uint16_t)packet[3] << 8) | (uint16_t)packet[4];
        process_touch(x, y, packet[5]);
    }
    else if (length == 2U && packet[0] == 0x66U) {
        if (packet[1] <= HMI_PAGE_SPECTRUM) {
            Display.current_page = packet[1];
            if (Display.current_page == HMI_PAGE_TIME) {
                Display.hold = 0U;
            }
            Display.redraw_required = 1U;
            Display.refresh_divider = 0U;
        }
    }
    else if (length == 1U && packet[0] == 0x88U) {
        Display.current_page = HMI_PAGE_TIME;
        Display.hold = 0U;
        Display.redraw_required = 1U;
        Display.refresh_divider = 0U;
    }
}

int display_init(void)
{
    int status;

    memset(&Display, 0, sizeof(Display));
    status = display_uart_init();
    if (status != XST_SUCCESS) {
        return status;
    }

    Display.initialized = 1U;
    Display.current_page = HMI_PAGE_TIME;
    Display.period_count = 1U;
    Display.redraw_required = 1U;

    /*
     * During SD cold boot the Zynq application starts while the HMI is still
     * loading its project.  JTAG starts much later and therefore hid this
     * race.  The first command handles the HMI's 9600-baud boot state; after
     * switching locally, the second handles a page0 already at 115200.
     */
    usleep(HMI_POWER_ON_DELAY_US);
    status = hmi_write("baud=115200");
    if (status != XST_SUCCESS ||
        display_wait_tx_done(HMI_UART_DONE_TIMEOUT_US) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    usleep(50000U);
    status = XUartPs_SetBaudRate(&Display.uart,
                                 DISPLAY_UART_BAUD_RATE);
    if (status != XST_SUCCESS) {
        Display.initialized = 0U;
        return status;
    }

    if (hmi_write("baud=115200") != XST_SUCCESS ||
        hmi_write("bkcmd=0") != XST_SUCCESS ||
        hmi_write("sendxy=1") != XST_SUCCESS ||
        hmi_write("page 0") != XST_SUCCESS) {
        return XST_FAILURE;
    }
    usleep(100000U);
    hmi_fill(626, 390, 151, 25, HMI_COLOR_GREEN);
    if (!Display.initialized || hmi_write("sendme") != XST_SUCCESS ||
        display_wait_tx_done(HMI_UART_DONE_TIMEOUT_US) != XST_SUCCESS) {
        return XST_FAILURE;
    }
    usleep(100000U);
    display_service();
    return XST_SUCCESS;
}

uint32_t display_transmitted_bytes(void)
{
    return Display.transmitted_bytes;
}

uint32_t display_received_packets(void)
{
    return Display.received_packets;
}

void display_service(void)
{
    uint32_t base;

    if (!Display.initialized) {
        return;
    }

    base = Display.uart.Config.BaseAddress;
    while (XUartPs_IsReceiveData(base)) {
        uint8_t byte = (uint8_t)XUartPs_ReadReg(base,
                                                XUARTPS_FIFO_OFFSET);

        if (byte == 0xFFU) {
            Display.rx_ff_count++;
            if (Display.rx_ff_count == 3U) {
                Display.received_packets++;
                process_packet(Display.rx_packet,
                               Display.rx_length);
                Display.rx_length = 0U;
                Display.rx_ff_count = 0U;
            }
            continue;
        }

        while (Display.rx_ff_count != 0U) {
            if (Display.rx_length < sizeof(Display.rx_packet)) {
                Display.rx_packet[Display.rx_length++] = 0xFFU;
            }
            Display.rx_ff_count--;
        }

        if (Display.rx_length < sizeof(Display.rx_packet)) {
            Display.rx_packet[Display.rx_length++] = byte;
        }
        else {
            Display.rx_length = 0U;
            Display.rx_ff_count = 0U;
        }
    }
}

void display_publish_measurement(const MeasurementResult *result)
{
    uint32_t refresh_period;

    if (!Display.initialized || result == NULL) {
        return;
    }

    display_service();
    if (Display.hold && Display.current_page == HMI_PAGE_SPECTRUM) {
        return;
    }

    refresh_period =
        Display.current_page == HMI_PAGE_TIME ?
        HMI_REFRESH_TIME_FRAMES : HMI_REFRESH_FFT_FRAMES;

    if (!Display.redraw_required) {
        Display.refresh_divider++;
        if (Display.refresh_divider < refresh_period) {
            return;
        }
    }

    Display.refresh_divider = 0U;
    Display.redraw_required = 0U;

    if (Display.current_page == HMI_PAGE_TIME) {
        draw_time_parameters(result);
        draw_time_waveform(result);
    }
    else {
        draw_component_table(result);
        draw_spectrum(result);
        draw_hold_state();
    }
}
