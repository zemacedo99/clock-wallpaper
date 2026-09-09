"""Regenerates the example backgrounds and sprites (plain gradients and a tiny car).

Run once:  python examples/make_assets.py
The outputs are committed, so this is only needed if you change the palette below.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SIZE = (1600, 900)

BACKGROUNDS = {
    "morning":      ("#fbe7c6", "#f6c9a0"),
    "work":         ("#efe9df", "#dcd6cb"),
    "leave-office": ("#ffb36b", "#e0602e"),
    "evening":      ("#3f4a6b", "#1e2440"),
    "night":        ("#141626", "#05060d"),
}


def hex_to_rgb(h: str):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def gradient(size, top: str, bottom: str) -> Image.Image:
    w, h = size
    a, b = hex_to_rgb(top), hex_to_rgb(bottom)
    column = Image.new("RGB", (1, h))
    px = [tuple(int(a[i] + (b[i] - a[i]) * y / (h - 1)) for i in range(3)) for y in range(h)]
    column.putdata(px)
    return column.resize(size)


def car(width: int = 160) -> Image.Image:
    """Top-down car, pointing up (12 o'clock)."""
    w, h = width, int(width * 1.9)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m = w * 0.14
    for y in (h * 0.22, h * 0.68):                       # wheels
        d.rounded_rectangle([0, y, w * 0.18, y + h * 0.16], radius=w * 0.05, fill="#1b1b1b")
        d.rounded_rectangle([w * 0.82, y, w, y + h * 0.16], radius=w * 0.05, fill="#1b1b1b")
    d.rounded_rectangle([m, 0, w - m, h], radius=w * 0.32, fill="#d9542b")          # body
    d.rounded_rectangle([m * 1.5, h * 0.30, w - m * 1.5, h * 0.44], radius=w * 0.06, fill="#a9d9ef")  # windscreen
    d.rounded_rectangle([m * 1.5, h * 0.62, w - m * 1.5, h * 0.72], radius=w * 0.06, fill="#a9d9ef")  # rear window
    d.rounded_rectangle([m * 1.5, h * 0.44, w - m * 1.5, h * 0.62], radius=w * 0.05, fill="#c34a25")  # roof
    d.ellipse([m * 1.2, h * 0.02, m * 2.4, h * 0.07], fill="#fff4b0")                # headlights
    d.ellipse([w - m * 2.4, h * 0.02, w - m * 1.2, h * 0.07], fill="#fff4b0")
    return img


def main() -> None:
    bg_dir = HERE / "backgrounds"
    bg_dir.mkdir(exist_ok=True)
    for name, (top, bottom) in BACKGROUNDS.items():
        gradient(SIZE, top, bottom).save(bg_dir / f"{name}.png", optimize=True)
    sprites = HERE / "sprites"
    sprites.mkdir(exist_ok=True)
    car().save(sprites / "car.png")
    print("assets written to", bg_dir, "and", sprites)


if __name__ == "__main__":
    main()
