"""Pure time -> geometry helpers.  No I/O, no platform code, no Pillow.

Angles are in degrees, measured clockwise from 12 o'clock, in [0, 360).
Screen coordinates have the y axis pointing down, which `polar()` accounts for.
"""
from __future__ import annotations

import math
from datetime import date, datetime, time, timedelta, tzinfo
from typing import Optional, Tuple

SECONDS_PER_DAY = 86_400


def seconds_since_midnight(t: datetime) -> float:
    return t.hour * 3600 + t.minute * 60 + t.second + t.microsecond / 1e6


def fraction_of_day(t: datetime) -> float:
    return seconds_since_midnight(t) / SECONDS_PER_DAY


def hour_angle(t: datetime, smooth: bool = True) -> float:
    """Angle of the hour hand. `smooth` advances it with the minutes (as a real clock does)."""
    h = t.hour % 12
    if smooth:
        return ((h + t.minute / 60 + t.second / 3600) * 30) % 360
    return (h * 30) % 360


def minute_angle(t: datetime, smooth: bool = False) -> float:
    """Angle of the minute hand. `smooth` advances it with the seconds."""
    m = t.minute + (t.second / 60 if smooth else 0)
    return (m * 6) % 360


def second_angle(t: datetime) -> float:
    return (t.second * 6) % 360


def day_angle(t: datetime) -> float:
    """One full turn per 24 h (midnight at the top)."""
    return (fraction_of_day(t) * 360) % 360


def polar(cx: float, cy: float, r: float, angle_deg: float) -> Tuple[float, float]:
    """Point at distance r from (cx, cy) at a clock angle (0 = up, clockwise)."""
    a = math.radians(angle_deg)
    return (cx + r * math.sin(a), cy - r * math.cos(a))


def next_boundary(now: datetime, period_seconds: float) -> datetime:
    """First instant strictly after `now` whose seconds-since-midnight is a multiple of the period."""
    if period_seconds <= 0:
        raise ValueError("period must be positive")
    s = seconds_since_midnight(now)
    k = math.floor(s / period_seconds) + 1
    midnight = now.replace(hour=0, minute=0, second=0, microsecond=0)
    return midnight + timedelta(seconds=k * period_seconds)


def fraction_between(now: datetime, start: datetime, end: datetime) -> float:
    """0 at `start`, 1 at `end`, clamped."""
    total = (end - start).total_seconds()
    if total <= 0:
        return 0.0
    return max(0.0, min(1.0, (now - start).total_seconds() / total))


def parse_hhmm(text: str) -> time:
    """Parse 'HH:MM' or 'HH:MM:SS' (24 h). '24:00' is accepted and means 00:00."""
    parts = text.strip().split(":")
    if len(parts) not in (2, 3):
        raise ValueError(f"bad time {text!r}, expected HH:MM")
    h, m = int(parts[0]), int(parts[1])
    s = int(parts[2]) if len(parts) == 3 else 0
    if h == 24 and m == 0 and s == 0:
        h = 0
    if not (0 <= h < 24 and 0 <= m < 60 and 0 <= s < 60):
        raise ValueError(f"bad time {text!r}")
    return time(h, m, s)


def at_time(day: date, t: time, tz: Optional[tzinfo]) -> datetime:
    return datetime.combine(day, t).replace(tzinfo=tz)


def local_now() -> datetime:
    """Aware local time."""
    return datetime.now().astimezone()
