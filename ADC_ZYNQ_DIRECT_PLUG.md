# AD9226 board direct-plug pinout

This mapping is for the ADC board and HelloFPGA Smart ZYNQ board shown in the
project photos.

## Mechanical orientation

Align the `A1 / ACK` end of the ADC signal socket with the ZYNQ right-side
`BANK33` header end marked:

```text
pin 1 = U22
pin 2 = T22
```

Before applying power, verify with a continuity meter that:

- ADC `A1` contacts ZYNQ `U22`.
- ADC `ACK` contacts ZYNQ `T22`.
- ADC `B1` contacts ZYNQ `AB16`.
- ADC `BCK` contacts ZYNQ `AA16`.

Do not rotate the ADC board by 180 degrees or swap the two socket rows.

## Channel A

| ADC signal | FPGA port | ZYNQ package pin |
|---|---|---|
| ACK | ADC_CLK_COMMON_OUT1_0 | T22 |
| A1 | ADC_DATA_IN_PIN1_0[0] | U22 |
| A2 | ADC_DATA_IN_PIN1_0[1] | V22 |
| A3 | ADC_DATA_IN_PIN1_0[2] | W22 |
| A4 | ADC_DATA_IN_PIN1_0[3] | Y20 |
| A5 | ADC_DATA_IN_PIN1_0[4] | Y21 |
| A6 | ADC_DATA_IN_PIN1_0[5] | AA22 |
| A7 | ADC_DATA_IN_PIN1_0[6] | AB22 |
| A8 | ADC_DATA_IN_PIN1_0[7] | AA21 |
| A9 | ADC_DATA_IN_PIN1_0[8] | AB21 |
| A10 | ADC_DATA_IN_PIN1_0[9] | AB20 |
| A11 | ADC_DATA_IN_PIN1_0[10] | AB19 |
| A12 | ADC_DATA_IN_PIN1_0[11] | Y19 |
| ORA | unused | AA19 |

## Channel B

| ADC signal | FPGA port | ZYNQ package pin |
|---|---|---|
| BCK | ADC_CLK_COMMON_OUT2_0 | AA16 |
| B1 | ADC_DATA_IN_PIN2_0[0] | AB16 |
| B2 | ADC_DATA_IN_PIN2_0[1] | AA18 |
| B3 | ADC_DATA_IN_PIN2_0[2] | Y18 |
| B4 | ADC_DATA_IN_PIN2_0[3] | AB15 |
| B5 | ADC_DATA_IN_PIN2_0[4] | AB14 |
| B6 | ADC_DATA_IN_PIN2_0[5] | AA13 |
| B7 | ADC_DATA_IN_PIN2_0[6] | Y13 |
| B8 | ADC_DATA_IN_PIN2_0[7] | W13 |
| B9 | ADC_DATA_IN_PIN2_0[8] | V13 |
| B10 | ADC_DATA_IN_PIN2_0[9] | W17 |
| B11 | ADC_DATA_IN_PIN2_0[10] | W18 |
| B12 | ADC_DATA_IN_PIN2_0[11] | AB17 |
| ORB | unused | AA17 |

All FPGA-facing ADC clock and data signals use `LVCMOS33`.
Confirm that the ZYNQ board `BANK33 VCCIO` selector is actually configured for
3.3 V before powering the boards; do not use this mapping with BANK33 set to
1.8 V or 2.5 V.

## Power

The ADC board `+5V` and `GND` holes are not assigned to FPGA I/O:

- Connect ADC `+5V` to the intended 5 V supply.
- Connect ADC `GND` and ZYNQ `GND` together.
- Never connect ADC `+5V` to a ZYNQ 3.3 V I/O pin.
- Insert or remove the ADC board only with both boards powered off.

The current 4 MS/s design captures channel A. Channel B is pinned correctly
for future use but is not part of the active sample stream.

## ADC input timing assumptions

The PL now forwards both ADC clocks through ODDR output registers and captures
each channel at a phase of 300 degrees relative to its forwarded sampling
clock.  At 64 MHz, the capture edge is 13.0208 ns after the ADC clock edge.

The constraints in `ad9226x.xdc` use the AD9226 Rev. B output-delay limits:

- Minimum data output delay: 3.5 ns.
- Maximum data output delay: 7.0 ns.
- Constraint maximum: 8.0 ns, including 1.0 ns for combined direct-plug PCB
  clock/data flight time and skew.

This gives nominal external setup and hold margins of approximately 5.02 ns
and 6.10 ns before FPGA internal setup/hold, clock uncertainty, and routing
are applied.  Channel A is captured in an input IOB register.

The 1.0 ns board allowance must be replaced by measured trace/oscilloscope
data if the boards are connected through a cable, adapter, or revised PCB.
Run `build_4msps.tcl`; it generates detailed reports in `timing_4msps` and
stops instead of exporting a new XSA if final setup or hold slack is negative.
