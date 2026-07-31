# v1.0.2-rc3 verification record

Verification date: 2026-07-31

## Artifact identity

The packaged application ELF is byte-identical to
`adctestvitis/adctestps/build_lowfreq_fix/adctestps.elf`:

```text
7E0E7F20DF669EE395977A63FD96D89D9E327937ED603069E356898911F33E5F
```

The quiet FSBL, routed PL bitstream, XSA, BIF, Bootgen script, and SD writer
are byte-identical to v1.0.2-rc2. In particular:

```text
fsbl.elf             AA567C93F5AEA63E4F6786037AE882B9C987DA04497157CF57473AA05360D34E
design_1_wrapper.bit 85B581A4DA0164229A62A417BCA432216D7C64555204B350E6C4CBDD9BA44597
design_1_wrapper.xsa 5C5D21D399E183126B9DE727CC48DA72BD1C797AA88F3ABEF32F67BEA4A97A58
```

## Software and PL verification

```text
PS_ANALYSIS_PASS
REQUIREMENT_SWEEP_PASS failed_cases=0
INTERFERENCE_TEST_PASS failures=0
PL_SIM_PASS samples=8192 status=0x8a
```

All three host executables also compile cleanly with C11,
`-Wall -Wextra -Werror -pedantic`.

Dedicated boundary vectors verify:

```text
9.0 kHz equivalent -> f0 10 kHz, H2 20 kHz, PASS
251.0 kHz equivalent -> f0 250 kHz, H2 500 kHz, PASS
9.76 kHz equivalent -> f0 10 kHz, H2 20 kHz, PASS
250.24 kHz equivalent -> f0 250 kHz, H2 500 kHz, PASS
8.995/251.005 kHz tolerance endpoints -> accepted, PASS
8.994/251.006 kHz outside endpoints -> ANALYSIS_ERROR, PASS
```

At the 251 kHz equivalent boundary, H1 and H2 peak amplitudes were
0.100002 V and 0.025000 V for 0.100000 V and 0.025000 V references. Composite
Vpp was 0.216347 V against a 0.216348 V phase-aware reference.

The 160-vector official 500 Hz grid sweep remained at zero failures. Maximum
component-amplitude error was 0.015 mV and maximum composite-Vpp error was
0.306 mV.

The Cortex-A9 ELF was built with `-O2 -Wall -Wextra`, Cortex-A9 VFPv3
hard-float ABI:

```text
text=99336 data=2332 bss=244080
entry=0x00100000
```

The only inherited build warning is the linker RWX LOAD segment; there are no
application-source warnings.

## Boot image verification

Bootgen reported `Bootimage generated successfully` and read the generated
image back as the expected quiet FSBL, PL bitstream, and two application ELF
partitions. The application load and execution address is `0x00100000`; its
second data partition loads at `0x00118000`.

```text
BOOT.BIN size: 4261384 bytes
BOOT.BIN SHA-256:
E5EB2BB1F02735A9BC4CA2DDB21146FDD124B1483DE099A77BE045ABBB372027
```

PowerShell `Get-FileHash` and an independent `certutil -hashfile ... SHA256`
returned the same digest.

## Physical verification

Not yet performed for this exact rc3 BOOT image. v1.0.2-rc2 remains the
field-proven fallback. Use an adequate stable 5 V supply during rc3 testing;
the earlier random low-frequency readings were traced to inadequate power.
