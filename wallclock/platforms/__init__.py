"""Platform adapters: the only place that knows how a given OS sets a wallpaper."""
from __future__ import annotations

import sys

from .base import NotSupported, WallpaperAdapter


def detect_platform() -> str:
    if sys.platform.startswith("win"):
        return "windows"
    if sys.platform == "darwin":
        return "macos"
    if sys.platform.startswith("linux"):
        # Android's python builds report linux; ANDROID_ROOT is a reliable tell.
        import os
        return "android" if "ANDROID_ROOT" in os.environ else "linux"
    return "file"


def get_adapter(name: str = "auto") -> WallpaperAdapter:
    name = detect_platform() if name in (None, "", "auto") else name.lower()
    if name == "windows":
        from .windows import WindowsAdapter
        return WindowsAdapter()
    if name == "macos":
        from .macos import MacOSAdapter
        return MacOSAdapter()
    if name == "linux":
        from .linux import LinuxAdapter
        return LinuxAdapter()
    if name == "android":
        from .android import AndroidAdapter
        return AndroidAdapter()
    if name == "ios":
        from .ios import IOSAdapter
        return IOSAdapter()
    if name == "file":
        from .file import FileAdapter
        return FileAdapter()
    raise ValueError(f"unknown platform {name!r}")


__all__ = ["WallpaperAdapter", "NotSupported", "get_adapter", "detect_platform"]
