#!/usr/bin/env python3
"""Re-level the factory bank from a preset-audit --trims report.

    cmake --build build --target preset-audit
    ./build/preset-audit_artefacts/Release/preset-audit --trims > trims.txt
    python3 scripts/level_presets.py trims.txt          # report only
    python3 scripts/level_presets.py trims.txt --write  # rewrite the bank

Each preset carries an `output.trim.db` trim (v0.15; it was `output.master.db`
until CC 7 — which every station song restates — was found to overwrite it) so
it peaks at the target when played. The audit reports `name|measured peak
dB|trim dB it rendered with|rms dB|category` (older reports have only the
first three fields), so the new trim is simply trim + (target - peak), clamped
to the parameter range. Presets that hit a clamp are listed: they cannot reach
the target and need their patch changed, not their trim.

Options:
  --only "A,B"   re-level only these presets; every other trim is left as is.
  --rms-floor    level the RMS_FLOOR presets by the blend rule below. Without
                 it they are SKIPPED (never pulled back to the plain peak target).

RMS floor (v0.16). Levelling by peak alone leaves short, spiky patches (stabs,
mallets, plucks) far quieter in a mix than their category neighbours: a 120 ms
stab peaks like a pad but carries 10-20 dB less RMS. For the presets listed in
RMS_FLOOR the trim is raised from the plain peak target until the preset's RMS
is within RMS_GAP_DB of its category's median RMS (the median of the OTHER
presets in that category, from the same report) or its peak reaches
RMS_FLOOR_PEAK_CAP_DB, whichever comes first. It never ends below the plain
peak target. The set is chosen by hand (presets more than 6 dB under their
category median, plus the v0.16 plucks named in the CHANGELOG); `--rms-floor`
also prints every preset more than 6 dB under its median so the set can be
reviewed. Needs a five-field report.

Run the audit again afterwards — one pass is not exact, because the engine's
soft knee is mildly nonlinear near the ceiling.
"""
import re
import statistics
import sys
from pathlib import Path

TARGET_DB = -6.0
MASTER_MIN, MASTER_MAX = -48.0, 24.0   # output.trim.db's range
BANK = Path(__file__).resolve().parent.parent / "source/plugin/FactoryPresets.cpp"

RMS_GAP_DB = 4.0                 # aim: RMS within this of the category median
RMS_FLOOR_PEAK_CAP_DB = -3.2     # never past this peak (the audit's hot line is -3)
RMS_FLOOR_FLAG_DB = 6.0          # --rms-floor lists presets this far under
RMS_FLOOR = {
    # v0.16: every preset > 6 dB under its category median RMS at the time,
    # plus Island Marimba, Pizzicato Pop and Island Pluck (named by review).
    "Punchy Pluck",
    "Poly Comp Stab",
    "Glass Clang",
    "Voice Round-Robin Keys",
    "Resonant Blip",
    "Ladder Brass Stab",
    "Poly Stab",
    "FM Bass Slap",
    "FM Clav",
    "Trance Pluck",
    "Rolling Bass",
    "Offbeat Bass",
    "Psy Bass",
    "Acid Roller",
    "Organ Stab",
    "House Pluck",
    "Jackin Bass",
    "Garage Chord",
    "Industrial Stab",
    "Bleep Lead",
    "Techno Zap",
    "Chase Bass",
    "Laser Zap",
    "Pluck Bass",
    "Clav Funk",
    "Rave Pluck",
    "Big Room Stab",
    "Nu Trance Bass",
    "Pumping Pluck",
    "Hard Dance Stab",
    "Saltwater Pluck",
    "Tine Glass EP",
    "Deep Garage Chord",
    "Anthem Pluck",
    "Ibiza Stab",
    "Wurli Drive EP",
    "Cut-Up Stab",
    "Island Marimba",
    "Pizzicato Pop",
    "Island Pluck",
}


def option(name):
    for i, a in enumerate(sys.argv):
        if a == name and i + 1 < len(sys.argv):
            return sys.argv[i + 1]
    return None


