# v1.0.2-rc2 contest-constraint analysis candidate

This image keeps the field-proven quiet FSBL and routed LTC2208 PL
bitstream. The PS application adds the low-frequency leakage fix and uses
the official G-problem constraints during fundamental selection.

## Analysis changes

- Use Hann-weighted least squares for harmonic amplitude and phase fitting.
- Refine the measured fundamental frequency without hard-snapping it to the
  nominal grid, preserving accuracy when the sampling clock has a small error.
- Prefer fundamental candidates on the official 500 Hz grid and fall back to
  continuous-frequency selection when a real clock offset exceeds the grid
  tolerance.
- Support the complete 10 kHz to 500 kHz component range, including H49/H50
  at a 10 kHz fundamental.
- Detect a 5 mV-peak fundamental even when H2/H4 are much stronger.
- Keep H1 plus at most the two strongest harmonics, matching the stated input
  composition, then compute composite Vpp and true RMS from those components.
- Prevent finite-record leakage from a strong low-frequency pure sine from
  appearing as false H2/H3/H4 components.

## Verification summary

- Host analysis: `PS_ANALYSIS_PASS`.
- Requirement sweep: `REQUIREMENT_SWEEP_PASS failed_cases=0`.
- Interference regression: `INTERFERENCE_TEST_PASS failures=0`.
- PL simulation: `PL_SIM_PASS samples=8192 status=0x8a`.
- Full-grid deterministic sweep: 160 vectors, zero failures, maximum frequency
  error 1.031 Hz and maximum component-amplitude error 0.015 mV.
- Composite Vpp against a 16,384-point reference: maximum error 0.306 mV.
- Cortex-A9 application: text 98,952 bytes, data 2,332 bytes, BSS 244,080
  bytes; no application-source warnings.
- Bootgen generated and read back the image successfully. The application
  load and execution address is `0x00100000`.

## Board result and power requirement

The exact `BOOT.BIN` hash in `SHA256SUMS.txt` was tested on the target board.
With an adequate stable 5 V supply, the reported measurements were fully
correct. An underpowered supply had previously caused slower screen refresh
and random low-frequency spectrum results. Treat input-power integrity as a
hardware acceptance prerequisite and verify the 5 V rail at the assembled
device under load.
