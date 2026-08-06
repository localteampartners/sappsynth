# SappLink CC-in (sappsynth side)

Implements the SappLink v1 contract (source of truth:
`~/apps/sapptune/sapplink/manifests/sappsynth.json`; protocol:
`~/apps/sapptune/sapplink/PROTOCOL.md`). Table lives in
`source/plugin/SappLinkCCMap.cpp`; a vendored manifest copy at
`tests/data/sapplink-manifest.json` is asserted against the table in
`tests/unit/test_sapplink.cpp`, so drift fails CI.

- CCs accepted on any channel; values slewed ~15 ms per step and applied via
  `setValueNotifyingHost` (same normalized path as host automation).
- CC 1 (mod wheel), CC 64 (sustain) and pitch bend are untouched.
- End-to-end proof: `SappUiShot --cctest` renders a CC 74 sweep through
  processBlock and asserts the spectral centroid brightens.

## Manifest corrections REQUIRED in sapptune (2026-08-06 manifest vs plugin)

The plugin implements its real parameter ranges; update the manifest to match:

| id | manifest has | must be | why |
|---|---|---|---|
| env.amp.release | [0.005, 10] log | **[0.001, 5] log** | plugin range is 0.001–5 s |
| voice.glide.s | [0, 2] log | **[0, 1] linear** | max is 1 s; log undefined at 0 |
| mixer.drive | [0, 1] linear | **[1, 8] log** | drive is a 1–8 gain factor |
| fx.delay.feedback | [0, 0.95] | **[0, 0.85]** | plugin caps at 0.85 |
| variation.driftCents | [0, 50] | **[0, 10]** | plugin range 0–10 cents |
| arp.rate.hz | [0.5, 30] log | **[0.5, 20] log** | plugin caps at 20 Hz |
| arp.gate | [0.05, 1] | **[0.05, 0.95]** | plugin caps at 0.95 |
| output.master.db | [-60, 6] | **[-40, 6]** | plugin floor is −40 dB |

All other rows (74/71/24/73/76/77/78/91/92/93/94/27, CC numbers and curves)
match as published.
