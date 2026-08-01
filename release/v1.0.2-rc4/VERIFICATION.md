# v1.0.2-rc4 verification record

Verification date: 2026-08-01

## Artifact identity

The packaged application ELF is byte-identical to
`adctestvitis/adctestps/build_lowfreq_fix/adctestps.elf`:

```text
5B4AF88CDBB97810C202BF96A6CAB5A3699B126A2C55E9576E38B3B0E212B435
```

The packaged HMI project is byte-identical to the operator-compiled project:

```text
A371184241DB185856F779C75609618F78BFF3A841163382116C392E6CC52AB7
```

The HMI editor-generated TFT image packaged after compilation is:

```text
C0C0D781850C6E1B689A1C096A8E8CAABD7BB9F2FF2FD4DD189BFB0256400D3B
```

The quiet FSBL, routed PL bitstream, XSA, BIF, Bootgen script, and SD writer
are byte-identical to v1.0.2-rc3:

```text
fsbl.elf             AA567C93F5AEA63E4F6786037AE882B9C987DA04497157CF57473AA05360D34E
design_1_wrapper.bit 85B581A4DA0164229A62A417BCA432216D7C64555204B350E6C4CBDD9BA44597
design_1_wrapper.xsa 5C5D21D399E183126B9DE727CC48DA72BD1C797AA88F3ABEF32F67BEA4A97A58
```

The FSBL symbol table contains `InitSD`, `SDAccess`, `ps7_init`, and
`ps7_post_config`.

## HMI contract

The project used for this build has the following page0 waveform contract:

```text
component: s0
object ID: 4
channel count: 1
firmware channel: 0
geometry: x=45, y=110, width=506, height=265
```

Firmware transfers exactly 506 raw samples with `addt 4,0,506` after clearing
the component with `cle 4,0`.

## Software verification

The complete host test script passed:

```text
PS_ANALYSIS_PASS
REQUIREMENT_SWEEP_PASS failed_cases=0
INTERFERENCE_TEST_PASS failures=0
DISPLAY_WAVEFORM_PASS failures=0 points=506
DISPLAY_PORT_PASS failures=0
```

The display regression covers H4, H8, H16, H49, and H50 in both one-period
and three-period modes. Every dominant display bin matched the expected bin.
After a synthetic capture-phase shift, the maximum displayed byte-code delta
was 1.

The fake-UART protocol regression compiles the production `display_port.c`
with strict C11 warnings and verifies:

```text
touch packet before FE -> no command bytes inside the 506-byte payload
missing FE -> transparent mode drained, next transaction byte-exact
missing FD -> command boundary restored, next transaction byte-exact
confirmed page1 -> no page0 addt command
confirmed page0 -> waveform transfer resumes
```

The Cortex-A9 application was rebuilt with `--clean-first`, `-O2`,
`-Wall -Wextra`, Cortex-A9 VFPv3, and the hard-float ABI:

```text
text=100880 data=2332 bss=244080
entry=0x00100000
ELF SHA-256=5B4AF88CDBB97810C202BF96A6CAB5A3699B126A2C55E9576E38B3B0E212B435
```

All application sources compiled without warnings. The only build warning is
the inherited linker warning for an RWX LOAD segment.

An independent read-only review initially rejected the unguarded transparent
transfer because touch callbacks could transmit during the payload and timeout
paths could remain out of sync. After the explicit transfer state machine,
deferred page query, bounded recovery, and protocol tests were added, the same
reviewer returned `APPROVED` with no blocking or suggested code changes.

## Boot image verification

The unwrapped Vivado 2025.2 Bootgen executable reported
`Bootimage generated successfully`. Its immediate readback found:

```text
fsbl.elf: one partition
design_1_wrapper.bit: one partition
adctestps.elf: two partitions
application code: load=0x00100000 exec=0x00100000
application data: load=0x00118000
```

```text
BOOT.BIN size: 4262920 bytes
BOOT.BIN SHA-256:
53E2745F25E0A20010E880BDAFD04E0DC620B2FE1B7C9F917DF07B8B78DC7A80
```

PowerShell `Get-FileHash` and an independent `certutil -hashfile` invocation
returned the same BOOT digest.

## Physical verification

The updated HMI project has been compiled and downloaded to the screen by the
operator, and its generated TFT image is included in this release. This exact
rc4 `BOOT.BIN` has not yet been copied to SD and cold-boot tested on the target.
The physical acceptance items are page0 waveform refresh, one/three-period
switching, page switching, touch response, and recovery after continuous
measurement updates.
