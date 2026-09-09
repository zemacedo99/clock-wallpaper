"""Null adapter: only writes the image.  Used for previews, exports and CI."""
from __future__ import annotations

import logging
from pathlib import Path

from .base import WallpaperAdapter

log = logging.getLogger(__name__)


class FileAdapter(WallpaperAdapter):
    name = "file"

    def set_wallpaper(self, path: Path) -> None:
        log.info("wallpaper written to %s (file adapter: not applied)", path)
