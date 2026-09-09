# wallclock - a dynamic clock wallpaper that does not cost battery

The wallpaper *is* the clock.  `wallclock` renders a static image (background +
hour markers + hands + optional text/graphics), sets it as the desktop
wallpaper, and then does nothing at all until the picture would look different
- normally the next full minute.  There is no live-wallpaper engine, no
rendering loop and no polling timer.

```
06:00 -> morning.jpg
09:00 -> work.jpg
18:00 -> leave-office.jpg | Leave office
20:00 -> evening.jpg
23:00 -> night.jpg
```

The hands are always drawn from the real current time on top of whichever
background the schedule selects.

## Why Python + Pillow, and why it is light

* **Work only when something changes.**  Each layer of a theme reports *when its
  output changes next* (minute boundary, schedule boundary, sunrise, ...).  The
  scheduler sleeps once, in a single blocking `sleep`, until the earliest of
  those.  Between updates the process uses 0 % CPU.  A resident `wallclock run`
  is ~30 MB RSS; alternatively `wallclock once` from cron / Task Scheduler /
  launchd needs no resident process at all.
* **A render is cheap.**  The scaled background is cached (memory and disk), so a
  minute update is: draw a few polygons on a small supersampled layer around the
  dial, composite, write a PNG, call one OS function.  Measured on a laptop:
  about 120-170 ms per update at 1080p with the default 3x supersampling; lower
  `supersample` to 2 to roughly halve it.  Once a minute, that is well under
  0.3 % of one core.
* **One dependency.**  Pillow is the only non-stdlib package.  Platform adapters
  use `ctypes` (Windows), `osascript` (macOS) and the desktop's own CLI tools
  (Linux) - no GUI toolkit, no Electron, no browser.
* **Portable core.**  Time maths, schedule rules, theme model and layers have no
  OS or Pillow-specific logic (Pillow is wrapped in `renderer/canvas.py`).  The
  same theme + schedule files can be consumed by a native Android/iOS port; see
  `docs/`.

The alternative of Go/Rust would make a smaller binary, but the process runs
for a fraction of a second per minute either way; Python keeps themes hackable
and the code short.

## Layout

```
wallclock/
  core/        timeinfo.py (time -> angles), schedule.py (rules), sun.py, config.py
  renderer/    canvas.py (drawing surface, anti-aliasing), assets.py (cache), render.py
  themes/      base.py (Theme, Layer, RenderContext), registry.py, layers/*.py
  platforms/   windows.py, macos.py, linux.py, android.py, ios.py, file.py
  scheduler.py compute next wake, sleep until then
  app.py       wires everything; __main__.py is the CLI
examples/      config.json, schedule.txt, themes/{minimal,sky,car}.json, backgrounds/, sprites/
tests/         unittest suite (python -m unittest)
docs/          android.md, ios.md
```

## Installation

Requires Python 3.9+.

```bash
pip install .
```

or without installing: `python -m wallclock ...` from the repository root
(needs `pip install Pillow`).

```bash
wallclock init                       # copies examples/ to ~/.wallclock
wallclock -c ~/.wallclock/config.json info
wallclock -c ~/.wallclock/config.json once     # render + set wallpaper, exit
wallclock -c ~/.wallclock/config.json run      # stay resident, update when needed
```

`-c` defaults to `~/.wallclock/config.json`, so after `init` it can be omitted.

### Keeping it running

*Windows* - Task Scheduler, trigger "At log on", action:
`pythonw -m wallclock run` (or `wallclock.exe run` if installed with pip).
Or trigger every minute with `once` if you prefer no resident process.

*macOS* - a LaunchAgent with `KeepAlive` running `wallclock run`, or
`StartInterval 60` running `wallclock once`.

*Linux* - a systemd user service (`ExecStart=wallclock run`), or
`* * * * * wallclock once` in crontab (set `DISPLAY`/`DBUS_SESSION_BUS_ADDRESS`
for gsettings).

## Configuration

`config.json`

