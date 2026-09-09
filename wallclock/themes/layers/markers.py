"""Hour markers: dots or ticks around the dial (static).

    {"type": "markers", "count": 12, "style": "dot", "radius": 1.0,
     "size": 0.022, "color": "@ink",
     "emphasize": {"every": 3, "size": 0.034, "color": "@accent"},
     "minor": {"count": 60, "style": "tick", "size": 0.006, "length": 0.03, "color": "@ink"},
     "numerals": {"size": 0.16, "radius": 0.82, "color": "@ink", "font": null, "every": 3}}

Sizes are fractions of the dial radius.  For ticks, `size` is the stroke width
and `length` the tick length (drawn inward from `radius`).
"""
from __future__ import annotations

from typing import Any, Dict

from ...core.timeinfo import polar
from ...renderer.canvas import Canvas
from ...renderer.fonts import load_font
from ..base import Layer, RenderContext
from ..registry import register_layer


@register_layer("markers")
class MarkersLayer(Layer):
    def _draw_ring(self, canvas: Canvas, ctx: RenderContext, spec: Dict[str, Any], skip_every: int = 0) -> None:
        count = int(spec.get("count", 12))
        style = spec.get("style", "dot")
        r_frac = float(spec.get("radius", 1.0))
        size = ctx.px(float(spec.get("size", 0.02)))
        color = ctx.color(spec.get("color", "@ink"))
        emph = spec.get("emphasize") or {}
        every = int(emph.get("every", 0) or 0)
        for i in range(count):
            if skip_every and i % skip_every == 0:
                continue          # minor ring: leave room for the major marker
            angle = 360.0 * i / count
            s, c = size, color
            if every and i % every == 0:
                s = ctx.px(float(emph.get("size", spec.get("size", 0.02))))
                c = ctx.color(emph.get("color", spec.get("color", "@ink")))
            if style == "tick":
                length = ctx.px(float(spec.get("length", 0.08)))
                p1 = polar(ctx.cx, ctx.cy, ctx.radius * r_frac, angle)
                p2 = polar(ctx.cx, ctx.cy, ctx.radius * r_frac - length, angle)
                canvas.line(p1, p2, fill=c, width=max(1, int(round(s))))
            else:
                x, y = polar(ctx.cx, ctx.cy, ctx.radius * r_frac, angle)
                canvas.circle(x, y, s / 2, fill=c)

    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:
        minor = self.opt("minor")
        if minor:
            major = int(self.opt("count", 12))
            mcount = int(minor.get("count", 60))
            self._draw_ring(canvas, ctx, {**minor, "radius": minor.get("radius", self.opt("radius", 1.0))},
                            skip_every=(mcount // major) if major and mcount % major == 0 else 0)
        self._draw_ring(canvas, ctx, self.spec)
        numerals = self.opt("numerals")
        if numerals:
            count = int(self.opt("count", 12))
            every = int(numerals.get("every", 1))
            font = load_font(numerals.get("font"), int(ctx.px(float(numerals.get("size", 0.16)))),
                             str(self.theme.dir) if self.theme.dir else None)
            color = ctx.color(numerals.get("color", "@ink"))
            for i in range(count):
                if i % every:
                    continue
                label = str(count if i == 0 else i) if count == 12 else str(i)
                x, y = polar(ctx.cx, ctx.cy, ctx.radius * float(numerals.get("radius", 0.8)), 360.0 * i / count)
                canvas.text(x, y, label, font, color, anchor="mm")
