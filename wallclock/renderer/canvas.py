"""Drawing surface used by layers.

Layers draw in *final screen pixels* and never touch Pillow directly, which
keeps them portable (a Kotlin/Swift port only needs to re-implement this class).

Anti-aliasing: Pillow's vector primitives are not anti-aliased.  Instead of
supersampling the whole wallpaper (very expensive at 4K) the canvas keeps a
supersampled RGBA layer covering just the dial region.  Vector primitives that
fall inside that region are drawn there at `supersample`x and downsampled on
flush; everything else is drawn at 1x.  Text and pasted images are already
smooth, so they always go to the base image (after flushing, so layer order
is preserved).
"""
from __future__ import annotations

from typing import Iterable, List, Optional, Sequence, Tuple, Union

from PIL import Image, ImageColor, ImageDraw, ImageFont

Color = Union[str, Sequence[int], Tuple[int, int, int, int]]
Point = Tuple[float, float]
BBox = Tuple[int, int, int, int]


def to_color(c: Optional[Color], default: Tuple[int, int, int, int] = (0, 0, 0, 255)) -> Tuple[int, int, int, int]:
    """Accept '#rgb', '#rrggbb', '#rrggbbaa', CSS names or [r,g,b(,a)] lists."""
    if c is None:
        return default
    if isinstance(c, (list, tuple)):
        t = tuple(int(v) for v in c)
        return t if len(t) == 4 else (t + (255,))  # type: ignore[return-value]
    return ImageColor.getcolor(str(c), "RGBA")  # type: ignore[return-value]


