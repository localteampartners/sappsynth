#!/usr/bin/env python3
"""Validate the factory preset bank against the real parameter layout.

Presets are written by hand, so a value can easily land outside its
parameter's range (the plugin would silently clamp it and the patch would
not sound as intended). This parses the ranges straight out of
PluginProcessor.cpp — one source of truth, no duplicated table — and checks
every value in FactoryPresets.cpp against them.

Exit 0 = clean, 1 = problems found. Wired into verify.sh.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
IDS = ROOT / "source/parameters/ParameterIds.h"
LAYOUT = ROOT / "source/plugin/PluginProcessor.cpp"
PRESETS = ROOT / "source/plugin/FactoryPresets.cpp"


def parse_ids():
    """symbol -> "osc1.wave" string id"""
    text = IDS.read_text()
    return dict(re.findall(r'inline constexpr const char\*\s+(\w+)\s*=\s*"([^"]+)"', text))


def parse_ranges(symbols):
    """symbol -> (lo, hi). Covers Float/Int/Choice params in the layout."""
    text = LAYOUT.read_text()
    ranges = {}

    # Choice params: p::subOctave -> 0..(count-1)
    for sym, arr in re.findall(r'ID\{p::(\w+),\s*1\},\s*"[^"]*",\s*'
                               r'juce::StringArray\s*\{([^}]*)\}', text):
        ranges[sym] = (0.0, float(len(re.findall(r'"', arr)) // 2 - 1))
    # Choice params referencing a named StringArray (waves, lfoShapes)
    named = {}
    for name, body in re.findall(r'const juce::StringArray (\w+)\s*\{([^}]*)\}', text):
        named[name] = len(re.findall(r'"', body)) // 2
    for sym, arr in re.findall(r'ID\{p::(\w+),\s*1\},\s*"[^"]*",\s*(\w+),\s*\d+\)', text):
        if arr in named:
            ranges[sym] = (0.0, float(named[arr] - 1))

    # Int params: Pi>(ID{p::x, 1}, "Name", lo, hi, default)
    for sym, lo, hi in re.findall(r'Pi>\(ID\{p::(\w+),\s*1\},\s*"[^"]*",\s*'
                                  r'(-?[\d.]+),\s*(-?[\d.]+),', text):
        ranges[sym] = (float(lo), float(hi))

    # Float params with an explicit NormalisableRange
    for sym, lo, hi in re.findall(r'ID\{p::(\w+),\s*1\},\s*"[^"]*",\s*'
                                  r'juce::NormalisableRange<float>\((-?[\d.]+f?),\s*(-?[\d.]+f?)',
                                  text):
        ranges[sym] = (float(lo.rstrip("f")), float(hi.rstrip("f")))

    # Float params using the logRange()/timeRange() helpers
    for sym, lo, hi in re.findall(r'ID\{p::(\w+),\s*1\},\s*"[^"]*",\s*'
                                  r'logRange\((-?[\d.]+)f?,\s*(-?[\d.]+)f?\)', text):
        ranges[sym] = (float(lo), float(hi))
    tr = re.search(r'timeRange\(\)\s*\{\s*juce::NormalisableRange<float>\s*'
                   r'range\(([\d.]+)f?,\s*([\d.]+)f?\)', text)
    time_lo, time_hi = (float(tr.group(1)), float(tr.group(2))) if tr else (0.001, 5.0)
    for sym in re.findall(r'adsr\(p::(\w+),\s*p::(\w+),\s*p::(\w+),\s*p::(\w+)', text):
        a, d, s, r = sym
        ranges[a] = ranges[d] = ranges[r] = (time_lo, time_hi)
        ranges[s] = (0.0, 1.0)

    # oscGroup() applies one shape to both oscillators
    for prefix in ("osc1", "osc2"):
        ranges[f"{prefix}Wave"] = (0.0, 3.0)
        ranges[f"{prefix}Octave"] = (-2.0, 2.0)
        ranges[f"{prefix}Semi"] = (-12.0, 12.0)
        ranges[f"{prefix}Fine"] = (-50.0, 50.0)
        ranges[f"{prefix}Pw"] = (0.05, 0.95)
        ranges[f"{prefix}Level"] = (0.0, 1.0)
    return ranges


def main():
    symbols = parse_ids()
    ranges = parse_ranges(symbols)
    text = PRESETS.read_text()

    problems = []
    names = {}
    categories = {}
    count = 0

    for name, category, body in re.findall(
            r'\{\s*"([^"]+)",\s*"([A-Z0-9 ]+)",\s*\{(.*?)\}\s*\}\s*,\s*\n', text, re.S):
        count += 1
        if name in names:
            problems.append(f"duplicate preset name: {name!r}")
        names[name] = category
        categories[category] = categories.get(category, 0) + 1

        # A preset is audibly dead if it mutes osc1 (the only source that is
        # on by default) without opening another one.
        sets = dict(re.findall(r'\{\s*p::(\w+),\s*(-?[\d.]+)f?\s*\}', body))
        sources = {"osc1Level": 1.0, "osc2Level": 0.0, "subLevel": 0.0, "noiseLevel": 0.0}
        if all(float(sets.get(s, d)) <= 0.0 for s, d in sources.items()):
            problems.append(f"{name}: every sound source is silent")
        if float(sets.get("ampSustain", 0.8)) <= 0.0 and float(sets.get("ampDecay", 0.2)) <= 0.002:
            problems.append(f"{name}: amp envelope decays to silence instantly")

        seen = set()
        for sym, value in re.findall(r'\{\s*p::(\w+),\s*(-?[\d.]+)f?\s*\}', body):
            if sym in seen:
                problems.append(f"{name}: sets p::{sym} twice")
            seen.add(sym)
            if sym not in symbols:
                problems.append(f"{name}: unknown parameter p::{sym}")
                continue
            if sym not in ranges:
                problems.append(f"{name}: no range known for p::{sym}")
                continue
            lo, hi = ranges[sym]
            v = float(value)
            if v < lo - 1e-6 or v > hi + 1e-6:
                problems.append(f"{name}: p::{sym} = {v} outside [{lo}, {hi}]")

    print(f"{count} presets, {len(categories)} categories")
    for category, n in sorted(categories.items(), key=lambda kv: -kv[1]):
        print(f"  {category:<10} {n}")

    if problems:
        print(f"\n{len(problems)} problem(s):")
        for p in problems:
            print(f"  - {p}")
        return 1
    print("\npreset bank OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
