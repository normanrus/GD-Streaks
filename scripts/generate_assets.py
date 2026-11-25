#!/usr/bin/env python3
"""Generates the PNG sprites used by GD Streaks.

Run with Pillow installed:  python3 scripts/generate_assets.py
The output is committed to resources/, so building the mod does not need Python.
"""

import math
import os

from PIL import Image, ImageDraw, ImageFilter

SS = 4  # supersampling factor for smooth edges
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "resources")


# ---------------------------------------------------------------- helpers

def rgba(hex_color, alpha=255):
    hex_color = hex_color.lstrip("#")
    return (
        int(hex_color[0:2], 16),
        int(hex_color[2:4], 16),
        int(hex_color[4:6], 16),
        alpha,
    )


def bezier(p0, c1, c2, p1, steps=64):
    pts = []
    for i in range(steps + 1):
        t = i / steps
        u = 1 - t
        x = u**3 * p0[0] + 3 * u * u * t * c1[0] + 3 * u * t * t * c2[0] + t**3 * p1[0]
        y = u**3 * p0[1] + 3 * u * u * t * c1[1] + 3 * u * t * t * c2[1] + t**3 * p1[1]
        pts.append((x, y))
    return pts


def vertical_gradient(size, stops):
    """stops: [(position0..1, (r,g,b,a)), ...] sorted by position."""
    w, h = size
    img = Image.new("RGBA", (1, h))
    px = img.load()
    for y in range(h):
        t = 1.0 - (y / max(1, h - 1))  # 0 at bottom, 1 at top
        color = stops[-1][1]
        for i in range(len(stops) - 1):
            p0, c0 = stops[i]
            p1, c1 = stops[i + 1]
            if p0 <= t <= p1:
                f = (t - p0) / max(1e-6, p1 - p0)
                color = tuple(int(c0[k] + (c1[k] - c0[k]) * f) for k in range(4))
                break
        else:
            if t < stops[0][0]:
                color = stops[0][1]
        px[0, y] = color
    return img.resize((w, h), Image.NEAREST)


def radial_glow(size, color, power=2.2):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = img.load()
    half = size / 2.0
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5 - half) / half
            dy = (y + 0.5 - half) / half
            r = math.sqrt(dx * dx + dy * dy)
            if r >= 1.0:
                continue
            a = int(color[3] * ((1.0 - r) ** power))
            px[x, y] = (color[0], color[1], color[2], max(0, min(255, a)))
    return img


def rounded_rect(size, radius, fill):
    w, h = size
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(img).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=fill)
    return img


def save(img, name, final_size):
    img = img.resize(final_size, Image.LANCZOS)
    path = os.path.join(OUT, name)
    img.save(path, optimize=True)
    print(f"wrote {path} {img.size}")


# ---------------------------------------------------------------- flame shape

FLAME_PATH = [
    # (start, c1, c2, end) in normalized coordinates, y grows upwards
    ((0.50, 0.02), (0.22, 0.05), (0.10, 0.26), (0.20, 0.42)),
    ((0.20, 0.42), (0.30, 0.58), (0.42, 0.60), (0.44, 0.78)),
    ((0.44, 0.78), (0.46, 0.90), (0.50, 0.94), (0.53, 1.00)),
    ((0.53, 1.00), (0.58, 0.86), (0.70, 0.80), (0.76, 0.66)),
    ((0.76, 0.66), (0.86, 0.50), (0.86, 0.28), (0.72, 0.15)),
    ((0.72, 0.15), (0.64, 0.05), (0.57, 0.02), (0.50, 0.02)),
]


def flame_polygon(size, scale=1.0, y_offset=0.0, y_scale=1.0):
    pts = []
    for p0, c1, c2, p1 in FLAME_PATH:
        pts.extend(bezier(p0, c1, c2, p1))
    out = []
    for x, y in pts:
        nx = 0.5 + (x - 0.5) * scale
        ny = 0.5 + (y - 0.5) * scale * y_scale + y_offset
        out.append((nx * size, (1.0 - ny) * size))
    return out


def make_flame(size):
    big = size * SS
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).polygon(flame_polygon(big), fill=255)

    body = vertical_gradient(
        (big, big),
        [
            (0.0, rgba("ffc531")),
            (0.30, rgba("ff9a1f")),
            (0.68, rgba("ff6a00")),
            (1.0, rgba("f03a00")),
        ],
    )
    body.putalpha(mask)

    # Hot inner core, slightly smaller and pushed towards the base.
    core_mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(core_mask).polygon(
        flame_polygon(big, scale=0.50, y_offset=-0.17, y_scale=0.78), fill=255
    )
    core_mask = core_mask.filter(ImageFilter.GaussianBlur(big * 0.015))
    core = Image.new("RGBA", (big, big), rgba("fff0b8", 220))
    core.putalpha(core_mask.point(lambda v: int(v * 0.78)))

    # Cool glow right at the bottom of the flame.
    base = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(base).ellipse(
        [big * 0.33, big * 0.74, big * 0.67, big * 0.97], fill=rgba("fff3c4", 120)
    )
    base = base.filter(ImageFilter.GaussianBlur(big * 0.05))
    base.putalpha(Image.composite(base.getchannel("A"), Image.new("L", (big, big), 0), mask))

    flame = Image.alpha_composite(body, core)
    flame = Image.alpha_composite(flame, base)
    return flame.resize((size, size), Image.LANCZOS)


