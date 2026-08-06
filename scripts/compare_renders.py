#!/usr/bin/env python3
"""Compare two sapp-render WAVs: RMS/peak/spectral difference.

Usage: python3 scripts/compare_renders.py a.wav b.wav

Exit code 0 if bit-identical, 1 otherwise (useful for determinism checks).
"""
import sys

import numpy as np

from analyze_wav import read_wav


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: compare_renders.py a.wav b.wav")
    a, sr_a = read_wav(sys.argv[1])
    b, sr_b = read_wav(sys.argv[2])
    if sr_a != sr_b:
        sys.exit(f"sample rates differ: {sr_a} vs {sr_b}")

    n = min(len(a), len(b))
    a, b = a[:n].mean(axis=1), b[:n].mean(axis=1)
    diff = a - b

    identical = bool(np.all(diff == 0.0)) and len(a) == len(b)
    rms_diff = float(np.sqrt((diff ** 2).mean()))
    peak_diff = float(np.abs(diff).max())

    spec_a = np.abs(np.fft.rfft(a[: 1 << 15] * np.hanning(min(n, 1 << 15))))
    spec_b = np.abs(np.fft.rfft(b[: 1 << 15] * np.hanning(min(n, 1 << 15))))
    spectral_diff = float(np.sqrt(((spec_a - spec_b) ** 2).mean()) / max(spec_a.max(), 1e-12))

    print(f"identical:      {identical}")
    print(f"rms diff:       {rms_diff:.2e}")
    print(f"peak diff:      {peak_diff:.2e}")
    print(f"spectral diff:  {spectral_diff:.2e}")
    sys.exit(0 if identical else 1)


if __name__ == "__main__":
    main()
