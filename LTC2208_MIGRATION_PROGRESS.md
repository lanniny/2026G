# LTC2208 16-bit migration progress

Baseline: `G:\tasks\ecomp\2026\ttt.zip`, extracted into an isolated working directory without modifying the archive.

## Verified inputs

- DG4162 identity: `Rigol Technologies,DG4162,DG4E165253659,00.01.09` at `192.168.31.35:5555`.
- Module input: 50 ohm, 9 Vpp (-4.5 V to +4.5 V).
- Module data: 16-bit offset binary, normal polarity, full-rate CMOS.
- LTC2208 CMOS ENC-to-data delay: 1.3 ns minimum, 4.0 ns maximum.
- SHDN: low = normal operation, high = shutdown.
- ADC clock: 64 MHz; application sample rate: 4 MS/s after /16 decimation.

## Final pin map

```text
SHDN W22    CKI  V22
D0   Y21    CKO  Y20
D2   AB22   D1   AA22
D4   AB21   D3   AA21
D6   AB19   D5   AB20
D8   AA19   D7   Y19
D10  AB16   D9   AA16
D12  Y18    D11  AA18
D14  AB14   D13  AB15
OFA  Y13    D15  AA13
```

## Completed work

- [x] Extract isolated source and reference copies.
- [x] Verify instrument identity and datasheet electrical behavior.
- [x] Migrate PL capture RTL and constraints to one 16-bit LTC2208 lane.
- [x] Update PS conversion constants and diagnostics.
- [x] Pass LTC2208 RTL simulation and host-side signal-analysis regression.
- [x] Migrate the live Block Design and complete Vivado implementation.
- [x] Close timing: WNS +2.366 ns, TNS 0.000 ns, no failing endpoints.
- [x] Regenerate the Vitis platform from the current XSA and rebuild the application ELF.
- [x] Download bitstream and ELF through JTAG and run the application on hardware.
- [x] Generate and Bootgen-readback-verify the three-stage SD `BOOT.BIN`.
- [x] Write the image to a FAT32 SD card and verify its SHA-256 by readback.
- [x] Complete user-observed signal tests; the original small-harmonic failure is effectively resolved.

Release evidence and hashes are recorded in `release/v1.0/VERIFICATION.md` and `release/v1.0/SHA256SUMS.txt`.

