# v1.0.1 verification record

Verification date: 2026-07-31

## Root-cause evidence

The old SD image was inspected without resetting or downloading the target:

- CPU state was `Running`.
- UART1 reference/APB clocks and transmitter/receiver were enabled.
- UART1 registers were `CR=0x00000114`, `MR=0x00000020`, `BRGR=0x0000007C`, `BDIV=0x00000006`, corresponding to 115200 baud from the 100 MHz reference clock.
- Bytes enqueued into the UART1 TX FIFO increased from `0x00009A7E` to `0x00009B90`, while received packets remained zero.

This rules out the previously suspected permanent `XUartPs_WaitTransmitDone` stall for the observed boot. The SD-only failure is consistent with the application switching from the HMI's 9600-baud boot state before the display has finished its own power-on initialization. JTAG starts later and therefore does not reproduce that race.

## UART1 mitigation

- HMI power-on delay: 2,000,000 us.
- TX FIFO timeout: 20,000 us.
- TX completion timeout: 250,000 us.
- A timeout disables HMI traffic without stopping measurement or UART0 diagnostics.
- `XUartPs_WaitTransmitDone` call sites in the final application ELF: 0.
- Unbounded UART waits in application source: 0.

## Platform and software build

`adctestvitis/refresh_dual_uart_platform.py` completed a full Vitis 2025.2 domain regeneration and platform build with:

```text
PLATFORM_DOMAIN_REGEN=FULL
PLATFORM_XSA_SHA256=5C5D21D399E183126B9DE727CC48DA72BD1C797AA88F3ABEF32F67BEA4A97A58
PLATFORM_PS7_INIT_SHA256=7DC9067CC1625D52F3CBB95B58DC06A793BA701459F4C2973123CA85D161C570
LTC2208_PLATFORM_REFRESH_COMPLETE
DUAL_UART_PLATFORM_REFRESH_COMPLETE
```

The rebuilt FSBL contains `InitSD`, `SDAccess`, `ps7_init`, and `ps7_post_config`. The application then completed a clean verbose build with text/data/bss sizes `97736/2332/244080` bytes.

All checked `ps7_init.c` copies in `hw`, `hw/sdt`, `export/adctestp/hw`, both BSP hardware-artifact directories, and the FSBL source directory match the current XSA hash above.

## Regression tests

Host signal-analysis tests:

```text
PS_ANALYSIS_PASS
REQUIREMENT_SWEEP_PASS failed_cases=0
INTERFERENCE_TEST_PASS failures=0
```

RTL simulation:

```text
PL_SIM_PASS samples=8192 status=0x8a
```

## SD boot image

Bootgen 2025.2 generated and read back `BOOT.BIN` successfully:

| Image | Partition count | Load address | Execute address |
| --- | ---: | ---: | ---: |
| `fsbl.elf` | 1 | `0x00000000` | `0x00000000` |
| `design_1_wrapper.bit` | 1 | `0x00000000` | `0x00000000` |
| `adctestps.elf` | 2 | `0x00100000`, `0x00118000` | `0x00100000`, `0x00000000` |

Final image size: 4,259,784 bytes.

Final image SHA-256:

```text
478E8A1A5B8FC5ED498E3DD5B184CA36B03ED9C9C00A59EC98D47758A26A37AB
```

Release inputs exactly match the current application build, FSBL build, routed bitstream, and exported XSA.

## SD card write/readback

The image was written to `K:` (FAT32, removable USB media, 29.11 GiB, physical disk index 2). The previous v1.0 image was backed up with SHA-256 `62B1433683941D22092E8EEF03FF1BCE179CC4B582CE81371936C0C16181FC45` before replacement. The final card readback completed with:

```text
SD_WRITE_VERIFIED_SHA256=478E8A1A5B8FC5ED498E3DD5B184CA36B03ED9C9C00A59EC98D47758A26A37AB
```

## SD cold-start acceptance

The user installed the 17:37 build and reported normal SD-boot display operation. That build and the final rebuilt image are byte-identical, both with SHA-256 `478E8A1A5B8FC5ED498E3DD5B184CA36B03ED9C9C00A59EC98D47758A26A37AB`.
