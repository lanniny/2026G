# v1.0.2-rc2 verification record

Verification date: 2026-07-31

## Artifact identity

The packaged application ELF is byte-identical to
`adctestvitis/adctestps/build_lowfreq_fix/adctestps.elf`:

```text
926852D71B89CA36C62D0E11AE1A7638CDC05321EBB975589F3F6A9E9AA5FDAE
```

The quiet FSBL and routed PL bitstream are byte-identical to the already
field-proven v1.0.1 lineage. The v1.0.1 BOOT image remained unchanged at:

```text
478E8A1A5B8FC5ED498E3DD5B184CA36B03ED9C9C00A59EC98D47758A26A37AB
```

## Software and PL verification

```text
PS_ANALYSIS_PASS
REQUIREMENT_SWEEP_PASS failed_cases=0
INTERFERENCE_TEST_PASS failures=0
PL_SIM_PASS samples=8192 status=0x8a
```

The deterministic requirement sweep covers:

- 10 kHz to 250 kHz fundamentals on the official 500 Hz grid;
- one or two harmonics through 500 kHz, including H49/H50;
- arbitrary phases and both 5 mV weak fundamentals and 5 mV weak harmonics;
- high-frequency sampling-clock offset and continuous-frequency fallback;
- pure low-frequency high-amplitude signals with no false harmonics;
- H1 plus only the two strongest harmonics;
- 9.9 kHz and 250.5 kHz out-of-range rejection.

Across the 160-vector full-grid sweep, the maximum frequency error was
1.031 Hz, the maximum component-amplitude error was 0.015 mV, and the maximum
composite-Vpp error against a 16,384-point reference was 0.306 mV.

The Cortex-A9 application was built with `-O2 -Wall -Wextra`, hard-float VFPv3:

```text
text=98952 data=2332 bss=244080
```

The only inherited build warning is the linker RWX LOAD segment; there are no
application-source warnings.

## Boot image verification

Bootgen reported `Bootimage generated successfully` and read the generated
image back as the expected quiet FSBL, PL bitstream and two application ELF
partitions. The application load and execution address remains `0x00100000`.

```text
BOOT.BIN size: 4261000 bytes
BOOT.BIN SHA-256:
9D7FFE73213BBDF76F62ADD4A1D2FB5A30E100AD70EB631B4A4F85FEBE9CAE3C
```

## Physical verification

The user tested this exact BOOT image on the Zynq/LTC2208 assembly. After
replacing an inadequate supply with an adequate stable supply, the previously
random low-frequency spectrum readings disappeared and measurements were
fully correct. The earlier slow refresh and random spectrum behavior are
therefore recorded as a power-integrity failure mode, not reproduced as an
algorithm regression under proper power.