# ---------------------------------------------------------------- assets

def main():
    os.makedirs(OUT, exist_ok=True)

    flame = make_flame(256)
    save(flame, "flame.png", (256, 256))

    glow = radial_glow(256, rgba("ff9a3c", 255), power=2.0)
    save(glow, "glow.png", (256, 256))

    # Active day: warm gradient disc with a soft rim.
    disc = Image.new("RGBA", (128 * SS, 128 * SS), (0, 0, 0, 0))
    grad = vertical_gradient(
        (128 * SS, 128 * SS),
        [(0.0, rgba("f4511e")), (0.55, rgba("ff8a1f")), (1.0, rgba("ffb03a"))],
    )
    circle_mask = Image.new("L", (128 * SS, 128 * SS), 0)
    d = ImageDraw.Draw(circle_mask)
    d.ellipse([4 * SS, 4 * SS, (128 - 4) * SS, (128 - 4) * SS], fill=255)
    grad.putalpha(circle_mask)
    disc = grad
    ImageDraw.Draw(disc).ellipse(
        [4 * SS, 4 * SS, (128 - 4) * SS, (128 - 4) * SS],
        outline=rgba("ffd166", 140),
        width=2 * SS,
    )
    save(disc, "circle-on.png", (128, 128))

    # Inactive day: flat dark disc with a faint rim.
    off = Image.new("RGBA", (128 * SS, 128 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(off)
    d.ellipse([4 * SS, 4 * SS, (128 - 4) * SS, (128 - 4) * SS], fill=rgba("232328", 235))
    d.ellipse(
        [4 * SS, 4 * SS, (128 - 4) * SS, (128 - 4) * SS],
        outline=rgba("6a6a72", 120),
        width=2 * SS,
    )
    save(off, "circle-off.png", (128, 128))

    # Golden highlight ring around "today".
    ring = Image.new("RGBA", (128 * SS, 128 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(ring)
    d.ellipse(
        [5 * SS, 5 * SS, (128 - 5) * SS, (128 - 5) * SS],
        outline=rgba("ffd166", 235),
        width=4 * SS,
    )
    ring = ring.filter(ImageFilter.GaussianBlur(SS * 0.6))
    save(ring, "ring.png", (128, 128))

    # Pill background for the main menu badge (9-slice, insets 18, capsule ends).
    pill = Image.new("RGBA", (96 * SS, 36 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(pill)
    d.rounded_rectangle(
        [0, 0, 96 * SS - 1, 36 * SS - 1], radius=18 * SS, fill=rgba("14141a", 215)
    )
    d.rounded_rectangle(
        [SS, SS, 95 * SS, 35 * SS], radius=17 * SS, outline=rgba("ffffff", 36), width=SS
    )
    top = Image.new("RGBA", (96 * SS, 36 * SS), (0, 0, 0, 0))
    ImageDraw.Draw(top).rounded_rectangle(
        [4 * SS, 2 * SS, 92 * SS, 17 * SS], radius=14 * SS, fill=rgba("ffffff", 16)
    )
    pill = Image.alpha_composite(pill, top.filter(ImageFilter.GaussianBlur(2 * SS)))
    save(pill, "pill.png", (96, 36))

    # White capsule bar, tinted in game (9-slice, insets 7).
    bar = rounded_rect((96 * SS, 14 * SS), 7 * SS, rgba("ffffff", 255))
    save(bar, "bar.png", (96, 14))

    # Plain white pixel used for dividers and thin accents.
    save(Image.new("RGBA", (8 * SS, 8 * SS), rgba("ffffff", 255)), "white.png", (8, 8))

    # Mod logo: flame on a dark rounded square.
    logo = Image.new("RGBA", (512 * SS, 512 * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(logo)
    d.rounded_rectangle(
        [0, 0, 512 * SS - 1, 512 * SS - 1], radius=96 * SS, fill=rgba("121216", 255)
    )
    halo = radial_glow(512 * SS, rgba("ff8a1f", 120), power=2.4)
    logo = Image.alpha_composite(logo, halo)
    flame_big = make_flame(512 * SS).resize((int(512 * SS * 0.72), int(512 * SS * 0.72)), Image.LANCZOS)
    offset = ((512 * SS - flame_big.width) // 2, int(512 * SS * 0.16))
    logo.alpha_composite(flame_big, offset)
    save(logo, "logo.png", (512, 512))


if __name__ == "__main__":
    main()
