"""Image asset cache.

Decoding and resizing a large photo every minute is the single most expensive
thing this program could do, so backgrounds scaled to the screen size are
cached both in memory (daemon mode) and on disk (one-shot mode driven by an OS
scheduler).  Cache keys include the source path, mtime, size and fit mode.
"""
from __future__ import annotations

import hashlib
import logging
from pathlib import Path
from typing import Dict, Optional, Tuple

from PIL import Image

log = logging.getLogger(__name__)


def fit_image(img: Image.Image, width: int, height: int, mode: str = "cover",
              fill=(0, 0, 0, 255)) -> Image.Image:
    """Scale `img` into width x height using cover | contain | stretch | center."""
    img = img.convert("RGBA")
    if mode == "stretch":
        return img.resize((width, height), Image.Resampling.LANCZOS)
    if mode == "center":
        out = Image.new("RGBA", (width, height), fill)
        out.alpha_composite(img.crop((0, 0, min(img.width, width), min(img.height, height))),
                            (max(0, (width - img.width) // 2), max(0, (height - img.height) // 2)))
        return out
    sw, sh = width / img.width, height / img.height
    scale = max(sw, sh) if mode == "cover" else min(sw, sh)
    nw, nh = max(1, round(img.width * scale)), max(1, round(img.height * scale))
    scaled = img.resize((nw, nh), Image.Resampling.LANCZOS)
    if mode == "cover":
        left, top = (nw - width) // 2, (nh - height) // 2
        return scaled.crop((left, top, left + width, top + height))
    out = Image.new("RGBA", (width, height), fill)
    out.alpha_composite(scaled, ((width - nw) // 2, (height - nh) // 2))
    return out


class AssetCache:
    def __init__(self, disk_dir: Optional[Path] = None):
        self.disk_dir = disk_dir
        self._mem: Dict[Tuple, Image.Image] = {}

    def image(self, path: Path) -> Image.Image:
        """Raw image (RGBA), memoised on path+mtime."""
        key = ("raw", str(path), path.stat().st_mtime_ns)
        img = self._mem.get(key)
        if img is None:
            with Image.open(path) as im:
                img = im.convert("RGBA")
            self._mem[key] = img
        return img

    def scaled(self, path: Path, width: int, height: int, mode: str = "cover") -> Image.Image:
        mtime = path.stat().st_mtime_ns
        key = ("scaled", str(path), mtime, width, height, mode)
        img = self._mem.get(key)
        if img is not None:
            return img
        disk = None
        if self.disk_dir is not None:
            digest = hashlib.sha1(repr(key).encode()).hexdigest()[:20]
            disk = self.disk_dir / f"bg-{digest}.png"
            if disk.exists():
                try:
                    with Image.open(disk) as im:
                        img = im.convert("RGBA")
                except OSError:
                    img = None
        if img is None:
            img = fit_image(self.image(path), width, height, mode)
            if disk is not None:
                try:
                    disk.parent.mkdir(parents=True, exist_ok=True)
                    img.save(disk, compress_level=1)
                except OSError as exc:  # cache is best effort
                    log.debug("could not write cache %s: %s", disk, exc)
        self._mem[key] = img
        return img

    def sprite(self, path: Path, width: Optional[int] = None) -> Image.Image:
        """Image scaled to a given pixel width (keeps aspect)."""
        src = self.image(path)
        if width is None or width == src.width:
            return src
        key = ("sprite", str(path), path.stat().st_mtime_ns, width)
        img = self._mem.get(key)
        if img is None:
            h = max(1, round(src.height * width / src.width))
            img = src.resize((max(1, width), h), Image.Resampling.LANCZOS)
            self._mem[key] = img
        return img
