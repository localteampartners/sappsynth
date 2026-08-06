#!/usr/bin/env python3
"""Renders SappSynth's vintage UI assets into assets/ as PNGs.

Everything is generated — walnut cabinet wood, matte panel texture, and
photoreal knob filmstrips (fixed top-left lighting, rotating pointer/ridges,
101 frames from -135 deg to +135 deg), plus decorative screws.

Run from repo root with the asset venv:
    .venv-assets/bin/python scripts/generate_ui_assets.py
"""
import os

import numpy as np
from PIL import Image, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "assets")
FRAMES = 101
LIGHT_ANGLE = np.deg2rad(-135.0)  # light from upper-left


def save(rgba, path):
    img = Image.fromarray(rgba.astype(np.uint8), "RGBA")
    img.save(path, optimize=True)
    print(f"wrote {path} ({img.width}x{img.height})")


def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# --------------------------------------------------------------- walnut ----
def walnut(w=256, h=1024, seed=7):
    rng = np.random.default_rng(seed)
    y = np.linspace(0, 1, h)[:, None] * np.ones((1, w))
    x = np.linspace(0, 1, w)[None, :] * np.ones((h, 1))

    # Grain bands run vertically with slow wobble; cathedral-ish arcs from a
    # radial term. Turbulence from summed sines (cheap, tileable enough).
    wobble = (0.005 * np.sin(2 * np.pi * (y * 1.3 + 0.3)) +
              0.003 * np.sin(2 * np.pi * (y * 3.7 + 1.1)) +
              0.0015 * np.sin(2 * np.pi * (y * 8.3 + 2.0)))
    bands = np.sin(2 * np.pi * ((x + wobble) * 6.0)) * 0.5 + 0.5
    bands2 = np.sin(2 * np.pi * ((x + wobble * 1.4) * 2.3 + 0.7)) * 0.5 + 0.5
    fine = np.sin(2 * np.pi * ((x + wobble * 1.7) * 41.0)) * 0.5 + 0.5
    grain = 0.45 * bands + 0.30 * bands2 + 0.25 * fine
    grain = grain ** 1.6

    noise = rng.normal(0, 1, (h, w))
    noise = np.array(Image.fromarray(((noise - noise.min()) / (np.ptp(noise)) * 255).astype(np.uint8))
                     .filter(ImageFilter.GaussianBlur(1.2)), dtype=np.float64) / 255.0

    dark = np.array([52, 30, 17], dtype=np.float64)
    mid = np.array([106, 62, 32], dtype=np.float64)
    light = np.array([132, 84, 47], dtype=np.float64)

    t = np.clip(grain * 0.85 + noise * 0.3, 0, 1)
    rgb = dark[None, None, :] + (light - dark)[None, None, :] * t[:, :, None]
    # occasional darker streaks
    streaks = smoothstep(0.96, 1.0, np.sin(2 * np.pi * ((x + wobble * 2.2) * 6.0 + 0.25)) * 0.5 + 0.5)
    rgb = rgb * (1.0 - 0.35 * streaks[:, :, None])
    # subtle sheen: vertical highlight band (satin lacquer)
    sheen = np.exp(-((x - 0.35) ** 2) / 0.08)
    rgb = rgb + 13.0 * sheen[:, :, None] * (mid / mid.max())[None, None, :]

    rgba = np.dstack([np.clip(rgb, 0, 255), np.full((h, w), 255.0)])
    return rgba


