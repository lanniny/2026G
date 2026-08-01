# v1.0.2-rc4 dense waveform candidate

This candidate keeps the v1.0.2-rc3 signal-analysis behavior, including the
official 500 Hz frequency grid and the 1 kHz boundary tolerance. It changes
only the time-domain display path and packages the matching HMI project.

## Display changes

- Use the page0 waveform component `s0`, object ID 4, channel 0.
- Replace 48 individually transmitted line segments with one 506-point
  transparent waveform transfer.
- Resample the internal 1024-point reconstructed period to exactly the
  506-pixel waveform width.
- Support both one-period and three-period views without dropping samples.
- Anchor the displayed trace to the rising zero crossing of the fundamental,
  preventing acquisition phase from sliding the trace horizontally.
- Scale waveform bytes into 16 through 239, leaving stable vertical margins.
- Use the TJC `addt` protocol and require both `FE FF FF FF` ready and
  `FD FF FF FF` finished responses, each with a bounded timeout.
- Block every ASCII command while transparent transfer is active; touch and
  page events are deferred until the raw payload has finished.
- Recover a lost ready or finished marker by draining the remaining transparent
  byte count, restoring a command terminator, clearing stale RX state, and
  confirming the active page before retrying.

At 115200 baud, the 506-byte waveform payload takes about 44 ms on the wire.
The old line-command path sent substantially more command text and represented
the trace with only 48 segments. The new path therefore improves both refresh
latency and high-order harmonic visibility.

## Packaged HMI and TFT

The included HMI project is the exact file compiled and downloaded to the
screen by the operator. Its SHA-256 is:

```text
A371184241DB185856F779C75609618F78BFF3A841163382116C392E6CC52AB7
```

The matching editor-generated TFT screen image is also included:

```text
C0C0D781850C6E1B689A1C096A8E8CAABD7BB9F2FF2FD4DD189BFB0256400D3B
```

The firmware constants require page0 `s0.id=4`, at least one channel, and use
channel 0. The configured component is 506 by 265 pixels.

## Release status

- Host analysis, requirement sweep, interference, waveform, and UART protocol
  regressions pass.
- The Cortex-A9 application was rebuilt from a clean target build.
- Bootgen generated and read back the complete Zynq boot image.
- The quiet FSBL and routed LTC2208 PL artifacts are byte-identical to rc3.
- An independent read-only code review approved the transfer state machine,
  timeout recovery, page confirmation, and regression coverage.
- This exact rc4 SD image has not yet completed a physical cold-boot and screen
  acceptance test. Keep the field-proven rc2 image available as a fallback.

For an SD test, copy only `BOOT.BIN` to the root of a FAT32 SD card, select SD
boot mode, and power-cycle the complete device with the stable 5 V supply.
