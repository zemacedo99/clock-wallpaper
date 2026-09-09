"""Sunrise / sunset without dependencies (NOAA-style sunrise equation, ~1-2 min accuracy).

Used by themes that place a sun or moon on the dial.  If no location is
configured, fixed times can be supplied instead.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from datetime import date, datetime, time, timedelta, timezone, tzinfo
from typing import Dict, Optional, Tuple

from .timeinfo import at_time

_J2000 = 2451545.0
_UNIX_EPOCH_JD = 2440587.5


def _julian_day(d: date) -> float:
    """Julian date at 00:00 UTC of a civil date (2000-01-01 -> 2451544.5)."""
    return d.toordinal() + 1721424.5


def _from_julian(jd: float, tz: Optional[tzinfo]) -> datetime:
    ts = (jd - _UNIX_EPOCH_JD) * 86400.0
    return datetime.fromtimestamp(ts, tz=timezone.utc).astimezone(tz)


def sun_times_utc(d: date, lat: float, lon: float) -> Optional[Tuple[float, float]]:
    """Return (sunrise_jd, sunset_jd) or None for polar day/night."""
    n = math.ceil(_julian_day(d) - _J2000 + 0.0008)
    j_star = n - lon / 360.0          # lon: east positive
    M = math.radians((357.5291 + 0.98560028 * j_star) % 360)
    C = 1.9148 * math.sin(M) + 0.02 * math.sin(2 * M) + 0.0003 * math.sin(3 * M)
    lam = math.radians((math.degrees(M) + C + 180 + 102.9372) % 360)
    j_transit = _J2000 + j_star + 0.0053 * math.sin(M) - 0.0069 * math.sin(2 * lam)
    sin_dec = math.sin(lam) * math.sin(math.radians(23.4397))
    dec = math.asin(sin_dec)
    phi = math.radians(lat)
    cos_w0 = (math.sin(math.radians(-0.833)) - math.sin(phi) * sin_dec) / (math.cos(phi) * math.cos(dec))
    if cos_w0 > 1 or cos_w0 < -1:
        return None
    w0 = math.degrees(math.acos(cos_w0))
    return j_transit - w0 / 360.0, j_transit + w0 / 360.0


@dataclass(frozen=True)
class SunTimes:
    sunrise: datetime
    sunset: datetime


class SunProvider:
    """Answers sunrise/sunset for any local date, computed lazily and memoised."""

    def __init__(self, lat: Optional[float] = None, lon: Optional[float] = None,
                 fixed: Optional[Tuple[time, time]] = None):
        if lat is None and fixed is None:
            raise ValueError("SunProvider needs either a location or fixed times")
        self.lat, self.lon, self.fixed = lat, lon, fixed
        self._cache: Dict[date, Optional[SunTimes]] = {}

    def times(self, d: date, tz: Optional[tzinfo]) -> Optional[SunTimes]:
        if d in self._cache:
            return self._cache[d]
        if self.fixed is not None:
            result: Optional[SunTimes] = SunTimes(at_time(d, self.fixed[0], tz), at_time(d, self.fixed[1], tz))
        else:
            jd = sun_times_utc(d, self.lat, self.lon)  # type: ignore[arg-type]
            result = None if jd is None else SunTimes(_from_julian(jd[0], tz), _from_julian(jd[1], tz))
        self._cache[d] = result
        return result

    def daylight(self, now: datetime) -> Optional[Tuple[datetime, datetime]]:
        """(sunrise, sunset) of `now`'s date, or None."""
        t = self.times(now.date(), now.tzinfo)
        return (t.sunrise, t.sunset) if t else None

    def night(self, now: datetime) -> Optional[Tuple[datetime, datetime]]:
        """The night interval that contains `now`, or that starts next if it is day."""
        today = self.times(now.date(), now.tzinfo)
        if today is None:
            return None
        if now < today.sunrise:            # night that started yesterday
            prev = self.times(now.date() - timedelta(days=1), now.tzinfo)
            if prev is None:
                return None
            return prev.sunset, today.sunrise
        nxt = self.times(now.date() + timedelta(days=1), now.tzinfo)
        if nxt is None:
            return None
        return today.sunset, nxt.sunrise
