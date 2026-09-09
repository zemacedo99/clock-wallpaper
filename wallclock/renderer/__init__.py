"""Rendering: Canvas (drawing surface), AssetCache, fonts and the Renderer.

`Renderer` is imported lazily (from .render) to avoid a circular import with the
theme package, which needs Canvas/AssetCache at import time.
"""
from .assets import AssetCache, fit_image  # noqa: F401
from .canvas import Canvas, to_color  # noqa: F401
