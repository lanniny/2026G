#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "display_port.h"
#include "display_waveform.h"
#include "xparameters.h"
#include "xuartps.h"
#include "xuartps_hw.h"

#define TEST_PI_F       3.14159265358979323846f
#define TEST_TWO_PI_F   (2.0f * TEST_PI_F)
#define TEST_RX_CAPACITY 8192U
#define TEST_COMMAND_CAPACITY 2048U

typedef struct {
    uint8_t rx[TEST_RX_CAPACITY];
    uint32_t rx_read;
    uint32_t rx_write;
    uint8_t command[TEST_COMMAND_CAPACITY];
    uint32_t command_length;
    uint32_t command_ff_count;
    uint8_t payload[DISPLAY_WAVEFORM_POINT_COUNT];
    uint32_t payload_count;
    uint32_t transparent_remaining;
    uint32_t addt_count;
    uint8_t page;
    uint8_t touch_before_ready;
    uint8_t drop_ready;
    uint8_t drop_finished;
} FakeHmi;

static FakeHmi Fake;
static XUartPs_Config FakeConfig = {
    XPAR_XUARTPS_1_DEVICE_ID,
    XPAR_XUARTPS_1_BASEADDR,
    100000000U,
    0
};
static int Failures;

static void check(int condition, const char *name)
{
    printf("  %-58s %s\n", name, condition ? "PASS" : "FAIL");
    if (!condition) {
        Failures++;
    }
}

static void push_rx_byte(uint8_t value)
{
    if (Fake.rx_write < TEST_RX_CAPACITY) {
        Fake.rx[Fake.rx_write++] = value;
    }
}

static void push_packet(const uint8_t *packet, uint32_t length)
{
    uint32_t index;

    for (index = 0U; index < length; index++) {
        push_rx_byte(packet[index]);
    }
    push_rx_byte(0xFFU);
    push_rx_byte(0xFFU);
    push_rx_byte(0xFFU);
}

static void push_touch(uint16_t x, uint16_t y)
{
    uint8_t packet[6];

    packet[0] = 0x67U;
    packet[1] = (uint8_t)(x >> 8);
    packet[2] = (uint8_t)x;
    packet[3] = (uint8_t)(y >> 8);
    packet[4] = (uint8_t)y;
    packet[5] = 0U;
    push_packet(packet, sizeof(packet));
}

static int command_equals(const char *text)
{
    size_t length = strlen(text);

    return Fake.command_length == length &&
        memcmp(Fake.command, text, length) == 0;
}

static void process_command(void)
{
    static const uint8_t ready[] = {0xFEU};
    uint8_t page_report[2];

    if (command_equals("addt 4,0,506")) {
        Fake.addt_count++;
        Fake.payload_count = 0U;
        Fake.transparent_remaining = DISPLAY_WAVEFORM_POINT_COUNT;
        if (Fake.touch_before_ready) {
            push_touch(700U, 350U);
        }
        if (!Fake.drop_ready) {
            push_packet(ready, sizeof(ready));
        }
    }
    else if (command_equals("sendme")) {
        page_report[0] = 0x66U;
        page_report[1] = Fake.page;
        push_packet(page_report, sizeof(page_report));
    }
    else if (command_equals("page 0")) {
        Fake.page = 0U;
    }
    else if (command_equals("page 1")) {
        Fake.page = 1U;
    }
}

static void accept_tx_byte(uint8_t value)
{
    static const uint8_t finished[] = {0xFDU};

    if (Fake.transparent_remaining != 0U) {
        if (Fake.payload_count < DISPLAY_WAVEFORM_POINT_COUNT) {
            Fake.payload[Fake.payload_count++] = value;
        }
        Fake.transparent_remaining--;
        if (Fake.transparent_remaining == 0U && !Fake.drop_finished) {
            push_packet(finished, sizeof(finished));
        }
        return;
    }

    if (value == 0xFFU) {
        Fake.command_ff_count++;
        if (Fake.command_ff_count == 3U) {
            process_command();
            Fake.command_length = 0U;
            Fake.command_ff_count = 0U;
        }
        return;
    }

    while (Fake.command_ff_count != 0U) {
        if (Fake.command_length < TEST_COMMAND_CAPACITY) {
            Fake.command[Fake.command_length++] = 0xFFU;
        }
        Fake.command_ff_count--;
    }
    if (Fake.command_length < TEST_COMMAND_CAPACITY) {
        Fake.command[Fake.command_length++] = value;
    }
}

XUartPs_Config *XUartPs_LookupConfig(uint16_t device_id)
{
    return device_id == XPAR_XUARTPS_1_DEVICE_ID ? &FakeConfig : NULL;
}

int XUartPs_CfgInitialize(XUartPs *instance,
                          XUartPs_Config *config,
                          uint32_t effective_address)
{
    instance->Config = *config;
    instance->Config.BaseAddress = effective_address;
    return 0;
}

int XUartPs_SetDataFormat(XUartPs *instance, XUartPsFormat *format)
{
    (void)instance;
    (void)format;
    return 0;
}

void XUartPs_SetOperMode(XUartPs *instance, uint8_t mode)
{
    (void)instance;
    (void)mode;
}

void XUartPs_SetFifoThreshold(XUartPs *instance, uint8_t threshold)
{
    (void)instance;
    (void)threshold;
}

void XUartPs_SetRecvTimeout(XUartPs *instance, uint8_t timeout)
{
    (void)instance;
    (void)timeout;
}

int XUartPs_SetBaudRate(XUartPs *instance, uint32_t baud_rate)
{
    (void)instance;
    (void)baud_rate;
    return 0;
}

