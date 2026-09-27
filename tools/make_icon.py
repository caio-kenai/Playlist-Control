"""Builds the application images from the artwork in assets/:

  assets/logo-master.png     logo (transparent background)
  assets/wordmark-master.png "PLAYLIST CONTROL" wordmark (white, transparent)

Outputs:
  resources/PlaylistControl.ico  16..256 px, the logo filling the whole frame
  resources/symbol.png           logo for the application header
  resources/wordmark.png         wordmark for the application header
  assets/logo.png                logo for the README
  assets/icon-preview.png        preview of every icon size

    python tools/make_icon.py

The artwork has a haze of almost transparent pixels and a few loose specks;
both are removed before cropping to the visible content.
"""

import os
import numpy as np
from PIL import Image, ImageFilter
from scipy import ndimage

ROOT = os.path.join(os.path.dirname(__file__), "..")


def clean(path, min_alpha=40, min_blob_fraction=0.0004):
    im = Image.open(path).convert("RGBA")
    a = np.array(im)
    alpha = a[:, :, 3].astype(np.int32)
    alpha[alpha < min_alpha] = 0
    # Drop small isolated blobs (specks around the shapes).
    labels, count = ndimage.label(alpha > 0)
    if count:
        sizes = ndimage.sum(np.ones_like(alpha), labels, index=range(1, count + 1))
        limit = alpha.size * min_blob_fraction
        for i, size in enumerate(sizes, start=1):
            if size < limit:
                alpha[labels == i] = 0
    a[:, :, 3] = alpha.astype(np.uint8)
    a[alpha == 0] = 0
    out = Image.fromarray(a, "RGBA")
    return out.crop(out.getbbox())


def square(im, margin=0.0):
    side = int(max(im.size) * (1 + 2 * margin))
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(im, ((side - im.width) // 2, (side - im.height) // 2), im)
    return canvas


def resize(im, size):
    out = im.resize((size, size), Image.LANCZOS)
    if size <= 32:
        out = out.filter(ImageFilter.UnsharpMask(radius=0.6, percent=60, threshold=0))
    return out


def main():
    logo = square(clean(os.path.join(ROOT, "assets", "logo-master.png")))
    sizes = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
    frames = [resize(logo, n) for n in sizes]
    frames[-1].save(os.path.join(ROOT, "resources", "PlaylistControl.ico"), format="ICO",
                    sizes=[(n, n) for n in sizes], append_images=frames[:-1])
    resize(logo, 128).save(os.path.join(ROOT, "resources", "symbol.png"))
    resize(logo, 512).save(os.path.join(ROOT, "assets", "logo.png"), optimize=True)

    wordmark = clean(os.path.join(ROOT, "assets", "wordmark-master.png"), min_alpha=60, min_blob_fraction=0.00005)
    target_h = 96
    wordmark.resize((round(wordmark.width * target_h / wordmark.height), target_h), Image.LANCZOS) \
        .save(os.path.join(ROOT, "resources", "wordmark.png"))

    preview = Image.new("RGBA", (sum(sizes) + 10 * len(sizes), 270), (232, 236, 241, 255))
    x = 5
    for f in frames:
        preview.paste(f, (x, 268 - f.height), f)
        x += f.width + 10
    preview.save(os.path.join(ROOT, "assets", "icon-preview.png"))
    print("images written")


if __name__ == "__main__":
    main()
