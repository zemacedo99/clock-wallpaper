"""Orbiter: anything that travels around the dial to show time.

A car driving round the clock once an hour, the sun crossing the sky between
sunrise and sunset, the moon at night, a planet doing one lap a day...

    {"type": "orbiter", "driver": "minute", "radius": 1.0,
     "image": "sprites/car.png", "size": 0.22, "rotate": true, "heading_offset": 90,
     "shape": {"kind": "circle", "color": "#f6c343", "size": 0.14},   // used when no image
     "arc": [270, 90],            // sun/moon: angle at start and at end of the visible span
     "update_seconds": 60}

driver: hour12 | hour24 | minute | second | sun | moon
   hour12/minute/second follow the corresponding hand angle; hour24 does one lap a day.
   sun  is visible from sunrise to sunset and travels the `arc` (default 270 -> 90,
        i.e. rises on the left, culminates at 12, sets on the right).
   moon does the same between sunset and the next sunrise.
   Both need `location` or `sun` in the config; without it the layer is hidden.
size: sprite width (or shape diameter) as a fraction of the dial radius.
rotate: turn the sprite along its direction of travel (+ heading_offset degrees).
update_seconds: how often the position must be refreshed (default 60).
"""
from __future__ import annotations

from datetime import datetime, timedelta
from typing import Optional, Tuple

from ...core.timeinfo import (day_angle, fraction_between, hour_angle, minute_angle,
                              next_boundary, second_angle)
from ...renderer.canvas import Canvas
from ..base import Layer, RenderContext
from ..registry import register_layer
from ._util import paste_rotated

Interval = Tuple[datetime, datetime]


@register_layer("orbiter")
class OrbiterLayer(Layer):
    # ---- time -> angle -----------------------------------------------
    def _interval(self, ctx: RenderContext) -> Optional[Interval]:
        """Visible interval for sun/moon drivers (None when unknown)."""
        if ctx.sun is None:
            return None
        driver = self.opt("driver", "minute")
        if driver == "sun":
            return ctx.sun.daylight(ctx.now)
        if driver == "moon":
            return ctx.sun.night(ctx.now)
        return None

    def angle(self, ctx: RenderContext) -> Optional[float]:
        """Current angle, or None when the object is not visible."""
        driver = self.opt("driver", "minute")
        now = ctx.now
        if driver == "hour12":
            return hour_angle(now)
        if driver == "hour24":
            return day_angle(now)
        if driver == "minute":
            return minute_angle(now, smooth=True)
        if driver == "second":
            return second_angle(now)
        if driver in ("sun", "moon"):
            iv = self._interval(ctx)
            if iv is None or not (iv[0] <= now < iv[1]):
                return None
            start, end = self.opt("arc", (270, 90))
            span = (end - start) % 360 or 360
            return (start + span * fraction_between(now, iv[0], iv[1])) % 360
        raise ValueError(f"unknown orbiter driver {driver!r}")

    # ---- drawing -----------------------------------------------------
    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:
        angle = self.angle(ctx)
        if angle is None:
            return
        x, y = ctx.point(angle, float(self.opt("radius", 1.0)))
        size = ctx.px(float(self.opt("size", 0.15)))
        image = self.opt("image")
        if image:
            path = ctx.resolve(image)
            sprite = ctx.assets.sprite(path, max(1, int(round(size))))
            heading = (angle + float(self.opt("heading_offset", 90))) if self.opt("rotate", False) else 0.0
            paste_rotated(canvas, sprite, x, y, heading, tuple(self.opt("pivot", (0.5, 0.5))))
            return
        shape = self.opt("shape") or {"kind": "circle"}
        color = ctx.color(shape.get("color", "@accent"))
        d = ctx.px(float(shape.get("size", self.opt("size", 0.15))))
        kind = shape.get("kind", "circle")
        if kind == "circle":
            canvas.circle(x, y, d / 2, fill=color)
        elif kind == "ring":
            canvas.circle(x, y, d / 2, outline=color, width=max(1, int(d * 0.12)))
        elif kind == "crescent":
            canvas.circle(x, y, d / 2, fill=color)
            bg = ctx.color(shape.get("shadow", "@paper"))
            canvas.circle(x + d * 0.22, y - d * 0.12, d / 2 * 0.86, fill=bg)
        else:
            raise ValueError(f"unknown shape kind {kind!r}")

    # ---- scheduling --------------------------------------------------
    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        driver = self.opt("driver", "minute")
        period = float(self.opt("update_seconds", 1 if driver == "second" else 60))
        if driver in ("sun", "moon"):
            iv = self._interval(ctx)
            if iv is None:
                return None
            if ctx.now < iv[0]:
                return iv[0]                   # appears later
            if ctx.now >= iv[1]:
                return None                    # gone; interval provider handles tomorrow via background/hands wake
            return min(next_boundary(ctx.now, period), iv[1])
        return next_boundary(ctx.now, period)
