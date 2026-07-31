# v1.0 verification record

Verification date: 2026-07-31

## Vivado implementation

- Tool: Vivado 2025.2, device `xc7z020clg484-1`.
- Routed timing: WNS `+2.366 ns`, TNS `0.000 ns`, zero setup failures.
- Hold timing: WHS `+0.037 ns`, THS `0.000 ns`, zero hold failures.
- LTC2208 input setup worst slack: `+2.366 ns` across 16 timed inputs.
- Timing report: `adc_easy_test/timing_ltc2208_4msps/timing_summary.rpt`.

## Host regression

`adctestvitis/adctestps/tests/run_host_tests.bat` completed with:

```text
PS_ANALYSIS_PASS
REQUIREMENT_SWEEP_PASS failed_cases=0
INTERFERENCE_TEST_PASS failures=0
```

The sweep includes 10 kHz to 250 kHz boundaries, 50 mVpp to 250 mVpp inputs, multiple harmonics, and out-of-band interference cases.

## RTL simulation

`adc_easy_test/sim_4msps/run_sim.bat` compiled the LTC2208 capture RTL with XPM CDC models and completed with:

```text
PL_SIM_PASS samples=8192 status=0x8a
```

## JTAG hardware run

The current bitstream and application were downloaded to the powered target. XSDB reached all four markers:

```text
LTC2208_JTAG_FPGA_DONE
LTC2208_JTAG_PS_INIT_DONE
LTC2208_JTAG_ELF_DOWNLOAD_DONE
LTC2208_APPLICATION_RUNNING
```

The user then exercised the real LTC2208 measurement path and reported that the original small-harmonic issue was effectively resolved.

## SD boot image

Bootgen 2025.2 generated `BOOT.BIN` successfully and read it back as:

| Image | Partition count | Load address | Execute address |
| --- | ---: | ---: | ---: |
| `fsbl.elf` | 1 | `0x00000000` | `0x00000000` |
| `design_1_wrapper.bit` | 1 | `0x00000000` | `0x00000000` |
| `adctestps.elf` | 2 | `0x00100000`, `0x00118000` | `0x00100000`, `0x00000000` |

The generated image is 4,259,528 bytes with SHA-256:

```text
62B1433683941D22092E8EEF03FF1BCE179CC4B582CE81371936C0C16181FC45
```

The image was written to `K:` (FAT32, removable USB media, physical disk index 2) by `write_sd.ps1`. The script completed with:

```text
SD_WRITE_VERIFIED_SHA256=62B1433683941D22092E8EEF03FF1BCE179CC4B582CE81371936C0C16181FC45
```

## Artifact provenance

- Release bitstream SHA-256 equals `adc_easy_test/adc_easy_test.runs/impl_1/design_1_wrapper.bit`.
- Release XSA SHA-256 equals `adc_easy_test/design_1_wrapper.xsa`.
- Current FSBL `ps7_init.c` SHA-256: `7DC9067CC1625D52F3CBB95B58DC06A793BA701459F4C2973123CA85D161C570`.
- Full release hashes: `SHA256SUMS.txt`.
