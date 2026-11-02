# Wallclock

A lightweight C++14 static-wallpaper clock: render local time onto a configurable
background, apply it, then sleep. No GUI framework, GPU loop, network service or
script runtime. Two vendored stb headers provide PNG/JPEG/BMP codecs.

This combines the native engine with Claude's layer/deadline design. Windows is
verified against actual desktop pixels across real minute changes. macOS and
GNOME/KDE adapters are supplied but unverified on hardware. Phones have export and
a shared core, not installable apps. See [platform limitations](docs/platforms.md)
and the [branch comparison and diagnoses](docs/branch-comparison.md).

## Build

Use a C++14 compiler and CMake 3.16+; dependencies are included, so no network fetch:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows with MinGW can instead run `powershell -ExecutionPolicy Bypass -File scripts/build.ps1`.
Visual Studio places executables in `build/Release`; MinGW uses `build`.
No administrator rights or installation service required.

## Use

From the repository directory (omit `.exe` on other platforms):

```powershell
# Preview without changing your wallpaper:
./build/wallclock.exe --config examples/minimal.ini --render build/preview.png --at 10:10
./build/wallclock.exe --config examples/minimal.ini --render build/leave-office.png --at 18:00

# Apply actual local time once, or keep updating until Ctrl+C:
./build/wallclock.exe --config examples/minimal.ini --once build/clock --auto-size
./build/wallclock.exe --config examples/minimal.ini --run build/clock --auto-size

# Phone automation image without frozen clock hands:
./build/wallclock.exe --config examples/phone-static.ini --render build/phone.png --at 18:00
```

Output directories must exist. Use absolute paths when launching elsewhere. Run
one native instance per Windows session (duplicates are rejected). Stopping leaves the last image in place;
restore your preferred wallpaper through OS settings. Running the engine alone
does not register startup.

For daily Windows use, the optional native `wallclock_startup.exe` registers a
per-user login entry and supervises the engine without polling. It logs exits,
restarts conservatively, and supports graceful stopping. See
[Windows startup and recovery](docs/windows-startup.md) for installation/control
commands and the [reliability report](docs/windows-reliability.md). Registration
is explicit; building or running the engine alone does not install it.

## Configure and extend

Edit [minimal.ini](examples/minimal.ini). The example works immediately with solid
colors and an 18:00 **Leave office** overlay. To use your own photo, add:

```ini
[rule 18:00]
background = leave-office.jpg
text = Leave office
```

Paths are relative to the config. Rules repeat daily, switch inclusively at `HH:MM`,
and wrap to yesterday's last rule before the first entry. Each rule overrides its
selected theme, not the preceding rule. Restart to reload config/assets.
Use `--size 2560x1440` for explicit dimensions; backgrounds center-crop to cover.
Drawing sizes scale with the shorter screen edge, so clocks remain circular.

A theme contains ordered `markers`, `hand`, `orbit`, or `text` elements.
[car.ini](examples/car.ini) supplies a rotating car sprite and tapered hand;
[sky.ini](examples/sky.ini) demonstrates day/night arcs and a transparent crescent.
For new behavior, register drawing and next-change callbacks. They receive time,
configuration and geometry without device APIs. See [theme reference](docs/themes.md).
The [C ABI](include/wallclock/bridge.h) allows future JNI/Swift hosts to use the same core.

## Battery and scheduling

Default updates are minute-aligned. The next wake is the earliest layer deadline,
schedule change, midnight or DST transition. Each wake reads actual wall time;
Windows time/resume events also recompute the deadline. No system wake is requested.
`update_seconds = 0` supports schedule-only themes; otherwise use multiples of 60.
Coarser intervals intentionally leave the hands frozen between updates.

The runner caches one resized background and up to 16 MiB of sprites. Static layers
request no minute ticks. It compares pixel hashes and writes fresh filenames after
complete writes, retaining two generated images through a manifest across restarts.
Windows uses BMP and pauses after display-off/lock notifications. Failed updates
retry after 60/120/240/300 seconds, with failure and recovery records. Native time
and resume events interrupt sleep; events received during rendering are preserved.
Config reload requires a restart.

`--run` writes a bounded `PREFIX.log` (256 KiB + one rotated file) with timestamps,
hashes, next deadline and separate drawing/writing/application costs. `--log FILE`
overrides it. Startup mistakes fail with a nonzero exit code. Closing/killing the
process still stops updates; use `--run`, not `--render`, for a continuing clock.

With a cached photo background, two RGBA canvases use about 16 MiB at 1080p or
63 MiB at 4K, plus codec and
process overhead. Photo decoding temporarily adds memory. BMP writes about 8 MiB
per 1080p change; OS wallpaper import can dominate the cost. This is not zero-cost,
and mobile background restrictions prevent promising an accurate minute clock.

## Structure and verification

`core.cpp`: time/schedule/deadlines; `config.cpp`: validated INI; `renderer.cpp`:
composition; `platform/`: native APIs/waits; `main.cpp`: orchestration;
`bridge.cpp`: C ABI. Core and renderer contain no OS-specific wallpaper logic.

Tests cover angles, schedule boundaries/rollover, deadlines, deterministic rendering,
changing hands, reminder text, portrait output, custom renderers, image round trips,
and invalid input, plus Windows lifecycle events and supervisor restart policy.
Unit tests do not change the desktop. The explicit integration test below does,
and restores it. Python/Pillow is test tooling only, not an engine dependency.

```powershell
./scripts/verify-windows.ps1 -Samples 5 -InjectFailure
python scripts/analyze-verification.py build/verification-YYYYMMDD-HHMMSS
```

The test briefly exposes the desktop for each capture. Screenshots and restoration
metadata stay under ignored `build/`. See [measured results](docs/validation.md).
