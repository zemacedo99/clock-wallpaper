"""Decides *when* the next render is needed and sleeps until then.

There is no polling loop: after each update the theme reports the next instant
its output changes (minute boundary for hands, slot boundary for a scheduled
background, sunrise for a sun layer, ...).  The process sleeps once, for
exactly that long, in a single blocking sleep.  CPU use between updates is zero.

Two guards:
  * `min_update_seconds` (default 60) - a theme cannot force faster updates;
    if it asks for less, the wake is aligned to the next multiple of the guard.
  * `max_sleep_seconds` (default 3600) - an occasional wake keeps the display
    correct after suspend/resume or clock changes, at negligible cost.
"""
from __future__ import annotations

import logging
import time
from datetime import datetime, timedelta
from typing import Callable, Optional

from .core.timeinfo import local_now, next_boundary

log = logging.getLogger(__name__)


def clamp_next_wake(now: datetime, proposed: Optional[datetime],
                    min_interval: int = 60, max_sleep: int = 3600) -> datetime:
    """Apply the battery guard and the max-sleep cap to a proposed wake time."""
    latest = now + timedelta(seconds=max_sleep)
    if proposed is None or proposed > latest:
        return latest
    if (proposed - now).total_seconds() < min_interval:
        # too soon: wake at the next guard-aligned instant instead (e.g. the next full minute)
        return next_boundary(now, min_interval)
    return proposed


def run_loop(update: Callable[[datetime], Optional[datetime]], min_interval: int = 60,
             max_sleep: int = 3600, sleep: Callable[[float], None] = time.sleep,
             clock: Callable[[], datetime] = local_now, iterations: Optional[int] = None) -> None:
    """update(now) renders + applies and returns the next proposed wake time."""
    n = 0
    while iterations is None or n < iterations:
        n += 1
        now = clock()
        try:
            proposed = update(now)
        except Exception:
            log.exception("update failed; retrying at the next boundary")
            proposed = None
        wake = clamp_next_wake(clock(), proposed, min_interval, max_sleep)
        delay = max(0.05, (wake - clock()).total_seconds())
        log.debug("next update at %s (in %.1fs)", wake.isoformat(timespec="seconds"), delay)
        sleep(delay)
