"""Linux: pick the desktop's own wallpaper command.  All are one-shot CLI calls.

GNOME / Cinnamon / MATE / Budgie  -> gsettings
KDE Plasma                        -> plasma-apply-wallpaperimage (5.26+), else qdbus script
XFCE                              -> xfconf-query (every monitor/workspace property)
sway                              -> swaymsg output * bg
anything else                     -> feh --bg-fill  (if installed)

Set "platform": "linux" and WALLCLOCK_LINUX_BACKEND=gnome|kde|xfce|sway|feh to force one.
"""
from __future__ import annotations

import json
import logging
import os
import re
import shutil
import subprocess
from pathlib import Path
from typing import List, Optional, Tuple

from .base import NotSupported, WallpaperAdapter

log = logging.getLogger(__name__)


def _run(cmd: List[str]) -> str:
    return subprocess.run(cmd, check=True, capture_output=True, text=True, timeout=15).stdout


class LinuxAdapter(WallpaperAdapter):
    name = "linux"

    def __init__(self) -> None:
        self.backend = os.environ.get("WALLCLOCK_LINUX_BACKEND") or self._detect_backend()

    @staticmethod
    def _detect_backend() -> str:
        desktop = (os.environ.get("XDG_CURRENT_DESKTOP", "") + ":" + os.environ.get("DESKTOP_SESSION", "")).lower()
        if os.environ.get("SWAYSOCK") and shutil.which("swaymsg"):
            return "sway"
        if "kde" in desktop or "plasma" in desktop:
            return "kde"
        if "xfce" in desktop:
            return "xfce"
        if any(k in desktop for k in ("gnome", "cinnamon", "mate", "budgie", "unity", "pantheon")) and shutil.which("gsettings"):
            return "gnome"
        if shutil.which("gsettings"):
            return "gnome"
        if shutil.which("feh"):
            return "feh"
        return "none"

    def describe(self) -> str:
        return f"linux/{self.backend}"

    def screen_size(self) -> Optional[Tuple[int, int]]:
        try:
            if self.backend == "sway":
                outputs = json.loads(_run(["swaymsg", "-t", "get_outputs"]))
                for o in outputs:
                    if o.get("active") and o.get("current_mode"):
                        return int(o["current_mode"]["width"]), int(o["current_mode"]["height"])
            if shutil.which("xrandr"):
                out = _run(["xrandr", "--query"])
                m = re.search(r" connected primary (\d+)x(\d+)", out) or re.search(r" connected (\d+)x(\d+)", out)
                if m:
                    return int(m.group(1)), int(m.group(2))
        except Exception as exc:  # pragma: no cover
            log.debug("screen detection failed: %s", exc)
        return None

    def set_wallpaper(self, path: Path) -> None:
        p = str(Path(path).resolve())
        uri = f"file://{p}"
        b = self.backend
        if b == "gnome":
            desktop = os.environ.get("XDG_CURRENT_DESKTOP", "").lower()
            schema = "org.cinnamon.desktop.background" if "cinnamon" in desktop else (
                "org.mate.background" if "mate" in desktop else "org.gnome.desktop.background")
            key = "picture-filename" if schema == "org.mate.background" else "picture-uri"
            value = p if schema == "org.mate.background" else uri
            _run(["gsettings", "set", schema, key, value])
            if schema == "org.gnome.desktop.background":
                subprocess.run(["gsettings", "set", schema, "picture-uri-dark", uri], capture_output=True)
                subprocess.run(["gsettings", "set", schema, "picture-options", "zoom"], capture_output=True)
        elif b == "kde":
            if shutil.which("plasma-apply-wallpaperimage"):
                _run(["plasma-apply-wallpaperimage", p])
            else:
                script = ('var all = desktops(); for (var i = 0; i < all.length; i++) { var d = all[i];'
                          'd.wallpaperPlugin = "org.kde.image"; d.currentConfigGroup = ["Wallpaper", "org.kde.image", "General"];'
                          f'd.writeConfig("Image", "{uri}"); }}')
                tool = shutil.which("qdbus6") or shutil.which("qdbus") or "qdbus"
                _run([tool, "org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", script])
        elif b == "xfce":
            props = _run(["xfconf-query", "-c", "xfce4-desktop", "-l"]).split()
            targets = [x for x in props if x.endswith("/last-image")] or ["/backdrop/screen0/monitor0/workspace0/last-image"]
            for prop in targets:
                _run(["xfconf-query", "-c", "xfce4-desktop", "-p", prop, "-s", p])
        elif b == "sway":
            _run(["swaymsg", "output", "*", "bg", p, "fill"])
        elif b == "feh":
            _run(["feh", "--bg-fill", p])
        else:
            raise NotSupported("no known wallpaper backend found (install feh or set WALLCLOCK_LINUX_BACKEND)")
