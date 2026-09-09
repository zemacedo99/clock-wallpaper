"""Windows: SystemParametersInfoW(SPI_SETDESKWALLPAPER) via ctypes.  No dependencies.

Applies to all monitors as one wallpaper (Windows stretches/fills per monitor
using the style set in the registry; we render at the primary monitor's size
and select the "Fill" style).  Per-monitor wallpapers would need the
IDesktopWallpaper COM interface; see README "Platform notes".
"""
from __future__ import annotations

import ctypes
import logging
from pathlib import Path
from typing import Optional, Tuple

from .base import WallpaperAdapter

log = logging.getLogger(__name__)

SPI_SETDESKWALLPAPER = 0x0014
SPIF_UPDATEINIFILE = 0x01
SPIF_SENDCHANGE = 0x02


class WindowsAdapter(WallpaperAdapter):
    name = "windows"

    def __init__(self) -> None:
        self._style_set = False

    def screen_size(self) -> Optional[Tuple[int, int]]:
        try:
            user32 = ctypes.windll.user32  # type: ignore[attr-defined]
            try:  # ask for physical pixels, not DPI-scaled ones
                ctypes.windll.shcore.SetProcessDpiAwareness(2)  # type: ignore[attr-defined]
            except Exception:
                user32.SetProcessDPIAware()
            return int(user32.GetSystemMetrics(0)), int(user32.GetSystemMetrics(1))
        except Exception as exc:  # pragma: no cover
            log.warning("could not detect screen size: %s", exc)
            return None

    def _ensure_fill_style(self) -> None:
        if self._style_set:
            return
        try:
            import winreg
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Control Panel\Desktop", 0, winreg.KEY_SET_VALUE) as k:
                winreg.SetValueEx(k, "WallpaperStyle", 0, winreg.REG_SZ, "10")  # Fill
                winreg.SetValueEx(k, "TileWallpaper", 0, winreg.REG_SZ, "0")
        except OSError as exc:
            log.debug("could not set wallpaper style: %s", exc)
        self._style_set = True

    def set_wallpaper(self, path: Path) -> None:
        self._ensure_fill_style()
        ok = ctypes.windll.user32.SystemParametersInfoW(  # type: ignore[attr-defined]
            SPI_SETDESKWALLPAPER, 0, str(Path(path).resolve()), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)
        if not ok:
            raise OSError(f"SystemParametersInfoW failed (error {ctypes.GetLastError()})")