def main() -> int:
    only_arg = option("--only")
    args = [a for i, a in enumerate(sys.argv[1:], 1)
            if not a.startswith("--") and sys.argv[i - 1] != "--only"]
    write = "--write" in sys.argv
    rms_floor = "--rms-floor" in sys.argv
    only = {n.strip() for n in only_arg.split(",")} if only_arg else None
    if not args:
        print(__doc__)
        return 2

    rows = {}
    for line in Path(args[0]).read_text().splitlines():
        fields = line.split("|")
        if len(fields) == 3:
            rows[fields[0]] = (float(fields[1]), float(fields[2]), None, None)
        elif len(fields) == 5:
            rows[fields[0]] = (float(fields[1]), float(fields[2]), float(fields[3]), fields[4])
    if not rows:
        print("no trim lines found")
        return 1

    by_category = {}
    for name, (_, _, rms, cat) in rows.items():
        if rms is not None:
            by_category.setdefault(cat, []).append((name, rms))

    def category_median(name):
        _, _, _, cat = rows[name]
        others = [r for n, r in by_category.get(cat, []) if n != name]
        return statistics.median(others) if others else None

    if rms_floor:
        if any(r[2] is None for r in rows.values()):
            print("--rms-floor needs a five-field report (preset-audit --trims, v0.16+)")
            return 1
        print(f"presets more than {RMS_FLOOR_FLAG_DB:.0f} dB under their category median RMS:")
        for name, (peak, _, rms, cat) in rows.items():
            med = category_median(name)
            if med is not None and rms < med - RMS_FLOOR_FLAG_DB:
                tag = "" if name in RMS_FLOOR else "   (not in RMS_FLOOR)"
                print(f"  {name:<24} {cat:<9} rms {rms:6.1f}  median {med:6.1f}{tag}")

    source = BANK.read_text()
    clamped, changed = [], 0

    for name, (peak, master, rms, cat) in sorted(rows.items()):
        if only is not None and name not in only:
            continue
        new = master + (TARGET_DB - peak)
        if name in RMS_FLOOR:
            if not rms_floor:
                print(f"  - {name}: RMS-floor preset, skipped (re-level it with --rms-floor)")
                continue
            med = category_median(name)
            to_rms = (med - RMS_GAP_DB) - rms
            to_cap = RMS_FLOOR_PEAK_CAP_DB - peak
            new = master + max(TARGET_DB - peak, min(to_rms, to_cap))
        if new < MASTER_MIN or new > MASTER_MAX:
            clamped.append((name, new))
            new = max(MASTER_MIN, min(MASTER_MAX, new))
        new = round(new, 1)
        if abs(new - master) < 0.05:
            continue

        # Rewrite the p::presetTrim entry inside this preset's block only. The block
        # runs from its name to the start of the next preset (or end of bank),
        # which is unambiguous where brace counting is not.
        anchor = source.find(f'{{ "{name}",')
        if anchor < 0:
            print(f"  ! {name}: not found in the bank")
            continue
        nxt = re.search(r'\n\s*\{ "', source[anchor + 1:])
        end = anchor + 1 + nxt.start() if nxt else len(source)
        block = source[anchor:end]
        patched, hits = re.subn(r"\{\s*p::presetTrim,\s*-?[0-9.]+f?\s*\}",
                                f"{{ p::presetTrim, {new}f }}", block)
        if hits != 1:
            print(f"  ! {name}: {hits} presetTrim entries, expected 1")
            continue
        source = source[:anchor] + patched + source[end:]
        changed += 1
        print(f"  {name:<24} {master:+6.1f} -> {new:+6.1f} dB")

    print(f"{len(rows)} presets, {changed} trims changed, target {TARGET_DB} dBFS")
    if clamped:
        print(f"{len(clamped)} could not reach the target (master range "
              f"{MASTER_MIN}..{MASTER_MAX} dB):")
        for name, wanted in sorted(clamped, key=lambda x: -abs(x[1])):
            print(f"  {name:<24} wanted {wanted:+.1f} dB")

    if write:
        BANK.write_text(source)
        print(f"wrote {BANK}")
    else:
        print("(dry run — pass --write to rewrite the bank)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
