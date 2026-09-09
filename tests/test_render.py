"""Theme loading and render behaviour, checked on tiny canvases by sampling pixels."""
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from PIL import Image

from wallclock.core.schedule import Schedule
from wallclock.core.sun import SunProvider
from wallclock.core.timeinfo import polar
from wallclock.renderer.render import Renderer
from wallclock.themes import Layer, Theme, register_layer
from wallclock.themes.registry import get_layer_class

TZ = timezone(timedelta(hours=1))
INK, PAPER = (42, 42, 42), (239, 233, 223)


def dt(h, m=0, s=0):
    return datetime(2026, 9, 9, h, m, s, tzinfo=TZ)


def close(a, b, tol=12):
    return all(abs(x - y) <= tol for x, y in zip(a, b))


MINIMAL = {
    "name": "t", "palette": {"ink": "#2a2a2a", "paper": "#efe9df"},
    "dial": {"center": [0.5, 0.5], "radius": 0.4},
    "layers": [
        {"type": "background", "source": "schedule", "color": "@paper"},
        {"type": "markers", "count": 12, "style": "dot", "radius": 1.0, "size": 0.08, "color": "@ink"},
        {"type": "hands",
         "hour": {"length": 0.5, "tail": 0.0, "width": 0.08, "color": "@ink", "shape": "rounded"},
         "minute": {"length": 0.8, "tail": 0.0, "width": 0.05, "color": "@ink", "shape": "rounded"}},
    ],
}


class ThemeLoadTests(unittest.TestCase):
    def test_layers_and_geometry(self):
        theme = Theme(MINIMAL)
        self.assertEqual([l.type_name for l in theme.layers], ["background", "markers", "hands"])
        cx, cy, r = theme.geometry(400, 200)
        self.assertEqual((cx, cy, r), (200, 100, 80))  # radius relative to min(w, h)

    def test_unknown_layer_type(self):
        with self.assertRaises(KeyError):
            Theme({"layers": [{"type": "nope"}]})

    def test_plugin_registration(self):
        @register_layer("test-dot")
        class DotLayer(Layer):
            def draw(self, canvas, ctx):
                canvas.circle(ctx.cx, ctx.cy, 5, fill="#ff0000")

            def next_change(self, ctx):
                return ctx.now + timedelta(hours=1)

        self.assertIs(get_layer_class("test-dot"), DotLayer)
        theme = Theme({"palette": {"paper": "#ffffff"}, "layers": [{"type": "test-dot"}]})
        ctx = theme.context(dt(10), 100, 100)
        img = Renderer(supersample=2).render(theme, ctx)
        self.assertTrue(close(img.getpixel((50, 50)), (255, 0, 0)))
        self.assertEqual(theme.next_change(ctx), dt(11))


