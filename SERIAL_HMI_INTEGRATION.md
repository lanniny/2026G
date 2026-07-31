# USART-HMI integration

The supplied `G题_串口屏完整工程.HMI` is an 800 x 480 USART-HMI
project. Its page-0 load event selects 115200 baud and enables coordinate
return (`sendxy=1`).

## Wiring

| Zynq signal | FPGA package pin | Connect to display |
|---|---:|---|
| `HMI_UART_0_txd` | H19 | RX |
| `HMI_UART_0_rxd` | H20 | TX |
| GND | board GND | GND |

The link is 3.3 V TTL UART, 115200 baud, 8 data bits, no parity, one stop
bit. Do not connect it to an RS-232 voltage-level port. Power the screen
according to its module specification and always connect the grounds.

The existing UART0 connection on L17/M17 is retained for USB debug output.

## Vivado hardware update

With `adc_easy_test.xpr` open, run this once in the Vivado Tcl Console:

```tcl
source D:/vivado/zynq/ttt/adc_easy_test/add_hmi_uart_emio.tcl
```

Then regenerate the bitstream and export an XSA including the bitstream.
In Vitis, update/re-read that XSA for the `adctestp` platform before
rebuilding `adctestps`.

## Implemented display behavior

- Time page: UPP, AC URMS, fundamental frequency and reconstructed waveform.
- `1 / 3 周期`: switches between one and three displayed periods.
- Spectrum page: the first three detected components, their frequencies,
  peak amplitudes, and normalized spectrum lines.
- `保持 / 运行`: freezes/resumes display updates without stopping ADC/DMA.
- The static footer is corrected at run time to 4.000 MSPS, FFT 8192 and
  488.28 Hz/bin.

The code is in `adctestvitis/adctestps/src/display_port.c`. Commands use
the USART-HMI `0xFF 0xFF 0xFF` terminator and the screen's `0x67`
coordinate-return packets.
