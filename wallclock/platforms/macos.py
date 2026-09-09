"""macOS: AppleScript through `osascript` (ships with the OS, no pyobjc needed).

Limitations: macOS caches the wallpaper by path (hence the a/b file
alternation in the writer), and per-Space/per-display images are set for
"every desktop" here.  Screen detection uses `system_profiler`, which takes
about a second, so it runs once per process and is cached.
"""
from __future__ import annotations

import json
import logging
import subprocess
from pathlib import Path
from typing import Optional, Tuple

from .base import WallpaperAdapter

log = logging.getLogger(__name__)


class MacOSAdapter(WallpaperAdapter):
    name = "macos"

    def __init__(self) -> None:
        self._size: Optional[Tuple[int, int]] = None

    def screen_size(self) -> Optional[Tuple[int, int]]:
        if self._size:
            return self._size
        try:
            out = subprocess.run(["system_profiler", "SPDisplaysDataType", "-json"],
                                 capture_output=True, text=True, timeout=15, check=True).stdout
            data = json.loads(out)
            for gpu in data.get("SPDisplaysDataType", []):
                for disp in gpu.get("spdisplays_ndrvs", []):
                    text = disp.get("_spdisplays_pixels") or disp.get("_spdisplays_resolution") or ""
                    parts = text.replace("x", " ").split()
                    nums = [int(p) for p in parts if p.isdigit()]
                    if len(nums) >= 2:
                        self._size = (nums[0], nums[1])
                        return self._size
        except Exception as exc:  # pragma: no cover
            log.warning("could not detect screen size: %s", exc)
        return None

    def set_wallpaper(self, path: Path) -> None:
        posix = str(Path(path).resolve()).replace('"', '\\"')
        script = f'tell application "System Events" to tell every desktop to set picture to POSIX file "{posix}"'
        subprocess.run(["osascript", "-e", script], check=True, capture_output=True, timeout=15)
