#!/usr/bin/env python3
# Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
#
# maketropical.py - generate the "tropical house" and 80s-retro synthwave desktop
# wallpapers shipped to /var/background.  These are original, procedurally-drawn
# 24-bit PNGs (no third party assets, no licensing) sized to the default 1024x768
# framebuffer.  They exercise the objGFX PNG decode path (ogImage::Load ->
# DecodePNG via stb_image).
#
# Run from the repo root:  python3 tools/maketropical.py
# Output: tools/backgrounds/tropical-*.png and tools/backgrounds/synthwave-*.png

import math
import os
import random
import struct
import zlib

W, H = 1024, 768
OUTDIR = os.path.join(os.path.dirname(__file__), "backgrounds")


def clamp(v):
    return 0 if v < 0 else 255 if v > 255 else int(v)


def lerp(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


def grad(stops, t):
    """Sample a multi-stop vertical gradient at t in [0,1]."""
    if t <= stops[0][0]:
        return stops[0][1]
    if t >= stops[-1][0]:
        return stops[-1][1]
    for i in range(len(stops) - 1):
        p0, c0 = stops[i]
        p1, c1 = stops[i + 1]
        if p0 <= t <= p1:
            lt = (t - p0) / (p1 - p0) if p1 > p0 else 0.0
            return lerp(c0, c1, lt)
    return stops[-1][1]


def make_buf():
    return bytearray(W * H * 3)


def setpx(buf, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        i = (int(y) * W + int(x)) * 3
        buf[i] = clamp(c[0])
        buf[i + 1] = clamp(c[1])
        buf[i + 2] = clamp(c[2])


def stamp(buf, x, y, r, c):
    """Filled disc - used to rasterize the palm trunk/fronds."""
    r = max(1.0, r)
    for yy in range(int(y - r), int(y + r + 1)):
        dy = yy - y
        for xx in range(int(x - r), int(x + r + 1)):
            dx = xx - x
            if dx * dx + dy * dy <= r * r:
                setpx(buf, xx, yy, c)


def write_png(path, buf):
    stride = W * 3
    raw = bytearray()
    for y in range(H):
        raw.append(0)  # filter: none
        raw += buf[y * stride:(y + 1) * stride]

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))  # 8-bit RGB
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def draw_palm(buf, basex, basey, scale, flip):
    """A simple curved-trunk palm silhouette with drooping fronds."""
    dark = (10, 9, 20)
    trunk_h = int(380 * scale)
    lean = (44 if not flip else -44) * scale
    topx = basex + int(lean)
    topy = basey - trunk_h
    ctrlx = basex + int((8 if not flip else -8) * scale)
    ctrly = basey - int(trunk_h * 0.5)

    steps = 130
    for i in range(steps + 1):
        t = i / steps
        x = (1 - t) ** 2 * basex + 2 * (1 - t) * t * ctrlx + t * t * topx
        y = (1 - t) ** 2 * basey + 2 * (1 - t) * t * ctrly + t * t * topy
        stamp(buf, x, y, (1 - t) * 9 * scale + 2, dark)

    nf = 8
    for k in range(nf):
        ang = math.radians(-18 - k * (146.0 / (nf - 1)))
        if flip:
            ang = math.pi - ang
        length = 230 * scale
        for i in range(70):
            t = i / 69
            droop = math.sin(t * math.pi) * 46 * scale
            x = topx + math.cos(ang) * length * t
            y = topy + math.sin(ang) * length * t + droop
            stamp(buf, x, y, max(1.0, (1 - t) * 5 * scale), dark)


def sunset(palms):
    """Warm sunset over a calm sea; optional palm silhouettes."""
    buf = make_buf()
    horizon = int(H * 0.60)
    sky = [
        (0.00, (28, 22, 66)),
        (0.35, (96, 42, 120)),
        (0.60, (202, 74, 116)),
        (0.85, (255, 126, 86)),
        (1.00, (255, 206, 128)),
    ]
    sea_top = (66, 150, 150)
    sea_bot = (14, 44, 78)
    sunx, suny, sunr = W // 2, int(horizon * 0.80), int(H * 0.13)

    for y in range(H):
        if y < horizon:
            base = grad(sky, y / horizon)
            for x in range(W):
                c = base
                dx, dy = x - sunx, y - suny
                if abs(dx) < sunr * 2.5 and abs(dy) < sunr * 2.5:
                    d = math.sqrt(dx * dx + dy * dy)
                    if d < sunr:
                        c = (255, 232, 176)
                    elif d < sunr * 2.4:
                        g = 1 - (d - sunr) / (sunr * 1.4)
                        c = lerp(c, (255, 210, 150), max(0.0, g) * 0.55)
                setpx(buf, x, y, c)
        else:
            t = (y - horizon) / (H - horizon)
            base = lerp(sea_top, sea_bot, t)
            shimmer = lerp(base, (255, 255, 255), 0.05) if (y % 6) < 1 else base
            for x in range(W):
                if abs(x - sunx) < sunr * 0.9:
                    band = (math.sin(y * 0.4) + 1) * 0.5
                    setpx(buf, x, y, lerp(base, (255, 210, 150), 0.5 * band * (1 - t)))
                else:
                    setpx(buf, x, y, shimmer)

    if palms:
        draw_palm(buf, int(W * 0.17), H, 1.00, False)
        draw_palm(buf, int(W * 0.86), H, 1.18, True)
    return buf


