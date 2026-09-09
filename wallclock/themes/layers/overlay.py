"""Overlay image, e.g. a 'Leave office' badge that a schedule slot switches on.

    {"type": "overlay", "source": "schedule", "position": [0.5, 0.5], "width": 0.4}

source: "schedule" -> the active slot's `overlay` image (nothing if the slot has none)
        "<path>"   -> fixed image
`width` is a fraction of the screen width (null keeps the sprite's own pixels).
"""
from __future__ import annotations

from datetime import datetime
from typing import Optional

from ...renderer.canvas import Canvas
from ..base import Layer, RenderContext
from ..registry import register_layer


@register_layer("overlay")
class OverlayLayer(Layer):
    def _path(self, ctx: RenderContext) -> Optional[str]:
        source = self.opt("source", "schedule")
        if source == "schedule":
            return ctx.slot.overlay if ctx.slot else None
        return source

    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:
        path = self._path(ctx)
        if not path:
            return
        resolved = ctx.resolve(path)
        if not resolved.exists():
            return
        width_frac = self.opt("width")
        sprite = ctx.assets.sprite(resolved, int(width_frac * ctx.width) if width_frac else None)
        x, y = ctx.screen_point(self.opt("position", (0.5, 0.5)))
        canvas.paste(sprite, x, y, center=True)

    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        if self.opt("source", "schedule") == "schedule" and ctx.schedule:
            return ctx.schedule.next_change(ctx.now)
        return None
