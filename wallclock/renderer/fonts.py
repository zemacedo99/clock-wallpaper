"""Font loading with sensible fallbacks and a small cache."""
from __future__ import annotations

from functools import lru_cache
from pathlib import Path
from typing import Optional

from PIL import ImageFont

_CANDIDATES = [
    # Windows
    "C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf",
    # macOS
    "/System/Library/Fonts/Supplemental/Arial.ttf", "/System/Library/Fonts/Helvetica.ttc",
    # Linux
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
]


@lru_cache(maxsize=64)
def load_font(spec: Optional[str], size_px: int, search_dir: Optional[str] = None) -> ImageFont.FreeTypeFont:
    size_px = max(4, int(size_px))
    candidates = []
    if spec:
        p = Path(spec).expanduser()
        if not p.is_absolute() and search_dir:
            candidates.append(Path(search_dir) / p)
        candidates.append(p)
        candidates.append(Path(spec))  # bare family name such as "arial.ttf" -> system lookup
    for c in candidates:
        try:
            return ImageFont.truetype(str(c), size_px)
        except OSError:
            continue
    for c in _CANDIDATES:
        if Path(c).exists():
            try:
                return ImageFont.truetype(c, size_px)
            except OSError:
                continue
    try:  # Pillow >= 10.1 ships a scalable default font
        return ImageFont.load_default(size=size_px)  # type: ignore[call-arg]
    except TypeError:
        return ImageFont.load_default()  # type: ignore[return-value]
