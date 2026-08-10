#!/usr/bin/env python3
"""Re-level the factory bank from a preset-audit --trims report.

    cmake --build build --target preset-audit
    ./build/preset-audit_artefacts/Release/preset-audit --trims > trims.txt
    python3 scripts/level_presets.py trims.txt          # report only
    python3 scripts/level_presets.py trims.txt --write  # rewrite the bank

Each preset carries an `output.master.db` trim so it peaks at the target when
played. The audit reports `name|measured peak dB|master dB it rendered with`,
so the new trim is simply master + (target - peak), clamped to the parameter
range. Presets that hit a clamp are listed: they cannot reach the target and
need their patch changed, not their trim.

Run the audit again afterwards — one pass is not exact, because the engine's
soft knee is mildly nonlinear near the ceiling.
"""
import re
import sys
from pathlib import Path

TARGET_DB = -6.0
MASTER_MIN, MASTER_MAX = -60.0, 6.0
BANK = Path(__file__).resolve().parent.parent / "source/plugin/FactoryPresets.cpp"


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    write = "--write" in sys.argv
    if not args:
        print(__doc__)
        return 2

    trims = {}
    for line in Path(args[0]).read_text().splitlines():
        if line.count("|") != 2:
            continue
        name, peak, master = line.split("|")
        trims[name] = (float(peak), float(master))
    if not trims:
        print("no trim lines found")
        return 1

    source = BANK.read_text()
    clamped, changed = [], 0

    for name, (peak, master) in sorted(trims.items()):
        new = master + (TARGET_DB - peak)
        if new < MASTER_MIN or new > MASTER_MAX:
            clamped.append((name, new))
            new = max(MASTER_MIN, min(MASTER_MAX, new))
        new = round(new, 1)
        if abs(new - master) < 0.05:
            continue

        # Rewrite the p::master entry inside this preset's block only. The block
        # runs from its name to the start of the next preset (or end of bank),
        # which is unambiguous where brace counting is not.
        anchor = source.find(f'{{ "{name}",')
        if anchor < 0:
            print(f"  ! {name}: not found in the bank")
            continue
        nxt = re.search(r'\n\s*\{ "', source[anchor + 1:])
        end = anchor + 1 + nxt.start() if nxt else len(source)
        block = source[anchor:end]
        patched, hits = re.subn(r"\{\s*p::master,\s*-?[0-9.]+f?\s*\}",
                                f"{{ p::master, {new}f }}", block)
        if hits != 1:
            print(f"  ! {name}: {hits} master entries, expected 1")
            continue
        source = source[:anchor] + patched + source[end:]
        changed += 1

    print(f"{len(trims)} presets, {changed} trims changed, target {TARGET_DB} dBFS")
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
