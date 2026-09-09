import unittest
from datetime import date, datetime, time, timedelta, timezone

from wallclock.core.sun import SunProvider

UTC = timezone.utc


class SunTests(unittest.TestCase):
    def test_porto_summer_solstice(self):
        sun = SunProvider(lat=41.15, lon=-8.61)
        t = sun.times(date(2026, 6, 21), UTC)
        self.assertIsNotNone(t)
        # reference values ~05:00 / 20:08 UTC; allow a few minutes
        self.assertTrue(time(4, 50) <= t.sunrise.time() <= time(5, 12), t.sunrise)
        self.assertTrue(time(19, 58) <= t.sunset.time() <= time(20, 20), t.sunset)
        self.assertEqual(t.sunrise.date(), date(2026, 6, 21))

    def test_equator_roughly_twelve_hours(self):
        sun = SunProvider(lat=0.0, lon=0.0)
        t = sun.times(date(2026, 3, 20), UTC)
        hours = (t.sunset - t.sunrise).total_seconds() / 3600
        self.assertAlmostEqual(hours, 12.1, delta=0.3)

    def test_polar_night_is_none(self):
        sun = SunProvider(lat=80.0, lon=20.0)
        self.assertIsNone(sun.times(date(2026, 12, 21), UTC))

    def test_fixed_times_and_night_interval(self):
        sun = SunProvider(fixed=(time(7, 0), time(19, 0)))
        now = datetime(2026, 9, 9, 23, 0, tzinfo=UTC)
        night = sun.night(now)
        self.assertEqual(night[0], datetime(2026, 9, 9, 19, 0, tzinfo=UTC))
        self.assertEqual(night[1], datetime(2026, 9, 10, 7, 0, tzinfo=UTC))
        early = datetime(2026, 9, 10, 3, 0, tzinfo=UTC)
        self.assertEqual(sun.night(early)[0], datetime(2026, 9, 9, 19, 0, tzinfo=UTC))
        self.assertEqual(sun.daylight(now), (datetime(2026, 9, 9, 7, tzinfo=UTC), datetime(2026, 9, 9, 19, tzinfo=UTC)))


if __name__ == "__main__":
    unittest.main()