class RenderTests(unittest.TestCase):
    def setUp(self):
        self.theme = Theme(MINIMAL)
        self.renderer = Renderer(supersample=3)

    def render(self, now, size=(200, 200), **kw):
        ctx = self.theme.context(now, *size, **kw)
        return self.renderer.render(self.theme, ctx), ctx

    def test_hands_follow_time(self):
        img, ctx = self.render(dt(3, 0))           # hour hand -> right, minute hand -> up
        on_hour = polar(ctx.cx, ctx.cy, ctx.radius * 0.35, 90)
        on_minute = polar(ctx.cx, ctx.cy, ctx.radius * 0.6, 0)
        off = polar(ctx.cx, ctx.cy, ctx.radius * 0.6, 225)
        self.assertTrue(close(img.getpixel(on_hour), INK), img.getpixel(on_hour))
        self.assertTrue(close(img.getpixel(on_minute), INK))
        self.assertTrue(close(img.getpixel(off), PAPER))

    def test_markers_present(self):
        img, ctx = self.render(dt(6, 0))
        for i in range(12):
            x, y = polar(ctx.cx, ctx.cy, ctx.radius, i * 30)
            self.assertTrue(close(img.getpixel((round(x), round(y))), INK), i)

    def test_different_times_differ(self):
        a, _ = self.render(dt(10, 10))
        b, _ = self.render(dt(10, 11))
        self.assertNotEqual(list(a.getdata()), list(b.getdata()))

    def test_any_resolution(self):
        for size in ((320, 240), (1080, 1920), (2560, 1080)):
            img, ctx = self.render(dt(9, 0), size=size)
            self.assertEqual(img.size, size)
            x, y = polar(ctx.cx, ctx.cy, ctx.radius * 0.35, 270)   # hour hand to the left at 9:00
            self.assertTrue(close(img.getpixel((round(x), round(y))), INK), size)

    def test_scheduled_background_and_palette(self):
        with tempfile.TemporaryDirectory() as d:
            red, blue = Path(d, "red.png"), Path(d, "blue.png")
            Image.new("RGB", (40, 20), (200, 0, 0)).save(red)
            Image.new("RGB", (40, 20), (0, 0, 200)).save(blue)
            sched = Schedule.from_text(f"09:00 -> red.png\n18:00 -> blue.png | Leave office | ink=#ffffff\n")
            for now, expect_bg, expect_ink in ((dt(12), (200, 0, 0), INK), (dt(19), (0, 0, 200), (255, 255, 255))):
                slot = sched.active(now)
                ctx = self.theme.context(now, 200, 200, slot=slot, schedule=sched, search_dirs=[Path(d)])
                img = self.renderer.render(self.theme, ctx)
                self.assertTrue(close(img.getpixel((5, 5)), expect_bg))
                x, y = polar(ctx.cx, ctx.cy, ctx.radius, 0)   # a marker, coloured with @ink
                self.assertTrue(close(img.getpixel((round(x), round(y))), expect_ink))
                # background layer wakes at the slot boundary; the hands make the theme wake next minute
                self.assertEqual(self.theme.layers[0].next_change(ctx), sched.next_change(now))
                self.assertEqual(self.theme.next_change(ctx), now + timedelta(minutes=1))
            self.assertEqual(sched.active(dt(19)).caption, "Leave office")

    def test_next_change_is_next_minute_for_hands(self):
        _, ctx = self.render(dt(10, 10, 30))
        self.assertEqual(self.theme.next_change(ctx), dt(10, 11))


class OrbiterTests(unittest.TestCase):
    def make(self, driver, **extra):
        spec = {"type": "orbiter", "driver": driver, "radius": 1.0, "arc": [270, 90],
                "shape": {"kind": "circle", "color": "#ffcc00", "size": 0.2}, **extra}
        return Theme({"palette": {"paper": "#000000"}, "dial": {"radius": 0.3}, "layers": [spec]})

    def test_minute_driver_position(self):
        theme = self.make("minute")
        ctx = theme.context(dt(10, 15), 300, 300)
        img = Renderer(supersample=2).render(theme, ctx)
        x, y = polar(ctx.cx, ctx.cy, ctx.radius, 90)       # 15 min -> 3 o'clock
        self.assertTrue(close(img.getpixel((round(x), round(y))), (255, 204, 0)))
        self.assertEqual(theme.layers[0].angle(ctx), 90)
        self.assertEqual(theme.next_change(ctx), dt(10, 16))

    def test_sun_visible_only_in_daylight(self):
        theme = self.make("sun")
        sun = SunProvider(fixed=(__import__("datetime").time(6, 0), __import__("datetime").time(18, 0)))
        day = theme.context(dt(12), 300, 300, sun=sun)
        self.assertAlmostEqual(theme.layers[0].angle(day), 0)          # culminates at 12 o'clock
        self.assertAlmostEqual(theme.layers[0].angle(theme.context(dt(9), 300, 300, sun=sun)), 315)
        self.assertIsNone(theme.layers[0].angle(theme.context(dt(22), 300, 300, sun=sun)))
        self.assertEqual(theme.next_change(theme.context(dt(4), 300, 300, sun=sun)), dt(6))  # wakes at sunrise
        self.assertIsNone(theme.layers[0].angle(theme.context(dt(12), 300, 300)))          # no provider -> hidden


if __name__ == "__main__":
    unittest.main()
