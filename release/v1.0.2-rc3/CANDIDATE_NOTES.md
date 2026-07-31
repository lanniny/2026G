# v1.0.2-rc3 frequency-boundary tolerance candidate

This candidate keeps the field-proven v1.0.2-rc2 quiet FSBL and routed
LTC2208 PL bitstream. The PS application changes only the interpretation of
measured frequencies near the legal fundamental boundaries.

## Analysis changes

- Keep the legal nominal fundamental range at 10 kHz through 250 kHz.
- Accept a measured boundary deviation of up to 1 kHz, plus a 5 Hz numerical
  guard for floating-point refinement.
- Separate the continuously fitted frequency from the official 500 Hz nominal
  grid frequency. Amplitude and phase use the continuous fit; legality,
  harmonic order, and displayed contest frequencies use the nominal grid.
- Preserve the nominal 500 kHz H2 when a legal 250 kHz fundamental is measured
  as high as 251 kHz. The H2 amplitude is fitted at the corresponding
  continuous frequency instead of being dropped at the display boundary.
- Expand only the internal peak-search guard band. The displayed/legal
  component limit remains 500 kHz.

## Boundary behavior

- A 9.0 kHz equivalent boundary measurement reports nominal 10 kHz and keeps
  its nominal 20 kHz H2.
- A 251.0 kHz equivalent boundary measurement reports nominal 250 kHz and
  keeps its nominal 500 kHz H2.
- 9.76 kHz and 250.24 kHz inside-boundary estimates also report their nominal
  10 kHz and 250 kHz grid frequencies.
- The 8.995/251.005 kHz endpoints are accepted; 8.994/251.006 kHz are rejected,
  proving the configured 1 kHz error limit plus 5 Hz numerical guard.

## Verification summary

- Host analysis: `PS_ANALYSIS_PASS`.
- Requirement sweep: `REQUIREMENT_SWEEP_PASS failed_cases=0`.
- Interference regression: `INTERFERENCE_TEST_PASS failures=0`.
- Strict host builds: C11, `-Wall -Wextra -Werror -pedantic`, all passed.
- PL simulation: `PL_SIM_PASS samples=8192 status=0x8a`.
- Cortex-A9 application: text 99,336 bytes, data 2,332 bytes, BSS 244,080
  bytes; no application-source warnings.
- Bootgen generated and read back the image successfully. The application
  load and execution address remains `0x00100000`.
- Independent PowerShell and CertUtil SHA-256 checks agree on the BOOT image.

## Physical status

The reused FSBL and PL bitstream are byte-identical to the field-proven rc2
artifacts. This rc3 BOOT image itself has not yet been tested on the target
board. Keep rc2 as the field-proven fallback until rc3 passes the same SD cold
boot and measurement checks.
