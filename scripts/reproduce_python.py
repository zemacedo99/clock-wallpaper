"""Read-only behavioral probe against an extracted original Python branch."""
import json
import sys
from datetime import datetime, timedelta, timezone, time
from pathlib import Path

sys.path.insert(0, str(Path(sys.argv[1]).resolve()))
from wallclock.scheduler import run_loop
from wallclock.themes import Theme
from wallclock.core.sun import SunProvider

now = datetime(2026, 9, 9, 10, 0, tzinfo=timezone.utc)
clock = [now]
sleeps = []
calls = []

def update(t):
    calls.append(t.isoformat())
    if len(calls) == 2:
        raise OSError("injected transient wallpaper error")
    return t.replace(second=0) + timedelta(minutes=1)

def sleep(seconds):
    sleeps.append(seconds)
    clock[0] += timedelta(seconds=seconds)

run_loop(update, clock=lambda: clock[0], sleep=sleep, iterations=3)
sun = Theme({"layers": [{"type": "orbiter", "driver": "sun"}]})
ctx = sun.context(now.replace(hour=22), 200, 200,
                  sun=SunProvider(fixed=(time(6), time(18))))
print(json.dumps({"update_times": calls, "sleep_seconds": sleeps,
                  "sun_only_next_after_sunset": str(sun.next_change(ctx))}, indent=2))
assert sleeps[1] == 3600, "Original hourly-retry bug no longer reproduces"
