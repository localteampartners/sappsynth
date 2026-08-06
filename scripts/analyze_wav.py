#!/usr/bin/env python3
"""Spectrum + waveform plot for a sapp-render WAV.

Usage: python3 scripts/analyze_wav.py render.wav [--fmax 20000] [--out plot.png]

Requires numpy; matplotlib optional (falls back to a text report).
"""
import argparse
import struct
import sys
import wave

import numpy as np


def read_wav(path: str):
    """Reads PCM or IEEE-float WAV without external deps."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        sys.exit(f"{path} is not a WAV file")
    pos = 12
    fmt = None
    samples = None
    while pos + 8 <= len(data):
        chunk_id = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if chunk_id == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif chunk_id == b"data":
            samples = body
        pos += 8 + size + (size & 1)
    if fmt is None or samples is None:
        sys.exit("missing fmt/data chunk")
    audio_format, channels, sample_rate, _, _, bits = fmt
    if audio_format == 3 and bits == 32:
        x = np.frombuffer(samples, dtype=np.float32)
    elif audio_format == 1 and bits == 16:
        x = np.frombuffer(samples, dtype=np.int16).astype(np.float32) / 32768.0
    else:
        sys.exit(f"unsupported format {audio_format}/{bits}")
    x = x.reshape(-1, channels)
    return x, sample_rate


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("wav")
    parser.add_argument("--fmax", type=float, default=20000.0)
    parser.add_argument("--out", default=None)
    args = parser.parse_args()

    x, sr = read_wav(args.wav)
    mono = x.mean(axis=1)
    n = min(len(mono), 1 << 16)
    seg = mono[len(mono) // 4:len(mono) // 4 + n]
    window = np.hanning(len(seg))
    spectrum = np.abs(np.fft.rfft(seg * window))
    freqs = np.fft.rfftfreq(len(seg), 1.0 / sr)
    db = 20 * np.log10(np.maximum(spectrum / spectrum.max(), 1e-9))

    print(f"{args.wav}: {len(mono)/sr:.2f}s @ {sr} Hz, "
          f"peak {np.abs(mono).max():.3f}, rms {np.sqrt((mono**2).mean()):.4f}")
    peak_bin = int(np.argmax(spectrum))
    print(f"dominant frequency: {freqs[peak_bin]:.1f} Hz")

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed — skipping plot")
        return

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 7))
    t = np.arange(min(len(mono), sr // 20)) / sr
    ax1.plot(t * 1000, mono[:len(t)], linewidth=0.7)
    ax1.set_xlabel("ms")
    ax1.set_title("waveform (first 50 ms)")
    mask = freqs <= args.fmax
    ax2.plot(freqs[mask], db[mask], linewidth=0.7)
    ax2.set_xlabel("Hz")
    ax2.set_ylabel("dB")
    ax2.set_ylim(-120, 3)
    ax2.set_title("spectrum")
    fig.tight_layout()
    out = args.out or args.wav.rsplit(".", 1)[0] + "_analysis.png"
    fig.savefig(out, dpi=110)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
