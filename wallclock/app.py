"""Wires config + theme + renderer + platform adapter together."""
from __future__ import annotations

import logging
from datetime import datetime
from pathlib import Path
from typing import Optional, Tuple

from PIL import Image

from .core.config import Config
from .core.timeinfo import local_now
from .platforms import WallpaperAdapter, get_adapter
from .renderer.assets import AssetCache
from .renderer.render import OutputWriter, Renderer
from .scheduler import clamp_next_wake, run_loop
from .themes import Theme

log = logging.getLogger(__name__)
FALLBACK_RESOLUTION = (1920, 1080)


class App:
    def __init__(self, config: Config, adapter: Optional[WallpaperAdapter] = None,
                 resolution: Optional[Tuple[int, int]] = None):
        self.config = config
        self.theme = Theme.load(config.theme_path)
        self.adapter = adapter or get_adapter(config.platform)
        self.renderer = Renderer(supersample=config.supersample)
        self.sun = config.sun_provider()
        out = config.output_path
        self.assets = AssetCache(disk_dir=out / "cache")
        self.writer = OutputWriter(out, fmt=self.adapter.preferred_format,
                                   compress_level=config.png_compress_level)
        self._resolution = resolution

    # ---- resolution ----------------------------------------------------
    def resolution(self) -> Tuple[int, int]:
        if self._resolution:
            return self._resolution
        if self.config.resolution != "auto":
            self._resolution = self.config.resolution  # type: ignore[assignment]
        else:
            detected = self.adapter.screen_size()
            if not detected:
                log.warning("could not detect screen size; using %sx%s (set 'resolution' in config)", *FALLBACK_RESOLUTION)
            self._resolution = detected or FALLBACK_RESOLUTION
        return self._resolution  # type: ignore[return-value]

    # ---- rendering -----------------------------------------------------
    def context(self, now: datetime, size: Optional[Tuple[int, int]] = None):
        w, h = size or self.resolution()
        slot = self.config.schedule.active(now) if self.config.schedule else None
        return self.theme.context(now, w, h, slot=slot, schedule=self.config.schedule, sun=self.sun,
                                  assets=self.assets, search_dirs=[self.config.base_dir])

    def render(self, now: Optional[datetime] = None, size: Optional[Tuple[int, int]] = None) -> Image.Image:
        return self.renderer.render(self.theme, self.context(now or local_now(), size))

    def next_change(self, now: datetime) -> Optional[datetime]:
        ctx = self.context(now)
        candidates = [self.theme.next_change(ctx)]
        if self.config.schedule:
            candidates.append(self.config.schedule.next_change(now))
        valid = [c for c in candidates if c is not None]
        return min(valid) if valid else None

    def update(self, now: Optional[datetime] = None) -> Optional[datetime]:
        """Render for `now`, apply as wallpaper, return the next proposed change."""
        now = now or local_now()
        img = self.render(now)
        path = self.writer.write(img)
        self.adapter.set_wallpaper(path)
        nxt = self.next_change(now)
        log.info("wallpaper updated for %s -> %s (next change %s)", now.strftime("%H:%M:%S"), path.name,
                 nxt.strftime("%H:%M:%S") if nxt else "none")
        return nxt

    def run_once(self) -> datetime:
        now = local_now()
        proposed = self.update(now)
        return clamp_next_wake(now, proposed, self.config.min_update_seconds, self.config.max_sleep_seconds)

    def run_forever(self) -> None:
        run_loop(self.update, self.config.min_update_seconds, self.config.max_sleep_seconds)
