"""Draws the PlaylistControl symbol and writes:
  resources/PlaylistControl.ico   (16..256 px, one drawing per size)
  resources/symbol.png            (symbol without background, for the header)
  assets/icon-256.png, assets/icon-preview.png
  assets/logo.png                 (the full logo, resized for the README)

    python tools/make_icon.py

The symbol follows the project logo (assets/logo-master.png): a white "C"
ring that holds a play button, cyan control sliders leaving the opening of
the ring and a gear. Small sizes drop the gear so the silhouette stays clear.
"""

import math
import os
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(__file__), "..")
NAVY_TOP = (22, 72, 158)
NAVY_BOTTOM = (8, 38, 102)
WHITE = (255, 255, 255, 255)
CYAN_A = (40, 205, 255)
CYAN_B = (26, 120, 255)


def gradient_shape(size, box, radius):
    """Rounded shape filled with the cyan gradient, as an RGBA layer."""
    x0, y0, x1, y1 = [int(round(v)) for v in box]
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    w, h = max(1, x1 - x0), max(1, y1 - y0)
    grad = Image.new("RGBA", (w, h))
    gd = ImageDraw.Draw(grad)
    for x in range(w):
        t = x / max(1, w - 1)
        c = tuple(int(CYAN_A[i] + (CYAN_B[i] - CYAN_A[i]) * t) for i in range(3))
        gd.line([(x, 0), (x, h)], fill=c + (255,))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius=radius, fill=255)
    layer.paste(grad, (x0, y0), mask)
    return layer


def draw(size, with_background=True):
    scale = 4
    s = size * scale
    u = s / 32.0
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))

    if with_background:
        grad = Image.new("RGBA", (s, s))
        gd = ImageDraw.Draw(grad)
        for y in range(s):
            t = y / (s - 1)
            c = tuple(int(NAVY_TOP[i] + (NAVY_BOTTOM[i] - NAVY_TOP[i]) * t) for i in range(3))
            gd.line([(0, y), (s, y)], fill=c + (255,))
        mask = Image.new("L", (s, s), 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, s - 1, s - 1], radius=int(s * 0.22), fill=255)
        img.paste(grad, (0, 0), mask)

    small = size < 32
    d = ImageDraw.Draw(img)

    # "C" ring, open on the right where the sliders come out.
    cx, cy = 13.0 * u, 16.5 * u
    r = (10.8 if small else 10.0) * u
    width = int((3.6 if small else 2.6) * u)
    d.arc([cx - r, cy - r, cx + r, cy + r], start=48, end=312, fill=WHITE, width=width)
    for angle in (48, 312):
        a = math.radians(angle)
        ex = cx + (r - width / 2) * math.cos(a)
        ey = cy + (r - width / 2) * math.sin(a)
        rr = width / 2
        d.ellipse([ex - rr, ey - rr, ex + rr, ey + rr], fill=WHITE)

    # Play button inside the ring.
    p = (5.6 if small else 5.0) * u
    d.polygon([(cx - p * 0.6, cy - p), (cx - p * 0.6, cy + p), (cx + p * 1.05, cy)], fill=WHITE)

    # Sliders (bar + knob), staggered like the logo.
    if small:
        bars = [(19.5, 13.0, 30.5), (18.0, 20.0, 29.0)]
        thickness = 3.0
    else:
        bars = [(20.0, 13.2, 30.5, 26.0), (19.0, 17.4, 30.5, 22.8), (18.0, 21.6, 29.0, 20.8)]
        thickness = 1.9
    for bar in bars:
        x0, y, x1 = bar[0], bar[1], bar[2]
        img = Image.alpha_composite(img, gradient_shape(s, [x0 * u, (y - thickness / 2) * u, x1 * u, (y + thickness / 2) * u],
                                                         int(thickness / 2 * u)))
        if not small:
            k, kr = bar[3] * u, 1.9 * u
            img = Image.alpha_composite(img, gradient_shape(s, [k - kr, y * u - kr, k + kr, y * u + kr], int(kr)))

    # Gear above the sliders.
    if size >= 48:
        gear = Image.new("RGBA", (s, s), (0, 0, 0, 0))
        gd = ImageDraw.Draw(gear)
        gx, gy, gr = 25.0 * u, 7.2 * u, 3.0 * u
        for i in range(8):
            a = 2 * math.pi * i / 8
            tx, ty = gx + math.cos(a) * gr * 1.08, gy + math.sin(a) * gr * 1.08
            tw = 0.95 * u
            gd.rounded_rectangle([tx - tw, ty - tw, tx + tw, ty + tw], radius=int(0.35 * u), fill=WHITE)
        gd.ellipse([gx - gr, gy - gr, gx + gr, gy + gr], fill=WHITE)
        hole = 1.25 * u
        gd.ellipse([gx - hole, gy - hole, gx + hole, gy + hole], fill=(0, 0, 0, 0))
        img = Image.alpha_composite(img, gear)

    return img.resize((size, size), Image.LANCZOS)


def main():
    sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    frames = [draw(n) for n in sizes]
    os.makedirs(os.path.join(ROOT, "resources"), exist_ok=True)
    os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)
    frames[-1].save(os.path.join(ROOT, "resources", "PlaylistControl.ico"), format="ICO",
                    sizes=[(n, n) for n in sizes], append_images=frames[:-1])
    frames[-1].save(os.path.join(ROOT, "assets", "icon-256.png"))
    draw(96, with_background=False).save(os.path.join(ROOT, "resources", "symbol.png"))

    preview = Image.new("RGBA", (sum(sizes) + 10 * len(sizes), 260), (232, 236, 241, 255))
    x = 5
    for f in frames:
        preview.paste(f, (x, 260 - f.height - 2), f)
        x += f.width + 10
    preview.save(os.path.join(ROOT, "assets", "icon-preview.png"))

    master = os.path.join(ROOT, "assets", "logo-master.png")
    if os.path.exists(master):
        logo = Image.open(master).convert("RGB")
        logo.thumbnail((640, 640), Image.LANCZOS)
        logo.save(os.path.join(ROOT, "assets", "logo.png"), optimize=True)
    print("icon written")


if __name__ == "__main__":
    main()
