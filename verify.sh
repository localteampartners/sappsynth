#!/usr/bin/env bash
# verify.sh — fast feedback loop for sappsynth.
#
# Builds the framework-independent core + tests (no JUCE — that lives in the
# full `build/` tree) and runs the unit suite. Warm runs finish well under 60s.

set -e
cd "$(dirname "$0")"

echo "▶ configure (core, no plugin)"
cmake -B build-core -DSAPPSYNTH_BUILD_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null

echo "▶ build (strict warnings act as the lint pass)"
cmake --build build-core -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo "▶ tests"
./build-core/sappsynth_tests

echo "✓ verify passed"
