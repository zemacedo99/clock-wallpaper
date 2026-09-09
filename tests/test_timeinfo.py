import unittest
from datetime import datetime, timedelta, timezone

from wallclock.core import timeinfo as ti

TZ = timezone(timedelta(hours=1))


def dt(h, m=0, s=0):
    return datetime(2026, 9, 9, h, m, s, tzinfo=TZ)


class AngleTests(unittest.TestCase):
    def test_noon_and_midnight_are_zero(self):
        for t in (dt(0), dt(12)):
            self.assertEqual(ti.hour_angle(t), 0)
            self.assertEqual(ti.minute_angle(t), 0)
            self.assertEqual(ti.second_angle(t), 0)

    def test_three_oclock(self):
        self.assertAlmostEqual(ti.hour_angle(dt(3)), 90)
        self.assertAlmostEqual(ti.hour_angle(dt(15)), 90)

    def test_smooth_hour_hand_advances_with_minutes(self):
        self.assertAlmostEqual(ti.hour_angle(dt(6, 30)), 195)
        self.assertAlmostEqual(ti.hour_angle(dt(6, 30), smooth=False), 180)

    def test_minute_and_second(self):
        self.assertAlmostEqual(ti.minute_angle(dt(10, 45)), 270)
        self.assertAlmostEqual(ti.minute_angle(dt(10, 45, 30), smooth=True), 273)
        self.assertAlmostEqual(ti.second_angle(dt(0, 0, 15)), 90)

    def test_day_angle(self):
        self.assertAlmostEqual(ti.day_angle(dt(6)), 90)
        self.assertAlmostEqual(ti.day_angle(dt(18)), 270)

    def test_polar_orientation(self):
        x, y = ti.polar(100, 100, 50, 0)
        self.assertAlmostEqual(x, 100); self.assertAlmostEqual(y, 50)     # 12 o'clock is up
        x, y = ti.polar(100, 100, 50, 90)
        self.assertAlmostEqual(x, 150); self.assertAlmostEqual(y, 100)    # 3 o'clock is right
        x, y = ti.polar(100, 100, 50, 180)
        self.assertAlmostEqual(y, 150)


class BoundaryTests(unittest.TestCase):
    def test_next_minute(self):
        self.assertEqual(ti.next_boundary(dt(10, 15, 20), 60), dt(10, 16))
        self.assertEqual(ti.next_boundary(dt(10, 15, 0), 60), dt(10, 16))  # strictly after

    def test_next_day_rollover(self):
        self.assertEqual(ti.next_boundary(dt(23, 59, 30), 60), dt(0) + timedelta(days=1))

    def test_fraction_between(self):
        self.assertEqual(ti.fraction_between(dt(12), dt(8), dt(16)), 0.5)
        self.assertEqual(ti.fraction_between(dt(7), dt(8), dt(16)), 0.0)
        self.assertEqual(ti.fraction_between(dt(20), dt(8), dt(16)), 1.0)

    def test_parse_hhmm(self):
        self.assertEqual(ti.parse_hhmm("18:00").hour, 18)
        self.assertEqual(ti.parse_hhmm("07:05:30").second, 30)
        self.assertEqual(ti.parse_hhmm("24:00").hour, 0)
        with self.assertRaises(ValueError):
            ti.parse_hhmm("25:00")


if __name__ == "__main__":
    unittest.main()
