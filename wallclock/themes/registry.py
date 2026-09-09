"""Layer type registry.  New visual concepts register here, the core never changes.

    from wallclock.themes import register_layer, Layer

    @register_layer("comet")
    class CometLayer(Layer):
        def draw(self, canvas, ctx): ...
        def next_change(self, ctx): ...

A theme file can load such a module with  "plugins": ["my_package.comet"].
"""
from __future__ import annotations

import importlib
from typing import Callable, Dict, Type, TYPE_CHECKING

if TYPE_CHECKING:  # pragma: no cover
    from .base import Layer

_REGISTRY: Dict[str, Type["Layer"]] = {}


def register_layer(type_name: str) -> Callable[[Type["Layer"]], Type["Layer"]]:
    def deco(cls: Type["Layer"]) -> Type["Layer"]:
        cls.type_name = type_name
        _REGISTRY[type_name] = cls
        return cls
    return deco


def get_layer_class(type_name: str) -> Type["Layer"]:
    try:
        return _REGISTRY[type_name]
    except KeyError:
        raise KeyError(f"unknown layer type {type_name!r}; known: {sorted(_REGISTRY)}") from None


def registered_types() -> list:
    return sorted(_REGISTRY)


def load_plugins(modules) -> None:
    for name in modules or ():
        importlib.import_module(name)
