"""Classic clock hands.

    {"type": "hands",
     "hour":   {"length": 0.52, "tail": 0.10, "width": 0.055, "color": "@ink", "shape": "rounded"},
     "minute": {"length": 0.80, "tail": 0.10, "width": 0.035, "color": "@ink", "shape": "rounded"},
     "second": null,
     "hub":    {"size": 0.06, "color": "@ink", "inner_size": 0.02, "inner_color": "@paper"},
     "smooth": true}

Shapes: "rounded" (bar with round ends), "line", "tapered", "triangle",
or "image" with {"image": "hand.png", "pivot": [0.5, 0.9], "size": 0.6}
where the sprite is drawn pointing at 12 o'clock and `size` is its length as a
fraction of the dial radius.  This is how pointer designs are swapped without
code.

A second hand forces per-second updates; the scheduler clamps that to the
configured `min_update_seconds` (default 60), so a second hand is only useful
if you deliberately lower that guard.
"""
from __future__ import annotations

import math
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

from ...core.timeinfo import hour_angle, minute_angle, second_angle
from ...renderer.canvas import Canvas
from ..base import Layer, RenderContext
from ..registry import register_layer
from ._util import paste_rotated, period_change


def hand_polygon(cx: float, cy: float, angle_deg: float, length: float, tail: float,
                 width: float, shape: str = "rounded") -> List[Tuple[float, float]]:
    a = math.radians(angle_deg)
    ux, uy = math.sin(a), -math.cos(a)            # along the hand
    px, py = -uy, ux                              # perpendicular
    tip = (cx + ux * length, cy + uy * length)
    base = (cx - ux * tail, cy - uy * tail)
    hw = width / 2
    if shape == "tapered":
        thw = hw * 0.25
        return [(base[0] + px * hw, base[1] + py * hw), (tip[0] + px * thw, tip[1] + py * thw),
                (tip[0] - px * thw, tip[1] - py * thw), (base[0] - px * hw, base[1] - py * hw)]
    if shape == "triangle":
        return [(base[0] + px * hw, base[1] + py * hw), tip, (base[0] - px * hw, base[1] - py * hw)]
    return [(base[0] + px * hw, base[1] + py * hw), (tip[0] + px * hw, tip[1] + py * hw),
            (tip[0] - px * hw, tip[1] - py * hw), (base[0] - px * hw, base[1] - py * hw)]


@register_layer("hands")
class HandsLayer(Layer):
    ORDER = ("hour", "minute", "second")

    def _draw_hand(self, canvas: Canvas, ctx: RenderContext, spec: Dict[str, Any], angle: float) -> None:
        shape = spec.get("shape", "rounded")
        color = ctx.color(spec.get("color", "@ink"))
        length = ctx.px(float(spec.get("length", 0.6)))
        tail = ctx.px(float(spec.get("tail", 0.0)))
        width = ctx.px(float(spec.get("width", 0.04)))
        if shape == "image":
            path = ctx.resolve(spec["image"])
            sprite_len = ctx.px(float(spec.get("size", spec.get("length", 0.6))))
            src = ctx.assets.image(path)
            sprite = ctx.assets.sprite(path, max(1, round(src.width * sprite_len / src.height)))
            paste_rotated(canvas, sprite, ctx.cx, ctx.cy, angle, tuple(spec.get("pivot", (0.5, 1.0))))
            return
        if shape == "line":
            a = math.radians(angle)
            p1 = (ctx.cx - math.sin(a) * tail, ctx.cy + math.cos(a) * tail)
            p2 = (ctx.cx + math.sin(a) * length, ctx.cy - math.cos(a) * length)
            canvas.line(p1, p2, fill=color, width=max(1, int(round(width))))
            return
        pts = hand_polygon(ctx.cx, ctx.cy, angle, length, tail, width, shape)
        canvas.polygon(pts, fill=color)
        if shape == "rounded":
            a = math.radians(angle)
            canvas.circle(ctx.cx + math.sin(a) * length, ctx.cy - math.cos(a) * length, width / 2, fill=color)
            if tail > 0:
                canvas.circle(ctx.cx - math.sin(a) * tail, ctx.cy + math.cos(a) * tail, width / 2, fill=color)

    def angles(self, ctx: RenderContext) -> Dict[str, float]:
        smooth = bool(self.opt("smooth", True))
        has_second = bool(self.opt("second"))
        return {"hour": hour_angle(ctx.now, smooth=smooth),
                "minute": minute_angle(ctx.now, smooth=smooth and has_second),
                "second": second_angle(ctx.now)}

    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:
        angles = self.angles(ctx)
        for name in self.ORDER:
            spec = self.opt(name)
            if spec:
                self._draw_hand(canvas, ctx, spec, angles[name])
        hub = self.opt("hub")
        if hub:
            r = ctx.px(float(hub.get("size", 0.05))) / 2
            canvas.circle(ctx.cx, ctx.cy, r, fill=ctx.color(hub.get("color", "@ink")))
            if hub.get("inner_size"):
                canvas.circle(ctx.cx, ctx.cy, ctx.px(float(hub["inner_size"])) / 2,
                              fill=ctx.color(hub.get("inner_color", "@paper")))

    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        if self.opt("second"):
            return period_change(ctx.now, 1)
        if self.opt("minute") or self.opt("smooth", True):
            return period_change(ctx.now, 60)
        return period_change(ctx.now, 3600)     # hour hand only, stepping