```json
{
  "theme": "themes/minimal.json",
  "schedule": "schedule.txt",
  "resolution": "auto",
  "output_dir": "~/.wallclock/out",
  "location": {"lat": 41.15, "lon": -8.61},
  "min_update_seconds": 60,
  "max_sleep_seconds": 3600,
  "supersample": 3,
  "platform": "auto"
}
```

| key | meaning |
|---|---|
| `theme` | theme file, relative to the config |
| `schedule` | `.txt` (format below), `.json`, or an inline `{"06:00": "morning.jpg", ...}` mapping; optional |
| `resolution` | `"auto"` (ask the OS) or `"2560x1440"`.  The theme scales to any size. |
| `location` / `sun` | lat/lon for sunrise/sunset, or fixed `{"sunrise": "07:00", "sunset": "19:00"}`; only for sun/moon layers |
| `min_update_seconds` | battery guard; a theme can never force updates faster than this |
| `max_sleep_seconds` | occasional wake to recover after suspend/clock changes |
| `platform` | `auto`, `windows`, `macos`, `linux`, `android`, `ios`, `file` |

`schedule.txt`

```
# time  -> background image           | caption        | extras
06:00 -> backgrounds/morning.png      | Good morning
18:00 -> backgrounds/leave-office.png | Leave office
23:00 -> backgrounds/night.png        | Good night     | ink=#e6e6ef tint=#00000040
```

