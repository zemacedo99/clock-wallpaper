"""Time-of-day schedule rules, e.g.  18:00 -> leave-office.jpg

A schedule is an ordered list of slots.  The active slot is the last one whose
start time is <= the current time of day; before the first slot, the last slot
of the (previous) day is active, so the schedule wraps around midnight.

Slots can carry more than a background: a caption, an overlay image, a tint,
or palette overrides that layers consult through the render context.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from datetime import datetime, time, timedelta
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Union

from .timeinfo import at_time, parse_hhmm

_COMMENT = re.compile(r"(^|\s)#.*$")


@dataclass(frozen=True)
class Slot:
    start: time
    image: Optional[str] = None
    caption: Optional[str] = None
    overlay: Optional[str] = None
    tint: Optional[str] = None
    palette: Dict[str, str] = field(default_factory=dict)
    extra: Dict[str, Any] = field(default_factory=dict)

    @property
    def label(self) -> str:
        return self.start.strftime("%H:%M")


def _slot_from_value(start: time, value: Any) -> Slot:
    if value is None:
        return Slot(start)
    if isinstance(value, str):
        return Slot(start, image=value)
    if isinstance(value, Mapping):
        known = {"image", "background", "caption", "text", "overlay", "tint", "palette"}
        extra = {k: v for k, v in value.items() if k not in known}
        return Slot(
            start,
            image=value.get("image", value.get("background")),
            caption=value.get("caption", value.get("text")),
            overlay=value.get("overlay"),
            tint=value.get("tint"),
            palette=dict(value.get("palette") or {}),
            extra=extra,
        )
    raise ValueError(f"unsupported schedule value for {start}: {value!r}")


class Schedule:
    def __init__(self, slots: List[Slot]):
        self.slots: List[Slot] = sorted(slots, key=lambda s: s.start)
        starts = [s.start for s in self.slots]
        if len(set(starts)) != len(starts):
            raise ValueError("duplicate slot start times in schedule")

    # ---- construction -------------------------------------------------
    @classmethod
    def from_mapping(cls, mapping: Mapping[str, Any]) -> "Schedule":
        return cls([_slot_from_value(parse_hhmm(k), v) for k, v in mapping.items()])

    @classmethod
    def from_text(cls, text: str) -> "Schedule":
        """Parse the plain-text format:

            # comment
            06:00 -> morning.jpg
            18:00 -> leave-office.jpg | Leave office
            23:00 -> night.jpg | Good night | ink=#f0f0f0 tint=#00000060

        Fields after the image are optional: a caption, then key=value extras.
        `tint` and `overlay` are recognised; any other key becomes a palette
        override (e.g. ink=#fff makes '@ink' colours white while the slot is active).
        """
        slots: List[Slot] = []
        for raw in text.splitlines():
            # a '#' starts a comment only at line start or after whitespace, so ink=#fff survives
            line = _COMMENT.sub("", raw).strip()
            if not line:
                continue
            if "->" not in line:
                raise ValueError(f"schedule line without '->': {raw!r}")
            when, rest = line.split("->", 1)
            parts = [p.strip() for p in rest.split("|")]
            image = parts[0] or None
            caption = parts[1] if len(parts) > 1 and parts[1] else None
            palette: Dict[str, str] = {}
            fields: Dict[str, Any] = {}
            if len(parts) > 2:
                for token in parts[2].split():
                    if "=" not in token:
                        raise ValueError(f"bad extra {token!r} in {raw!r}")
                    k, v = token.split("=", 1)
                    if k in ("tint", "overlay"):
                        fields[k] = v
                    else:
                        palette[k] = v
            slots.append(Slot(parse_hhmm(when), image=image, caption=caption,
                              overlay=fields.get("overlay"), tint=fields.get("tint"),
                              palette=palette))
        return cls(slots)

    @classmethod
    def load(cls, source: Union[str, Path, Mapping[str, Any], None],
             base_dir: Optional[Path] = None) -> Optional["Schedule"]:
        """Accepts a mapping, a path to a .json/.txt file, or None."""
        if source is None:
            return None
        if isinstance(source, Mapping):
            return cls.from_mapping(source)
        path = Path(source).expanduser()
        if base_dir is not None and not path.is_absolute():
            path = base_dir / path
        text = path.read_text(encoding="utf-8")
        if path.suffix.lower() == ".json":
            import json
            return cls.from_mapping(json.loads(text))
        return cls.from_text(text)

    # ---- queries ------------------------------------------------------
    def __len__(self) -> int:
        return len(self.slots)

    def active(self, now: datetime) -> Optional[Slot]:
        if not self.slots:
            return None
        t = now.time().replace(microsecond=0)
        current = None
        for slot in self.slots:
            if slot.start <= t:
                current = slot
            else:
                break
        return current if current is not None else self.slots[-1]

    def next_change(self, now: datetime) -> Optional[datetime]:
        """Instant at which the active slot changes next (strictly after `now`)."""
        if not self.slots:
            return None
        t = now.time()
        for slot in self.slots:
            if slot.start > t:
                return at_time(now.date(), slot.start, now.tzinfo)
        return at_time(now.date() + timedelta(days=1), self.slots[0].start, now.tzinfo)

    def image_paths(self) -> List[str]:
        return [s.image for s in self.slots if s.image]
