"""Theme model and the Layer interface.

A Theme is data (JSON) plus an ordered list of Layer objects.  Every visual
element - background, markers, hands, text, a car driving round the dial, the
sun setting - is a Layer with two responsibilities:

    draw(canvas, ctx)        paint yourself for ctx.now
    next_change(ctx)         when will what you paint look different?  (None = never)

The scheduler takes the minimum over all layers, so a theme with only an hour
hand wakes every 12 minutes and a schedule-only theme wakes only at slot
boundaries.  Nothing polls.

All sizes in a theme are fractions of the dial radius (hands, markers) or of
the screen (positions), so a theme renders identically on any resolution.
"""
from __future__ import annotations

import json
import logging
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

from ..core.schedule import Schedule, Slot
from ..core.sun import SunProvider
from ..core.timeinfo import polar
from ..renderer.assets import AssetCache
from ..renderer.canvas import Canvas
from .registry import get_layer_class, load_plugins

log = logging.getLogger(__name__)


@dataclass
class RenderContext:
    now: datetime
    width: int
    height: int
    cx: float
    cy: float
    radius: float
    palette: Dict[str, str] = field(default_factory=dict)
    slot: Optional[Slot] = None
    schedule: Optional[Schedule] = None
    sun: Optional[SunProvider] = None
    assets: AssetCache = field(default_factory=AssetCache)
    search_dirs: List[Path] = field(default_factory=list)

    # geometry helpers -------------------------------------------------
    def px(self, frac: float) -> float:
        """Fraction of the dial radius -> pixels."""
        return frac * self.radius

    def point(self, angle_deg: float, r_frac: float) -> Tuple[float, float]:
        return polar(self.cx, self.cy, self.radius * r_frac, angle_deg)

    def screen_point(self, pos: Sequence[float]) -> Tuple[float, float]:
        """[0..1, 0..1] screen fractions -> pixels."""
        return pos[0] * self.width, pos[1] * self.height

    # data helpers -----------------------------------------------------
    def color(self, value: Any, default: str = "#000000") -> Any:
        """Resolve '@name' palette references (slot overrides win)."""
        if value is None:
            return default
        if isinstance(value, str) and value.startswith("@"):
            name = value[1:]
            if self.slot and name in self.slot.palette:
                return self.slot.palette[name]
            return self.palette.get(name, default)
        return value

    def resolve(self, path: Union[str, Path]) -> Path:
        p = Path(path).expanduser()
        if p.is_absolute():
            return p
        for d in self.search_dirs:
            cand = d / p
            if cand.exists():
                return cand
        return (self.search_dirs[0] / p) if self.search_dirs else p

    @property
    def caption(self) -> str:
        return (self.slot.caption if self.slot and self.slot.caption else "")


class Layer:
    """Base class for all layers.  Subclasses read their options from `spec`."""

    type_name: str = "layer"

    def __init__(self, spec: Dict[str, Any], theme: "Theme"):
        self.spec = spec
        self.theme = theme
        self.name = spec.get("name") or self.type_name

    def opt(self, key: str, default: Any = None) -> Any:
        return self.spec.get(key, default)

    def enabled(self, ctx: RenderContext) -> bool:
        return bool(self.spec.get("enabled", True))

    def draw(self, canvas: Canvas, ctx: RenderContext) -> None:  # pragma: no cover
        raise NotImplementedError

    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        """Next instant this layer will look different. None means static."""
        return None

    def __repr__(self) -> str:
        return f"<{type(self).__name__} {self.name}>"


class Theme:
    def __init__(self, data: Dict[str, Any], path: Optional[Path] = None):
        self.data = data
        self.path = path
        self.name: str = data.get("name", path.stem if path else "theme")
        self.palette: Dict[str, str] = dict(data.get("palette") or {})
        dial = data.get("dial") or {}
        self.dial_center: Tuple[float, float] = tuple(dial.get("center", (0.5, 0.5)))  # type: ignore[assignment]
        self.dial_radius: float = float(dial.get("radius", 0.33))
        self.dial_reference: str = dial.get("reference", "min")
        self.dial_padding: float = float(dial.get("padding", 1.4))
        load_plugins(data.get("plugins"))
        self.layers: List[Layer] = []
        for spec in data.get("layers", []):
            cls = get_layer_class(spec["type"])
            self.layers.append(cls(spec, self))

    @property
    def paper(self) -> str:
        return self.palette.get("paper", "#000000")

    @property
    def dir(self) -> Optional[Path]:
        return self.path.parent if self.path else None

    @classmethod
    def load(cls, path: Union[str, Path]) -> "Theme":
        p = Path(path).expanduser()
        with p.open(encoding="utf-8") as fh:
            data = json.load(fh)
        return cls(data, p.resolve())

    # ---- context -----------------------------------------------------
    def geometry(self, width: int, height: int) -> Tuple[float, float, float]:
        ref = {"min": min(width, height), "max": max(width, height),
               "width": width, "height": height}[self.dial_reference]
        return self.dial_center[0] * width, self.dial_center[1] * height, self.dial_radius * ref

    def context(self, now: datetime, width: int, height: int, *, slot: Optional[Slot] = None,
                schedule: Optional[Schedule] = None, sun: Optional[SunProvider] = None,
                assets: Optional[AssetCache] = None, search_dirs: Sequence[Path] = ()) -> RenderContext:
        cx, cy, r = self.geometry(width, height)
        dirs = list(search_dirs)
        if self.dir and self.dir not in dirs:
            dirs.append(self.dir)
        return RenderContext(now=now, width=width, height=height, cx=cx, cy=cy, radius=r,
                             palette=self.palette, slot=slot, schedule=schedule, sun=sun,
                             assets=assets or AssetCache(), search_dirs=dirs)

    def next_change(self, ctx: RenderContext) -> Optional[datetime]:
        times = [t for t in (layer.next_change(ctx) for layer in self.layers if layer.enabled(ctx)) if t]
        return min(times) if times else None