The active slot is the last one whose time has passed (wrapping past
midnight).  A slot provides the background image, an optional caption (drawn
by the theme's `text` layer via `{caption}`), an optional `tint`, an optional
`overlay=badge.png` image, and palette overrides: `ink=#fff` recolours every
`"@ink"` colour in the theme while the slot is active, so dark hands on a dark
night background are a one-line fix.

## How wallpaper updates work

1. `App.update(now)` asks the schedule for the active slot and builds a
   `RenderContext` (time, size, dial geometry, palette, slot, sun provider,
   asset cache).
2. `Renderer` draws every layer of the theme onto a `Canvas` and produces one
   RGB image at the screen resolution.
3. The image is written to `output_dir`, alternating `wallclock-a.png` /
   `wallclock-b.png` (Windows, GNOME and macOS skip the refresh if the path is
   unchanged).
4. The platform adapter applies it (`SystemParametersInfoW`, `osascript`,
   `gsettings`, ...).
5. Every layer reports its `next_change(ctx)`; the schedule reports its next
   boundary.  The minimum, clamped by `min_update_seconds` and
   `max_sleep_seconds`, is when the process wakes again.

A theme with hands wakes once a minute; a theme with an hour hand only wakes
every 12 minutes; a schedule-only theme wakes five times a day.

## Themes

A theme is JSON: a palette, the dial geometry and an ordered list of layers.
All sizes are fractions of the dial radius (or of the screen for positions),
so one theme fits every resolution and orientation.

```json
{
  "name": "minimal",
  "palette": {"ink": "#2a2a2a", "paper": "#efe9df", "accent": "#d9542b"},
  "dial": {"center": [0.5, 0.5], "radius": 0.34, "reference": "min"},
  "layers": [
    {"type": "background", "source": "schedule", "color": "@paper"},
    {"type": "markers", "count": 12, "style": "dot", "size": 0.022, "color": "@ink",
     "emphasize": {"every": 3, "size": 0.036}},
    {"type": "hands",
     "hour":   {"length": 0.52, "tail": 0.10, "width": 0.055, "color": "@ink", "shape": "rounded"},
     "minute": {"length": 0.82, "tail": 0.10, "width": 0.035, "color": "@ink", "shape": "rounded"},
     "hub":    {"size": 0.07, "color": "@ink", "inner_size": 0.024, "inner_color": "@paper"}},
    {"type": "text", "text": "{caption}", "position": [0.5, 0.92], "size": 0.13, "color": "@ink"}
  ]
}
```

Built-in layer types (each module's docstring documents every option):

| type | what it draws | wakes |
|---|---|---|
| `background` | slot image / fixed image / colour, plus slot tint | at slot boundaries |
| `markers` | dots, ticks, minor ticks, numerals | never |
| `hands` | hour/minute/second hands: `rounded`, `line`, `tapered`, `triangle`, or `image` sprites (`pivot` = pivot point of a sprite drawn pointing at 12) | every minute (second hand: every second, capped by the guard) |
| `text` | `{caption}` and/or strftime text | as needed by the format |
| `overlay` | slot `overlay=` image or fixed image | at slot boundaries |
| `orbiter` | anything moving around the dial: `driver` = `minute` (car laps hourly), `hour12`, `hour24`, `sun` (sunrise -> sunset along an arc), `moon` (night) | per `update_seconds`, plus sunrise/sunset |

Included examples: `themes/minimal.json` (the reference look), `themes/sky.json`
(sun by day, crescent moon by night, digital time), `themes/car.json` (a car
sprite as the minute hand, tapered hour hand).  Preview any theme at any time
and size without touching the wallpaper:

```bash
wallclock -c examples/config.json -t themes/car.json render --time 18:05 --size 1080x1920 -o preview.png
```

### Creating a new theme

1. Copy `examples/themes/minimal.json`, change palette/dial/layers.  Sprites and
   fonts referenced by relative paths are looked up next to the theme, then next
   to the config.
2. Swap pointer designs with `"shape": "image", "image": "hands/hour.png",
   "pivot": [0.5, 0.85], "size": 0.6"`.
3. For a new *kind* of visual, add a layer class - no core changes needed:

```python
# mypkg/comet.py
from wallclock.themes import Layer, register_layer
from wallclock.core.timeinfo import next_boundary

@register_layer("comet")
class CometLayer(Layer):
    def draw(self, canvas, ctx):
        x, y = ctx.point(angle_deg=ctx.now.minute * 6, r_frac=1.1)
        canvas.circle(x, y, ctx.px(0.05), fill=ctx.color("@accent"))

    def next_change(self, ctx):
        return next_boundary(ctx.now, 60)      # or None if static
```

and in the theme: `"plugins": ["mypkg.comet"], "layers": [..., {"type": "comet"}]`.
`ctx` gives you the time, dial geometry (`ctx.point`, `ctx.px`), palette
(`ctx.color`), active slot, sun provider (`ctx.sun.daylight(now)`) and the asset
cache (`ctx.assets.sprite(path, width)`).  `canvas` offers `polygon`, `circle`,
`ellipse`, `line`, `text`, `paste`, `tint`.

## Platform notes

| platform | adapter | status |
|---|---|---|
| Windows 10/11 | `SystemParametersInfoW` via ctypes; sets "Fill" style; resolution via `GetSystemMetrics` (DPI-aware) | working, tested |
| macOS | `osascript` "tell every desktop to set picture"; resolution via `system_profiler` (once) | implemented, untested here |
| Linux | GNOME/Cinnamon/MATE (`gsettings`), KDE (`plasma-apply-wallpaperimage` / qdbus), XFCE (`xfconf-query`), sway (`swaymsg`), fallback `feh`; `WALLCLOCK_LINUX_BACKEND` to force | implemented, untested here |
| Android | `termux-wallpaper` if Termux:API exists; otherwise needs a native app | limited, see `docs/android.md` |
| iOS | no wallpaper API; `wallclock export --schedule-only` + Shortcuts time-of-day automations | schedule only, see `docs/ios.md` |

Limitations worth knowing:

* **Multiple monitors**: one image is rendered at the primary display size and
  applied to all displays (Windows "Fill", GNOME "zoom").  Per-monitor images
  would need `IDesktopWallpaper` (Windows) or per-desktop AppleScript - a
  contained change inside the adapters.
* **Lock screen** is not touched on any platform.
* **DST transitions**: schedule times are wall-clock; the hour that repeats or
  is skipped is handled by the hourly `max_sleep` re-sync.
* **Second hands** are supported by the renderer but clamped to
  `min_update_seconds` (60 s by default) because a wake per second defeats the
  point of the project.

## Tests

```bash
python -m unittest discover -s tests -v
```

Covers time -> angle maths, schedule selection/parsing (incl. midnight wrap),
sunrise/sunset, theme loading, plugin registration, pixel-level render checks
at several resolutions, palette overrides, orbiter positions and the
scheduler's wake computation.
