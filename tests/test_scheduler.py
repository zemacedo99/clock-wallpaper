import unittest
from datetime import datetime, timedelta, timezone

from wallclock.scheduler import clamp_next_wake, run_loop

TZ = timezone(timedelta(hours=1))


def dt(h, m=0, s=0):
    return datetime(2026, 9, 9, h, m, s, tzinfo=TZ)


class ClampTests(unittest.TestCase):
    def test_passes_through_reasonable_times(self):
        self.assertEqual(clamp_next_wake(dt(10, 0, 0), dt(10, 1)), dt(10, 1))
        self.assertEqual(clamp_next_wake(dt(10, 0, 30), dt(10, 5)), dt(10, 5))

    def test_too_frequent_is_aligned_to_guard(self):
        # theme asks for a per-second update; guard is 60 s -> next full minute
        self.assertEqual(clamp_next_wake(dt(10, 0, 30), dt(10, 0, 31), min_interval=60), dt(10, 1))

    def test_none_and_far_future_capped(self):
        self.assertEqual(clamp_next_wake(dt(10), None, max_sleep=3600), dt(11))
        self.assertEqual(clamp_next_wake(dt(10), dt(20), max_sleep=3600), dt(11))


class LoopTests(unittest.TestCase):
    def test_loop_sleeps_until_reported_change(self):
        clock = {"now": dt(10, 0, 20)}
        sleeps, updates = [], []

        def update(now):
            updates.append(now)
            return now.replace(second=0) + timedelta(minutes=1)

        def sleep(seconds):
            sleeps.append(seconds)
            clock["now"] += timedelta(seconds=seconds)

        run_loop(update, min_interval=60, sleep=sleep, clock=lambda: clock["now"], iterations=3)
        self.assertEqual(len(updates), 3)
        self.assertEqual(sleeps, [40.0, 60.0, 60.0])   # one sleep per change, no polling

    def test_update_error_does_not_kill_loop(self):
        calls = []

        def update(now):
            calls.append(now)
            raise RuntimeError("boom")

        with self.assertLogs("wallclock.scheduler", level="ERROR"):
            run_loop(update, sleep=lambda s: None, clock=lambda: dt(10), iterations=2)
        self.assertEqual(len(calls), 2)


if __name__ == "__main__":
    unittest.main()
