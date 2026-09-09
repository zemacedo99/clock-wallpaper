import unittest
from datetime import datetime, timedelta, timezone

from wallclock.core.schedule import Schedule

TZ = timezone(timedelta(hours=1))


def dt(h, m=0):
    return datetime(2026, 9, 9, h, m, tzinfo=TZ)


MAPPING = {"06:00": "morning.jpg", "09:00": "work.jpg", "18:00": "leave-office.jpg",
           "20:00": "evening.jpg", "23:00": "night.jpg"}

TEXT = """
# comment line
06:00 -> morning.jpg
09:00 -> work.jpg            # trailing comment
18:00 -> leave-office.jpg | Leave office
23:00 -> night.jpg | Good night | ink=#f0f0f0 tint=#00000060
"""


class ScheduleSelectionTests(unittest.TestCase):
    def setUp(self):
        self.s = Schedule.from_mapping(MAPPING)

    def test_active_slot(self):
        self.assertEqual(self.s.active(dt(8, 59)).image, "morning.jpg")
        self.assertEqual(self.s.active(dt(9, 0)).image, "work.jpg")
        self.assertEqual(self.s.active(dt(18, 0)).image, "leave-office.jpg")
        self.assertEqual(self.s.active(dt(19, 30)).image, "leave-office.jpg")

    def test_wraps_around_midnight(self):
        self.assertEqual(self.s.active(dt(2, 0)).image, "night.jpg")

    def test_next_change(self):
        self.assertEqual(self.s.next_change(dt(17, 59)), dt(18))
        self.assertEqual(self.s.next_change(dt(18, 0)), dt(20))
        self.assertEqual(self.s.next_change(dt(23, 30)), dt(6) + timedelta(days=1))

    def test_unsorted_input_is_sorted(self):
        s = Schedule.from_mapping({"18:00": "b", "06:00": "a"})
        self.assertEqual([x.image for x in s.slots], ["a", "b"])

    def test_duplicates_rejected(self):
        with self.assertRaises(ValueError):
            Schedule.from_mapping({"06:00": "a", "06:00:00": "b"})


class ScheduleParsingTests(unittest.TestCase):
    def test_text_format(self):
        s = Schedule.from_text(TEXT)
        self.assertEqual(len(s), 4)
        leave = s.active(dt(18, 30))
        self.assertEqual(leave.image, "leave-office.jpg")
        self.assertEqual(leave.caption, "Leave office")
        night = s.active(dt(23, 30))
        self.assertEqual(night.palette, {"ink": "#f0f0f0"})
        self.assertEqual(night.tint, "#00000060")
        self.assertEqual(s.active(dt(9, 30)).image, "work.jpg")

    def test_mapping_with_objects(self):
        s = Schedule.from_mapping({"18:00": {"image": "x.png", "caption": "Go", "overlay": "badge.png"}})
        slot = s.active(dt(18, 5))
        self.assertEqual((slot.image, slot.caption, slot.overlay), ("x.png", "Go", "badge.png"))

    def test_bad_line(self):
        with self.assertRaises(ValueError):
            Schedule.from_text("06:00 morning.jpg")

    def test_empty(self):
        s = Schedule.from_mapping({})
        self.assertIsNone(s.active(dt(10)))
        self.assertIsNone(s.next_change(dt(10)))


if __name__ == "__main__":
    unittest.main()
