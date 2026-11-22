"""Placeholder for a Nintendo DS-compatible raw IMA-ADPCM encoder.

Planned input: headerless mono signed little-endian PCM16 at 22050 Hz.
Planned output: DS hardware-compatible header and continuous ADPCM data.
Not implemented; do not substitute block-framed WAV IMA-ADPCM output.
TODO: Verify predictor/index rules, nibble order, length, and padding
against authoritative hardware documentation and hardware tests.
"""


def main():
    raise SystemExit(
        "Not implemented: no audio was converted and no output was written."
    )


if __name__ == "__main__":
    main()
