"""Small helpers shared by layers."""
from __future__ import annotations

import math
from datetime import datetime
from typing import Optional, Tuple

from PIL import Image

from ...core.timeinfo import next_boundary
from ...renderer.canvas import Canvas


def period_change(now: datetime, seconds: Optional[float]) -> Optional[datetime]:
    return next_boundary(now, seconds) if seconds else None


def paste_rotated(canvas: Canvas, sprite: Image.Image, x: float, y: float,
                  angle_cw_deg: float, pivot: Tuple[float, float] = (0.5, 0.5)) -> None:
    """Paste `sprite` rotated clockwise by `angle` so that its pivot lands on (x, y).

    `pivot` is a fraction of the sprite size; (0.5, 0.5) is its centre and
    (0.5, 1.0) the middle of its bottom edge (useful for hand sprites drawn
    pointing up).
    """
    w, h = sprite.size
    vx, vy = pivot[0] * w - w / 2, pivot[1] * h - h / 2      # pivot relative to centre
    a = math.radians(angle_cw_deg)
    cos_a, sin_a = math.cos(a), math.sin(a)
    rvx = vx * cos_a - vy * sin_a                              # rotate clockwise (y down)
    rvy = vx * sin_a + vy * cos_a
    rotated = sprite.rotate(-angle_cw_deg, resample=Image.Resampling.BICUBIC, expand=True) \
        if angle_cw_deg % 360 else sprite
    canvas.paste(rotated, x - rvx, y - rvy, center=True)
