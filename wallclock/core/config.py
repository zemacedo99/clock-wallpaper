"""User configuration (JSON).  Everything here is data; nothing OS-specific.

Example config.json:

{
  "theme": "themes/minimal.json",
  "schedule": "schedule.txt",            // or an inline {"06:00": "morning.jpg", ...} mapping
  "resolution": "auto",                  // or "1920x1080"
  "output_dir": "~/.wallclock/out",
  "location": {"lat": 41.15, "lon": -8.61},   // optional, for sun/moon layers
  "sun": {"sunrise": "07:00", "sunset": "19:00"},  // alternative to location
  "min_update_seconds": 60,              // battery guard: never update more often than this
  "supersample": 3,                      // anti-aliasing factor for vector layers
  "platform": "auto"                     // windows | macos | linux | file
}
"""
from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from datetime import time
from pathlib import Path
from typing import Any, Dict, Optional, Tuple, Union

from .schedule import Schedule
from .sun import SunProvider
from .timeinfo import parse_hhmm

Resolution = Union[str, Tuple[int, int]]


def parse_resolution(value: Any) -> Resolution:
    if value in (None, "auto"):
        return "auto"
    if isinstance(value, (list, tuple)) and len(value) == 2:
        return int(value[0]), int(value[1])
    m = re.fullmatch(r"\s*(\d+)\s*[xX*]\s*(\d+)\s*", str(value))
    if not m:
        raise ValueError(f"bad resolution {value!r}, expected WIDTHxHEIGHT or 'auto'")
    return int(m.group(1)), int(m.group(2))


@dataclass
class Config:
    base_dir: Path
    theme: str = "themes/minimal.json"
    resolution: Resolution = "auto"
    output_dir: Path = Path("~/.wallclock/out")
    schedule: Optional[Schedule] = None
    location: Optional[Tuple[float, float]] = None
    sun_fixed: Optional[Tuple[time, time]] = None
    min_update_seconds: int = 60
    max_sleep_seconds: int = 3600
    supersample: int = 3
    platform: str = "auto"
    png_compress_level: int = 1
    raw: Dict[str, Any] = field(default_factory=dict)

    def resolve(self, p: Union[str, Path]) -> Path:
        path = Path(p).expanduser()
        return path if path.is_absolute() else (self.base_dir / path)

    @property
    def theme_path(self) -> Path:
        return self.resolve(self.theme)

    @property
    def output_path(self) -> Path:
        return Path(self.output_dir).expanduser() if str(self.output_dir).startswith("~") else self.resolve(self.output_dir)

    def sun_provider(self) -> Optional[SunProvider]:
        if self.location:
            return SunProvider(lat=self.location[0], lon=self.location[1])
        if self.sun_fixed:
            return SunProvider(fixed=self.sun_fixed)
        return None

    # ---- construction -------------------------------------------------
    @classmethod
    def from_dict(cls, data: Dict[str, Any], base_dir: Path) -> "Config":
        cfg = cls(base_dir=base_dir, raw=dict(data))
        cfg.theme = data.get("theme", cfg.theme)
        cfg.resolution = parse_resolution(data.get("resolution"))
        cfg.output_dir = Path(data.get("output_dir", cfg.output_dir))
        cfg.schedule = Schedule.load(data.get("schedule"), base_dir)
        loc = data.get("location")
        if loc:
            cfg.location = (float(loc["lat"]), float(loc["lon"]))
        sun = data.get("sun")
        if sun:
            cfg.sun_fixed = (parse_hhmm(sun["sunrise"]), parse_hhmm(sun["sunset"]))
        cfg.min_update_seconds = int(data.get("min_update_seconds", cfg.min_update_seconds))
        cfg.max_sleep_seconds = int(data.get("max_sleep_seconds", cfg.max_sleep_seconds))
        cfg.supersample = int(data.get("supersample", cfg.supersample))
        cfg.platform = str(data.get("platform", cfg.platform))
        cfg.png_compress_level = int(data.get("png_compress_level", cfg.png_compress_level))
        if cfg.min_update_seconds < 1:
            raise ValueError("min_update_seconds must be >= 1")
        return cfg

    @classmethod
    def load(cls, path: Union[str, Path]) -> "Config":
        p = Path(path).expanduser().resolve()
        with p.open(encoding="utf-8") as fh:
            data = json.load(fh)
        return cls.from_dict(data, p.parent)


DEFAULT_CONFIG_DIR = Path("~/.wallclock").expanduser()
DEFAULT_CONFIG_FILE = DEFAULT_CONFIG_DIR / "config.json"