uint32_t XUartPs_ReadReg(uint32_t base_address, uint32_t offset)
{
    (void)base_address;
    if (offset == XUARTPS_SR_OFFSET) {
        return XUARTPS_SR_TXEMPTY;
    }
    if (offset == XUARTPS_FIFO_OFFSET && Fake.rx_read < Fake.rx_write) {
        return Fake.rx[Fake.rx_read++];
    }
    return 0U;
}

void XUartPs_WriteReg(uint32_t base_address,
                      uint32_t offset,
                      uint32_t value)
{
    (void)base_address;
    if (offset == XUARTPS_FIFO_OFFSET) {
        accept_tx_byte((uint8_t)value);
    }
}

int XUartPs_IsReceiveData(uint32_t base_address)
{
    (void)base_address;
    return Fake.rx_read < Fake.rx_write;
}

void usleep(uint32_t useconds)
{
    (void)useconds;
}

static void make_result(MeasurementResult *result)
{
    uint32_t point;

    memset(result, 0, sizeof(*result));
    result->fundamental_hz = 10000.0f;
    result->component_count = 2U;
    result->components[0].harmonic = 1U;
    result->components[0].frequency_hz = 10000.0f;
    result->components[0].amplitude_peak_v = 0.040f;
    result->components[0].phase_rad = 0.31f;
    result->components[1].harmonic = 50U;
    result->components[1].frequency_hz = 500000.0f;
    result->components[1].amplitude_peak_v = 0.010f;
    result->components[1].phase_rad = -0.72f;

    for (point = 0U; point < APP_WAVEFORM_POINTS; point++) {
        float phase = TEST_TWO_PI_F * (float)point /
                      (float)APP_WAVEFORM_POINTS;

        result->waveform_one_period[point] =
            0.040f * cosf(phase + 0.31f) +
            0.010f * cosf(50.0f * phase - 0.72f);
    }
}

static void start_case(void)
{
    memset(&Fake, 0, sizeof(Fake));
    Fake.page = 0U;
    check(display_init() == 0, "display initialization and page query");
}

static void test_touch_before_ready(void)
{
    MeasurementResult result;
    uint8_t expected[DISPLAY_WAVEFORM_POINT_COUNT];

    printf("CASE touch packet precedes transparent-ready marker\n");
    start_case();
    make_result(&result);
    check(display_waveform_make(&result, 1U, expected),
          "expected one-period payload generated");
    Fake.touch_before_ready = 1U;
    display_publish_measurement(&result);
    check(Fake.addt_count == 1U, "one addt transaction issued");
    check(Fake.payload_count == DISPLAY_WAVEFORM_POINT_COUNT,
          "transparent payload contains exactly 506 bytes");
    check(memcmp(Fake.payload, expected, sizeof(expected)) == 0,
          "touch handling emits no command inside raw payload");
}

static void test_missing_ready_recovers(void)
{
    MeasurementResult result;
    uint8_t expected[DISPLAY_WAVEFORM_POINT_COUNT];

    printf("CASE missing ready marker is drained and retried\n");
    start_case();
    make_result(&result);
    check(display_waveform_make(&result, 1U, expected),
          "expected payload generated");
    Fake.drop_ready = 1U;
    display_publish_measurement(&result);
    check(Fake.addt_count == 1U, "failed transaction reached addt");
    check(Fake.transparent_remaining == 0U,
          "recovery drains pending transparent byte count");

    Fake.drop_ready = 0U;
    display_publish_measurement(&result);
    check(Fake.addt_count == 2U, "next refresh retries addt");
    check(Fake.payload_count == DISPLAY_WAVEFORM_POINT_COUNT &&
          memcmp(Fake.payload, expected, sizeof(expected)) == 0,
          "retry delivers the exact waveform payload");
}

static void test_missing_finished_recovers(void)
{
    MeasurementResult result;
    uint8_t expected[DISPLAY_WAVEFORM_POINT_COUNT];

    printf("CASE missing finished marker is resynchronized\n");
    start_case();
    make_result(&result);
    check(display_waveform_make(&result, 1U, expected),
          "expected payload generated");
    Fake.drop_finished = 1U;
    display_publish_measurement(&result);
    check(Fake.addt_count == 1U, "failed completion reached addt");
    check(Fake.transparent_remaining == 0U,
          "screen is outside transparent mode after recovery");

    Fake.drop_finished = 0U;
    display_publish_measurement(&result);
    check(Fake.addt_count == 2U, "next refresh starts a clean transaction");
    check(Fake.payload_count == DISPLAY_WAVEFORM_POINT_COUNT &&
          memcmp(Fake.payload, expected, sizeof(expected)) == 0,
          "post-timeout transaction remains byte exact");
}

static void test_page_confirmation_gates_addt(void)
{
    MeasurementResult result;

    printf("CASE page confirmation gates page0 waveform commands\n");
    start_case();
    make_result(&result);

    Fake.page = 1U;
    push_touch(600U, 350U);
    display_publish_measurement(&result);
    check(Fake.addt_count == 0U,
          "confirmed spectrum page receives no page0 addt");

    Fake.page = 0U;
    push_touch(600U, 330U);
    display_publish_measurement(&result);
    check(Fake.addt_count == 1U,
          "confirmed time page resumes waveform transfer");
}

int main(void)
{
    test_touch_before_ready();
    test_missing_ready_recovers();
    test_missing_finished_recovers();
    test_page_confirmation_gates_addt();

    printf("DISPLAY_PORT_%s failures=%d\n",
           Failures == 0 ? "PASS" : "FAIL",
           Failures);
    return Failures == 0 ? 0 : 1;
}