class Canvas:
    def __init__(self, width: int, height: int, supersample: int = 3,
                 aa_bbox: Optional[BBox] = None, background: Color = "#000000"):
        self.width, self.height = int(width), int(height)
        self.base = Image.new("RGBA", (self.width, self.height), to_color(background))
        self._draw = ImageDraw.Draw(self.base)
        self.ss = max(1, int(supersample))
        self.aa_bbox: Optional[BBox] = None
        if aa_bbox is not None and self.ss > 1:
            x0, y0, x1, y1 = aa_bbox
            x0, y0 = max(0, int(x0)), max(0, int(y0))
            x1, y1 = min(self.width, int(x1)), min(self.height, int(y1))
            if x1 > x0 and y1 > y0:
                self.aa_bbox = (x0, y0, x1, y1)
        self._aa: Optional[Image.Image] = None
        self._aa_draw: Optional[ImageDraw.ImageDraw] = None

    # ---- internals ----------------------------------------------------
    def _aa_layer(self) -> ImageDraw.ImageDraw:
        if self._aa is None:
            assert self.aa_bbox is not None
            x0, y0, x1, y1 = self.aa_bbox
            self._aa = Image.new("RGBA", ((x1 - x0) * self.ss, (y1 - y0) * self.ss), (0, 0, 0, 0))
            self._aa_draw = ImageDraw.Draw(self._aa)
        assert self._aa_draw is not None
        return self._aa_draw

    def _fits_aa(self, xs: Iterable[float], ys: Iterable[float], margin: float = 0) -> bool:
        if self.aa_bbox is None:
            return False
        xs, ys = list(xs), list(ys)
        x0, y0, x1, y1 = self.aa_bbox
        return (min(xs) - margin >= x0 and max(xs) + margin <= x1
                and min(ys) - margin >= y0 and max(ys) + margin <= y1)

    def _tx(self, pts: Sequence[Point]) -> List[Point]:
        assert self.aa_bbox is not None
        x0, y0 = self.aa_bbox[0], self.aa_bbox[1]
        s = self.ss
        return [((x - x0) * s, (y - y0) * s) for x, y in pts]

    def flush(self) -> None:
        """Composite the supersampled layer onto the base image."""
        if self._aa is None or self.aa_bbox is None:
            return
        x0, y0, x1, y1 = self.aa_bbox
        small = self._aa.resize((x1 - x0, y1 - y0), Image.Resampling.BOX)
        self.base.alpha_composite(small, (x0, y0))
        self._aa = None
        self._aa_draw = None

    # ---- vector primitives (may be anti-aliased) ----------------------
    def polygon(self, points: Sequence[Point], fill: Optional[Color] = None,
                outline: Optional[Color] = None, width: int = 1) -> None:
        f = to_color(fill) if fill is not None else None
        o = to_color(outline) if outline is not None else None
        xs, ys = [p[0] for p in points], [p[1] for p in points]
        if self._fits_aa(xs, ys, width):
            self._aa_layer().polygon(self._tx(points), fill=f, outline=o, width=max(1, width * self.ss))
        else:
            self._draw.polygon([(float(x), float(y)) for x, y in points], fill=f, outline=o, width=width)

    def ellipse(self, cx: float, cy: float, rx: float, ry: Optional[float] = None,
                fill: Optional[Color] = None, outline: Optional[Color] = None, width: int = 1) -> None:
        ry = rx if ry is None else ry
        f = to_color(fill) if fill is not None else None
        o = to_color(outline) if outline is not None else None
        box = [(cx - rx, cy - ry), (cx + rx, cy + ry)]
        if self._fits_aa([cx - rx, cx + rx], [cy - ry, cy + ry], width):
            self._aa_layer().ellipse(self._tx(box), fill=f, outline=o, width=max(1, width * self.ss))
        else:
            self._draw.ellipse(box, fill=f, outline=o, width=width)

    def circle(self, cx: float, cy: float, r: float, fill: Optional[Color] = None,
               outline: Optional[Color] = None, width: int = 1) -> None:
        self.ellipse(cx, cy, r, r, fill=fill, outline=outline, width=width)

    def line(self, p1: Point, p2: Point, fill: Color, width: int = 1) -> None:
        f = to_color(fill)
        if self._fits_aa([p1[0], p2[0]], [p1[1], p2[1]], width):
            self._aa_layer().line(self._tx([p1, p2]), fill=f, width=max(1, int(width * self.ss)))
        else:
            self._draw.line([p1, p2], fill=f, width=max(1, int(width)))

    # ---- raster primitives (always on the base image) -----------------
    def fill(self, color: Color) -> None:
        self.flush()
        self._draw.rectangle([(0, 0), (self.width, self.height)], fill=to_color(color))

    def tint(self, color: Color) -> None:
        """Blend a translucent colour over the whole image (e.g. '#00000060')."""
        self.flush()
        layer = Image.new("RGBA", self.base.size, to_color(color))
        self.base.alpha_composite(layer)

    def paste(self, img: Image.Image, x: float, y: float, center: bool = True) -> None:
        """Alpha-composite an image; (x, y) is its centre unless center=False (then top-left)."""
        self.flush()
        img = img if img.mode == "RGBA" else img.convert("RGBA")
        w, h = img.size
        left = int(round(x - w / 2)) if center else int(round(x))
        top = int(round(y - h / 2)) if center else int(round(y))
        # clip to the canvas; alpha_composite needs the source to fit
        sx0, sy0 = max(0, -left), max(0, -top)
        sx1, sy1 = min(w, self.width - left), min(h, self.height - top)
        if sx1 <= sx0 or sy1 <= sy0:
            return
        if (sx0, sy0, sx1, sy1) != (0, 0, w, h):
            img = img.crop((sx0, sy0, sx1, sy1))
        self.base.alpha_composite(img, (left + sx0, top + sy0))

    def text(self, x: float, y: float, s: str, font: ImageFont.FreeTypeFont, fill: Color,
             anchor: str = "mm") -> None:
        self.flush()
        self._draw.text((x, y), s, font=font, fill=to_color(fill), anchor=anchor)

    def text_size(self, s: str, font: ImageFont.FreeTypeFont) -> Tuple[int, int]:
        left, top, right, bottom = self._draw.textbbox((0, 0), s, font=font)
        return right - left, bottom - top

    # ---- output -------------------------------------------------------
    def finish(self) -> Image.Image:
        self.flush()
        return self.base.convert("RGB")

    def pixel(self, x: int, y: int) -> Tuple[int, int, int]:
        """Convenience for tests: colour of a pixel in the finished image."""
        self.flush()
        return self.base.convert("RGB").getpixel((int(x), int(y)))  # type: ignore[return-value]
