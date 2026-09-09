"""Background layer.

    {"type": "background", "source": "schedule", "color": "@paper", "fit": "cover"}

source: "schedule"  -> image of the active schedule slot (falls back to `image`/`color`)
        "<path>"    -> fixed image
        null        -> solid `color`
A slot `tint` (e.g. "#00000060") is blended over the background when present.
"""
from __future__ import annotations

from datetime import datetime
from typing import Optional

from ...renderer.canvas import Canvas
from ..base import Layer, RenderContext
from ..registry import register_layer


@register_layer("background")
class BackgroundLayer(Layer):
    def _image_path(self, ctx: RenderContext) -> Optional[str]:
        source = self.opt("source", "schedule")
        if source == "schedule":
            if ctx.slot and ctx.slot.image:
                return ctx.slot.image
            return self.opt("image")
        return source

    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:
        canvas.fill(ctx.color(self.opt("color", "@paper"), self.theme.paper))
        path = self._image_path(ctx)
        if path:
            resolved = ctx.resolve(path)
            if resolved.exists():
                img = ctx.assets.scaled(resolved, ctx.width, ctx.height, self.opt("fit", "cover"))
                canvas.paste(img, 0, 0, center=False)
            else:
                import logging
                logging.getLogger(__name__).warning("background image not found: %s", resolved)
        tint = (ctx.slot.tint if ctx.slot and ctx.slot.tint else self.opt("tint"))
        if tint:
            canvas.tint(ctx.color(tint))

    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        if self.opt("source", "schedule") == "schedule" and ctx.schedule:
            return ctx.schedule.next_change(ctx.now)
        return None
