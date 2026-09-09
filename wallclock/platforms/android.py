"""Android placeholder adapter.

There is no way for a script (Termux, etc.) to set the wallpaper of the
launcher on stock Android without an app: WallpaperManager.setBitmap() needs
the SET_WALLPAPER permission held by an installed APK.  Two practical routes:

1. Termux + Termux:API (`termux-wallpaper -f file.png`) - works when both apps
   are installed; this adapter uses it if present.  Waking every minute in
   Termux needs `termux-job-scheduler` (>= 15 min) or a wake lock, so on
   Android this is honest but battery-hostile.
2. A tiny native app - see docs/android.md.  The battery-correct design there
   is a WallpaperService that redraws ONE frame per minute, only while visible.
   The theme JSON and schedule are the same files.
"""
from __future__ import annotations

import shutil
import subprocess
from pathlib import Path

from .base import NotSupported, WallpaperAdapter


class AndroidAdapter(WallpaperAdapter):
    name = "android"

    def set_wallpaper(self, path: Path) -> None:
        tool = shutil.which("termux-wallpaper")
        if not tool:
            raise NotSupported("Android: install Termux + Termux:API (termux-wallpaper) or use the native app; see docs/android.md")
        subprocess.run([tool, "-f", str(path)], check=True, timeout=30)
