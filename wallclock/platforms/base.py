from __future__ import annotations

from pathlib import Path
from typing import Optional, Tuple


class NotSupported(RuntimeError):
    """Raised when the OS offers no way to set the wallpaper programmatically."""


class WallpaperAdapter:
    """Contract for platform adapters.  Keep implementations tiny."""

    name = "base"
    preferred_format = "png"

    def screen_size(self) -> Optional[Tuple[int, int]]:
        """Primary display size in physical pixels, or None when unknown."""
        return None

    def set_wallpaper(self, path: Path) -> None:
        raise NotSupported(f"{self.name}: setting the wallpaper is not supported")

    def describe(self) -> str:
        return self.name
