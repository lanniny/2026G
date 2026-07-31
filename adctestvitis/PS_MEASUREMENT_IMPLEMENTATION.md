# PS periodic-signal measurement implementation

## Data path

The bare-metal Cortex-A9 application now processes each PL frame locally:

```text
AXI DMA, 8192 x 32-bit words
        |
        v
extract ADC1 low 12 bits
        |
        v
code-to-voltage calibration
        |
        v
DC removal + Hann window + 8192-point FFT
        |
        v
peak interpolation + harmonic-family fundamental selection
        |
        v
least-squares amplitude/phase fit
        |
        v
harmonic waveform reconstruction, Upp and RMS
```

The real sample rate is 4 MHz and the physical FFT-bin spacing is
488.28125 Hz.

## Important configuration

Edit `adctestps/src/app_config.h` before amplitude-accuracy testing:

- `APP_DEFAULT_ADC_ZERO_CODE`
- `APP_DEFAULT_ADC_SPAN_VPP`
- `APP_DEFAULT_FRONTEND_GAIN`

The checked-in defaults assume an offset-binary AD9226, 2 Vpp ADC span and
unity analog-front-end gain.  They are safe bring-up defaults, not a
competition calibration.

Edit `calibration_load_defaults()` in `calibration.c` with measured
frequency-response correction values.  A correction value is:

```text
known input peak amplitude / measured peak amplitude
```

The `is_calibrated` member intentionally remains zero in the default table,
so every UART measurement is marked `UNCALIBRATED`.

## USB-UART output

The PS UART connected to the board USB-UART connector is explicitly
configured as `115200 8N1`.  Every measurement is sent as an ASCII frame:

```text
@MEAS_BEGIN,frame
@STATUS,flags,status_text
@ADC,minimum_code,maximum_code,clipped_sample_count
@PARAM,f0_Hz,Upp_uV,RMS_AC_uV,RMS_TOTAL_uV,DC_uV,SNR_centi_dB
@RAW,Upp_uV,RMS_AC_uV
@COMPONENT_COUNT,count
@COMP,harmonic,frequency_Hz,amplitude_peak_uV,phase_mdeg
@MEAS_END,frame
```

No waveform sample points are transmitted.  The protocol sends only the
measured waveform parameters and detected frequency components.

On the computer, install pyserial and run:

```powershell
python -m pip install pyserial
python receive_measurements_serial.py COM5
```

Replace `COM5` with the USB-UART port shown in Windows Device Manager.

Set `APP_UART_STREAM_RAW` to `1` only when the existing MATLAB receiver is
needed.  Raw mode adds the `ADC_FRAME`/binary/`ADC_END` payload.  At 115200
baud that payload consumes most of the two-second response budget, so it is
disabled for normal operation.

## Display integration

`display_publish_measurement()` remains a weak no-op function.  A future
HDMI/LCD text interface can provide a strong implementation and use the same
parameter result structure.  The USB-UART protocol does not send or display
the waveform curve.

## Build

The Vitis application source list is in `adctestps/src/UserConfig.cmake`.
Optimization is set to `-O2`; do not use the old `-O0` setting for timing
tests.

After exporting the XSA produced by the current PL build:

1. Update the `adctestp` platform hardware specification.
2. Regenerate the standalone BSP.
3. Clean and rebuild `adctestps`.
4. Program the matching bitstream and ELF together.

The old ELF and IDE bitstream predate these changes and must not be used for
final testing.
