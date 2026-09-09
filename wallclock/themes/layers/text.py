"""Text layer: captions from the schedule, digital time, dates.

    {"type": "text", "text": "{caption}", "position": [0.5, 0.9], "size": 0.14,
     "color": "@ink", "font": null, "anchor": "mm", "align": "center"}

`text` is first formatted with {caption} (the active slot's caption) and then
passed through strftime, so "%H:%M  {caption}" works.  A layer whose text is
empty after formatting draws nothing.  `size` is a fraction of the dial radius.
"""
from __future__ import annotations

import re
from datetime import datetime, timedelta
from typing import Optional

from ...renderer.canvas import Canvas
from ...renderer.fonts import load_font
from ..base import Layer, RenderContext
from ..registry import register_layer
from ._util import period_change

_PER_SECOND = re.compile(r"%[STXc]|%-?S")
_PER_MINUTE = re.compile(r"%[MRr]|%-?[IHl]")


@register_layer("text")
class TextLayer(Layer):
    def render_text(self, ctx: RenderContext) -> str:
        template = str(self.opt("text", "{caption}"))
        s = template.replace("{caption}", ctx.caption)
        if "%" in s:
            s = ctx.now.strftime(s)
        return s.strip()

    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:
        s = self.render_text(ctx)
        if not s:
            return
        font = load_font(self.opt("font"), int(ctx.px(float(self.opt("size", 0.12)))),
                         str(self.theme.dir) if self.theme.dir else None)
        x, y = ctx.screen_point(self.opt("position", (0.5, 0.9)))
        canvas.text(x, y, s, font, ctx.color(self.opt("color", "@ink")), anchor=self.opt("anchor", "mm"))

    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        template = str(self.opt("text", "{caption}"))
        if _PER_SECOND.search(template):
            return period_change(ctx.now, 1)
        if _PER_MINUTE.search(template):
            return period_change(ctx.now, 60)
        if "%" in template:  # date-ish -> midnight
            return (ctx.now + timedelta(days=1)).replace(hour=0, minute=0, second=0, microsecond=0)
        if "{caption}" in template and ctx.schedule:
            return ctx.schedule.next_change(ctx.now)
        return None
