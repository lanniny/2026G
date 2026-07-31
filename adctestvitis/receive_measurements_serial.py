#!/usr/bin/env python3
"""Receive and print Zynq periodic-signal measurements over USB-UART."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field

import serial


@dataclass
class Frame:
    number: int = 0
    status: str = ""
    adc_min: int = 0
    adc_max: int = 0
    clipped: int = 0
    f0_hz: int | None = None
    upp_uv: int = 0
    rms_ac_uv: int = 0
    rms_total_uv: int = 0
    dc_uv: int = 0
    snr_centi_db: int = 0
    components: list[tuple[int, int, int, int]] = field(
        default_factory=list
    )


def print_frame(frame: Frame) -> None:
    print(f"\nFrame {frame.number}: {frame.status}")
    print(
        f"  ADC codes: {frame.adc_min}..{frame.adc_max}, "
        f"clipped samples: {frame.clipped}"
    )

    if frame.f0_hz is None:
        return

    print(
        f"  f0={frame.f0_hz / 1000:.3f} kHz, "
        f"Upp={frame.upp_uv / 1000:.3f} mV, "
        f"RMS(ac)={frame.rms_ac_uv / 1000:.3f} mV"
    )
    print(
        f"  RMS(total)={frame.rms_total_uv / 1000:.3f} mV, "
        f"DC={frame.dc_uv / 1000:.3f} mV, "
        f"SNR~{frame.snr_centi_db / 100:.2f} dB"
    )

    for harmonic, frequency_hz, amplitude_uv, phase_mdeg in frame.components:
        print(
            f"  H{harmonic}: {frequency_hz / 1000:.3f} kHz, "
            f"Apeak={amplitude_uv / 1000:.3f} mV, "
            f"phase={phase_mdeg / 1000:.3f} deg"
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="Serial port, for example COM5")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    frame: Frame | None = None
    with serial.Serial(args.port, args.baud, timeout=1) as connection:
        print(f"Listening on {args.port} at {args.baud} baud...")

        while True:
            raw_line = connection.readline()
            if not raw_line:
                continue

            line = raw_line.decode("ascii", errors="replace").strip()
            if not line:
                continue

            if not line.startswith("@"):
                print(line)
                continue

            fields = line.split(",")
            record = fields[0]

            try:
                if record == "@MEAS_BEGIN":
                    frame = Frame(number=int(fields[1]))
                elif record == "@STATUS" and frame is not None:
                    frame.status = ",".join(fields[2:])
                elif record == "@ADC" and frame is not None:
                    frame.adc_min = int(fields[1])
                    frame.adc_max = int(fields[2])
                    frame.clipped = int(fields[3])
                elif record == "@PARAM" and frame is not None:
                    frame.f0_hz = int(fields[1])
                    frame.upp_uv = int(fields[2])
                    frame.rms_ac_uv = int(fields[3])
                    frame.rms_total_uv = int(fields[4])
                    frame.dc_uv = int(fields[5])
                    frame.snr_centi_db = int(fields[6])
                elif record == "@COMP" and frame is not None:
                    frame.components.append(
                        (
                            int(fields[1]),
                            int(fields[2]),
                            int(fields[3]),
                            int(fields[4]),
                        )
                    )
                elif record == "@MEAS_END" and frame is not None:
                    print_frame(frame)
                    frame = None
                elif record in {
                    "@DEVICE",
                    "@UART",
                    "@UNITS",
                    "@CONFIG",
                    "@WARNING",
                    "@READY",
                }:
                    print(line)
            except (IndexError, ValueError) as error:
                print(f"Malformed record: {line!r}: {error}")


if __name__ == "__main__":
    main()
