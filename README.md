# Wallclock

A lightweight C++14 static-wallpaper clock: render local time onto a configurable
background, apply it, then sleep. No GUI framework, GPU loop, network service or
script runtime. Two vendored stb headers provide PNG/JPEG/BMP codecs.

Windows is implemented and locally tested. An unverified macOS adapter is included.
Linux and phones currently have image export and a shared core for native hosts,
not complete apps. See [platform limitations](docs/platforms.md).

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
one instance per wallpaper destination. Stopping leaves the last image in place;
restore your preferred wallpaper through OS settings. No startup task is installed.

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
[orbit.ini](examples/orbit.ini) demonstrates a 24-hour pointer that can use a car PNG.
For new behavior, register an `ElementRenderer`; it receives time, configuration,
resolution and a canvas, without device APIs. See [theme reference](docs/themes.md).
The [C ABI](include/wallclock/bridge.h) allows future JNI/Swift hosts to use the same core.

## Battery and scheduling

Default updates are minute-aligned. The next wake is the earliest theme interval,
schedule change, midnight or DST transition. Each wake reads actual wall time;
Windows time/resume events also recompute the deadline. No system wake is requested.
`update_seconds = 0` supports schedule-only themes; otherwise use multiples of 60.
Coarser intervals intentionally leave the hands frozen between updates.

The runner caches one resized background, skips unchanged visual keys, compares
pixel hashes, and alternates two output files after complete writes. Windows uses
BMP for low encoding CPU and pauses after display-off/lock notifications. Application
errors exit rather than retrying in a tight loop. Config reload requires a restart.

At 1080p the two RGBA canvases use about 16 MiB; at 4K, about 63 MiB, plus codec and
process overhead. Photo decoding temporarily adds memory. BMP writes about 8 MiB
per 1080p change; OS wallpaper import can dominate the cost. This is not zero-cost,
and mobile background restrictions prevent promising an accurate minute clock.

## Structure and verification

`core.cpp`: time/schedule/deadlines; `config.cpp`: validated INI; `renderer.cpp`:
composition; `platform/`: native APIs/waits; `main.cpp`: orchestration;
`bridge.cpp`: C ABI. Core and renderer contain no OS-specific wallpaper logic.

Tests cover angles, schedule boundaries/rollover, deadlines, deterministic rendering,
changing hands, reminder text, portrait output, custom renderers, image round trips,
and invalid input. Tests do not require changing the user's desktop.
