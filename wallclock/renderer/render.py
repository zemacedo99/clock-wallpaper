"""Renderer: theme + context -> one static wallpaper image."""
from __future__ import annotations

import logging
from pathlib import Path
from typing import TYPE_CHECKING

from PIL import Image

from .canvas import Canvas

if TYPE_CHECKING:  # pragma: no cover
    from ..themes.base import RenderContext, Theme

log = logging.getLogger(__name__)


class Renderer:
    def __init__(self, supersample: int = 3):
        self.supersample = supersample

    def render(self, theme: "Theme", ctx: "RenderContext") -> Image.Image:
        pad = theme.dial_padding
        r = ctx.radius * pad
        aa_bbox = (int(ctx.cx - r), int(ctx.cy - r), int(ctx.cx + r) + 1, int(ctx.cy + r) + 1)
        canvas = Canvas(ctx.width, ctx.height, supersample=self.supersample,
                        aa_bbox=aa_bbox, background=ctx.color(theme.paper))
        for layer in theme.layers:
            if layer.enabled(ctx):
                layer.draw(canvas, ctx)
        return canvas.finish()


class OutputWriter:
    """Writes the rendered image, alternating between two file names.

    Several desktops (Windows, GNOME, macOS) skip the refresh when asked to set
    a wallpaper whose path has not changed, so we alternate a/b.
    """

    def __init__(self, out_dir: Path, fmt: str = "png", compress_level: int = 1):
        self.out_dir = Path(out_dir)
        self.fmt = fmt.lower()
        self.compress_level = compress_level
        self._toggle = False

    def write(self, img: Image.Image, stem: str = "wallclock") -> Path:
        self.out_dir.mkdir(parents=True, exist_ok=True)
        self._toggle = not self._toggle
        path = self.out_dir / f"{stem}-{'a' if self._toggle else 'b'}.{self.fmt}"
        if self.fmt == "png":
            img.save(path, compress_level=self.compress_level)
        elif self.fmt in ("jpg", "jpeg"):
            img.save(path, quality=92)
        else:
            img.save(path)
        return path