# ---------------------------------------------------------------- panel ----
def panel(w=512, h=512, seed=11):
    rng = np.random.default_rng(seed)
    base = np.full((h, w), 30.0)
    n = rng.normal(0, 1, (h, w))
    n = np.array(Image.fromarray(((n - n.min()) / np.ptp(n) * 255).astype(np.uint8))
                 .filter(ImageFilter.GaussianBlur(0.7)), dtype=np.float64) / 255.0
    crinkle = rng.normal(0, 1, (h // 4, w // 4))
    crinkle = np.array(Image.fromarray(((crinkle - crinkle.min()) / np.ptp(crinkle) * 255).astype(np.uint8))
                       .resize((w, h), Image.BILINEAR), dtype=np.float64) / 255.0
    v = base + n * 9.0 + crinkle * 7.0 - 6.0
    rgb = np.dstack([v * 0.98, v * 0.99, v * 1.06])
    rgba = np.dstack([np.clip(rgb, 0, 255), np.full((h, w), 255.0)])
    return rgba


# ----------------------------------------------------------------- knob ----
def knob_frame(big, body, dark_pointer, angle):
    """One supersampled frame. body: base RGB; angle: pointer angle (radians,
    0 = up). Fixed light from upper-left; ridges rotate with the knob."""
    yy, xx = np.mgrid[0:big, 0:big].astype(np.float64)
    c = big / 2.0
    dx, dy = xx - c, yy - c
    r = np.sqrt(dx * dx + dy * dy)
    theta = np.arctan2(dy, dx)  # 0 = +x, increases clockwise in image coords

    skirt_r = 0.470 * big
    skirt_top = 0.400 * big
    cap_r = 0.300 * big
    cap_face = 0.262 * big
    aa = 1.5

    img = np.zeros((big, big, 3))
    alpha = 1.0 - smoothstep(skirt_r - aa, skirt_r + aa, r)

    body = np.array(body, dtype=np.float64)

    # --- skirt with ridges (rotate with knob) ---
    ridge_phase = (theta + angle) * 30.0
    ridges = 0.5 + 0.5 * np.cos(ridge_phase)
    # directional light on the ridged edge
    edge_light = 0.75 + 0.45 * np.clip(np.cos(theta - LIGHT_ANGLE), -1, 1)
    skirt_shade = (0.58 + 0.20 * ridges) * edge_light
    skirt_zone = smoothstep(skirt_top - aa, skirt_top + aa, r) * (1.0 - smoothstep(skirt_r - aa, skirt_r + aa, r))
    img += (body[None, None, :] * skirt_shade[:, :, None]) * skirt_zone[:, :, None]

    # --- skirt top face (flat ring, slight radial falloff) ---
    ring_zone = smoothstep(cap_r - aa, cap_r + aa, r) * (1.0 - smoothstep(skirt_top - aa, skirt_top + aa, r))
    ring_shade = 0.92 - 0.10 * smoothstep(cap_r, skirt_top, r)
    ring_shade = ring_shade * (0.94 + 0.10 * np.clip(np.cos(theta - LIGHT_ANGLE), -1, 1))
    img += (body[None, None, :] * ring_shade[:, :, None]) * ring_zone[:, :, None]

    # --- cap side wall ---
    wall_zone = smoothstep(cap_face - aa, cap_face + aa, r) * (1.0 - smoothstep(cap_r - aa, cap_r + aa, r))
    wall_shade = 0.55 + 0.30 * np.clip(np.cos(theta - LIGHT_ANGLE), -1, 1)
    img += (body[None, None, :] * wall_shade[:, :, None]) * wall_zone[:, :, None]

    # --- cap face: domed, lit off-center toward the light ---
    lx = c + 0.09 * big * np.cos(LIGHT_ANGLE)
    ly = c + 0.09 * big * np.sin(LIGHT_ANGLE)
    dome_d = np.sqrt((xx - lx) ** 2 + (yy - ly) ** 2) / cap_face
    dome = 1.06 - 0.26 * np.clip(dome_d, 0, 1.4) ** 1.5
    face_zone = 1.0 - smoothstep(cap_face - aa, cap_face + aa, r)
    img += (body[None, None, :] * dome[:, :, None]) * face_zone[:, :, None]

    # --- specular highlight on the cap ---
    spec = np.exp(-(((xx - lx) ** 2 + (yy - ly) ** 2) / (0.005 * big * big)))
    img += 42.0 * spec[:, :, None] * face_zone[:, :, None]

    # --- pointer line (rotates), on cap + skirt top ---
    pa = angle - np.pi / 2.0  # 0 = up
    ang_diff = np.arctan2(np.sin(theta - pa), np.cos(theta - pa))
    line_halfwidth = 0.055
    in_line = (np.abs(ang_diff) < line_halfwidth) & (r > 0.06 * big) & (r < skirt_top * 0.96)
    line_soft = np.clip(1.0 - np.abs(ang_diff) / line_halfwidth, 0, 1) * in_line
    pointer_col = np.array(dark_pointer, dtype=np.float64)
    img = img * (1 - 0.92 * line_soft[:, :, None]) + pointer_col[None, None, :] * 0.92 * line_soft[:, :, None]

    # --- rim ambient occlusion ---
    ao = 1.0 - 0.25 * smoothstep(skirt_r * 0.86, skirt_r, r)
    img *= ao[:, :, None]

    return np.clip(img, 0, 255), np.clip(alpha, 0, 1) * 255.0


def knob_strip(size, body, pointer, path, shadow=True):
    S = 4
    big = size * S
    frames = []
    for f in range(FRAMES):
        angle = np.deg2rad(-135.0 + 270.0 * f / (FRAMES - 1))
        rgb, a = knob_frame(big, body, pointer, angle)
        frame = np.dstack([rgb, a])
        img = Image.fromarray(frame.astype(np.uint8), "RGBA").resize((size, size), Image.LANCZOS)

        if shadow:
            sh = Image.new("RGBA", (size, size), (0, 0, 0, 0))
            mask = Image.fromarray((a).astype(np.uint8), "L").resize((size, size), Image.LANCZOS)
            black = Image.new("RGBA", (size, size), (0, 0, 0, 110))
            sh.paste(black, (int(size * 0.03), int(size * 0.05)), mask)
            sh = sh.filter(ImageFilter.GaussianBlur(size * 0.03))
            sh.alpha_composite(img)
            img = sh
        frames.append(img)

    strip = Image.new("RGBA", (size, size * FRAMES))
    for i, fr in enumerate(frames):
        strip.paste(fr, (0, i * size))
    strip.save(path, optimize=True)
    print(f"wrote {path} ({strip.width}x{strip.height}, {FRAMES} frames)")


# ---------------------------------------------------------------- screw ----
def screw(size=48, seed=3):
    S = 4
    big = size * S
    yy, xx = np.mgrid[0:big, 0:big].astype(np.float64)
    c = big / 2.0
    dx, dy = xx - c, yy - c
    r = np.sqrt(dx * dx + dy * dy)
    theta = np.arctan2(dy, dx)
    R = 0.42 * big
    alpha = (1.0 - smoothstep(R - 2, R + 2, r)) * 255.0

    shade = 0.62 + 0.34 * np.clip(np.cos(theta - LIGHT_ANGLE), -1, 1)
    dome = 1.0 - 0.35 * np.clip(r / R, 0, 1) ** 2
    v = 165.0 * shade * dome
    # slot
    ang = np.deg2rad(37.0)
    slot_d = np.abs(dx * np.sin(ang) - dy * np.cos(ang))
    in_slot = (slot_d < 0.07 * big) & (r < R * 0.85)
    v = np.where(in_slot, v * 0.30, v)
    rgb = np.dstack([v, v, v * 1.04])
    img = Image.fromarray(np.dstack([np.clip(rgb, 0, 255), alpha]).astype(np.uint8), "RGBA")
    return img.resize((size, size), Image.LANCZOS)


def main():
    os.makedirs(OUT, exist_ok=True)
    save(walnut(), os.path.join(OUT, "wood_side.png"))
    save(panel(), os.path.join(OUT, "panel.png"))
    # Cream bakelite (main controls) and black bakelite (small/fx) knobs.
    knob_strip(96, (233, 224, 202), (40, 34, 28), os.path.join(OUT, "knob_cream.png"))
    knob_strip(96, (46, 45, 47), (225, 218, 200), os.path.join(OUT, "knob_black.png"))
    screw().save(os.path.join(OUT, "screw.png"), optimize=True)
    print("wrote", os.path.join(OUT, "screw.png"))


if __name__ == "__main__":
    main()