def miami():
    """Retro synthwave sun + neon perspective grid."""
    buf = make_buf()
    horizon = int(H * 0.58)
    sky = [
        (0.00, (10, 12, 60)),
        (0.30, (70, 18, 120)),
        (0.55, (190, 30, 130)),
        (0.80, (255, 70, 120)),
        (1.00, (255, 150, 70)),
    ]
    sunx, suny, sunr = W // 2, int(horizon * 0.72), int(H * 0.16)

    for y in range(H):
        if y < horizon:
            base = grad(sky, y / horizon)
            for x in range(W):
                c = base
                dx, dy = x - sunx, y - suny
                if dx * dx + dy * dy < sunr * sunr:
                    st = (y - (suny - sunr)) / (2 * sunr)
                    sunc = lerp((255, 240, 120), (255, 80, 140), st)
                    # retro horizontal cut bands, denser toward the bottom
                    if y > suny and ((y - suny) % 10) < (2 + (y - suny) // (sunr // 3 + 1)):
                        c = base
                    else:
                        c = sunc
                setpx(buf, x, y, c)
        else:
            for x in range(W):
                setpx(buf, x, y, (20, 8, 40))

    grid = (255, 60, 160)
    cx = W // 2
    yy, spacing = float(H), 8.0
    while yy > horizon:
        for x in range(W):
            setpx(buf, x, int(yy), grid)
        yy -= spacing
        spacing *= 1.18
    for vx in range(-12, 13):
        bx = cx + vx * 120
        steps = 320
        for i in range(steps + 1):
            t = i / steps
            setpx(buf, bx + (cx - bx) * t, H + (horizon - H) * t, grid)
    return buf


def draw_grid(buf, horizon, color, vgap=120, nv=12):
    """Neon perspective grid: receding horizontals + converging verticals."""
    cx = W // 2
    yy, spacing = float(H), 8.0
    while yy > horizon:
        for x in range(W):
            setpx(buf, x, int(yy), color)
        yy -= spacing
        spacing *= 1.18
    for vx in range(-nv, nv + 1):
        bx = cx + vx * vgap
        steps = 340
        for i in range(steps + 1):
            t = i / steps
            setpx(buf, bx + (cx - bx) * t, H + (horizon - H) * t, color)


def draw_stars(buf, horizon, sunx, suny, sunr, n, seed):
    rnd = random.Random(seed)
    cols = [(255, 255, 255), (200, 220, 255), (255, 225, 245)]
    placed, tries = 0, 0
    while placed < n and tries < n * 6:
        tries += 1
        x = rnd.randint(0, W - 1)
        y = rnd.randint(0, int(horizon * 0.78))
        if (x - sunx) ** 2 + (y - suny) ** 2 < (sunr * 1.7) ** 2:
            continue
        c = rnd.choice(cols)
        setpx(buf, x, y, c)
        if rnd.random() < 0.18:
            setpx(buf, x + 1, y, c)
            setpx(buf, x - 1, y, c)
            setpx(buf, x, y + 1, c)
            setpx(buf, x, y - 1, c)
        placed += 1


def draw_mountains(buf, horizon, ridge_color, fill_color, seed):
    """Jagged neon-ridged mountain silhouette sitting on the horizon."""
    rnd = random.Random(seed)
    nv = 16
    xs = [int(i * (W - 1) / (nv - 1)) for i in range(nv)]
    ys = [horizon - rnd.randint(int(H * 0.05), int(H * 0.24)) for _ in range(nv)]
    glow = lerp(ridge_color, (255, 255, 255), 0.35)
    for s in range(nv - 1):
        x0, y0 = xs[s], ys[s]
        x1, y1 = xs[s + 1], ys[s + 1]
        span = max(1, x1 - x0)
        for x in range(x0, x1 + 1):
            peak = int(y0 + (y1 - y0) * ((x - x0) / span))
            for y in range(peak, horizon):
                setpx(buf, x, y, fill_color)
            setpx(buf, x, peak, ridge_color)
            setpx(buf, x, peak - 1, glow)


def draw_road(buf, horizon, vanish_x, edge_color):
    """A highway receding to the vanishing point, with a dashed centre line."""
    road = (12, 10, 28)
    half_bottom = int(W * 0.46)
    for y in range(horizon, H):
        t = (y - horizon) / (H - horizon)
        half = int(half_bottom * t)
        for x in range(vanish_x - half, vanish_x + half):
            setpx(buf, x, y, road)
        setpx(buf, vanish_x - half, y, edge_color)
        setpx(buf, vanish_x + half, y, edge_color)
        if (int(y * 0.09) % 2) == 0:
            cw = max(1, int(3 * t))
            for x in range(vanish_x - cw, vanish_x + cw):
                setpx(buf, x, y, (255, 232, 120))


def synth(sky, sun_top, sun_bot, grid_color, sun_y=0.74, sun_r=0.16, horizon_frac=0.58,
          ground=(20, 8, 40), stars=0, mountains=False, palms=False, road=False, seed=1):
    """Parametrized 80s synthwave/outrun scene: banded sun + neon grid + extras."""
    buf = make_buf()
    horizon = int(H * horizon_frac)
    sunx, suny, sunr = W // 2, int(horizon * sun_y), int(H * sun_r)

    for y in range(horizon):
        base = grad(sky, y / horizon)
        for x in range(W):
            c = base
            dx, dy = x - sunx, y - suny
            if dx * dx + dy * dy < sunr * sunr:
                st = (y - (suny - sunr)) / (2 * sunr)
                sunc = lerp(sun_top, sun_bot, st)
                # retro horizontal cut bands, denser toward the bottom of the disc
                if y > suny and ((y - suny) % 10) < (2 + (y - suny) // (sunr // 3 + 1)):
                    c = base
                else:
                    c = sunc
            setpx(buf, x, y, c)

    if stars:
        draw_stars(buf, horizon, sunx, suny, sunr, stars, seed)
    if mountains:
        draw_mountains(buf, horizon, grid_color, (8, 6, 22), seed)
    for y in range(horizon, H):
        for x in range(W):
            setpx(buf, x, y, ground)
    if road:
        draw_road(buf, horizon, sunx, grid_color)
    else:
        draw_grid(buf, horizon, grid_color)
    if palms:
        draw_palm(buf, int(W * 0.13), H, 1.00, False)
        draw_palm(buf, int(W * 0.88), H, 1.15, True)
    return buf


def main():
    os.makedirs(OUTDIR, exist_ok=True)
    jobs = [
        ("tropical-sunset.png", lambda: sunset(False)),
        ("tropical-palms.png", lambda: sunset(True)),
        ("tropical-miami.png", lambda: miami()),
        ("synthwave-classic.png", lambda: synth(
            sky=[(0, (10, 8, 52)), (0.45, (74, 22, 120)), (0.78, (196, 44, 150)), (1, (255, 120, 180))],
            sun_top=(255, 240, 120), sun_bot=(255, 70, 150), grid_color=(0, 232, 232),
            ground=(12, 6, 34), stars=150, seed=11)),
        ("synthwave-outrun.png", lambda: synth(
            sky=[(0, (22, 10, 62)), (0.4, (120, 24, 92)), (0.72, (232, 64, 60)), (1, (255, 172, 64))],
            sun_top=(255, 232, 96), sun_bot=(255, 64, 96), grid_color=(255, 42, 144),
            ground=(26, 8, 40), palms=True, seed=12)),
        ("synthwave-mountains.png", lambda: synth(
            sky=[(0, (8, 20, 72)), (0.42, (30, 92, 132)), (0.72, (120, 60, 150)), (1, (255, 122, 122))],
            sun_top=(255, 236, 140), sun_bot=(255, 92, 150), grid_color=(0, 240, 200),
            ground=(10, 16, 44), mountains=True, seed=13)),
        ("synthwave-road.png", lambda: synth(
            sky=[(0, (15, 8, 56)), (0.4, (92, 22, 122)), (0.72, (212, 42, 122)), (1, (255, 132, 92))],
            sun_top=(255, 236, 120), sun_bot=(255, 72, 122), grid_color=(255, 62, 162),
            ground=(14, 8, 34), road=True, seed=14)),
        ("synthwave-vapor.png", lambda: synth(
            sky=[(0, (120, 182, 232)), (0.4, (202, 150, 222)), (0.72, (255, 182, 212)), (1, (255, 222, 192))],
            sun_top=(255, 255, 232), sun_bot=(255, 158, 202), grid_color=(120, 232, 255),
            ground=(70, 58, 104), sun_y=0.78, seed=15)),
    ]
    for name, fn in jobs:
        path = os.path.join(OUTDIR, name)
        write_png(path, fn())
        print("wrote %s (%d bytes)" % (path, os.path.getsize(path)))


if __name__ == "__main__":
    main()
