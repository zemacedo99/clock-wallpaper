"""iOS placeholder adapter.

iOS gives third-party code NO API to set the wallpaper, and no background
process may run every minute.  The closest practical approach is to
pre-render images with `wallclock export` and let the Shortcuts app switch
wallpapers with time-of-day Personal Automations.  See docs/ios.md.
"""
from __future__ import annotations

from pathlib import Path

from .base import NotSupported, WallpaperAdapter


class IOSAdapter(WallpaperAdapter):
    name = "ios"

    def set_wallpaper(self, path: Path) -> None:
        raise NotSupported("iOS: no wallpaper API. Use `wallclock export` + Shortcuts automations; see docs/ios.md")
