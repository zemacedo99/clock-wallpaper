# Local validation — 9 September 2026

- Windows build: MinGW GCC 6.3.0, C++14, optimized. `scripts/build.ps1` completed
  successfully, including **41 checks and clean process exit**.
- Checked own C++ sources with `-Wall -Wextra -Wpedantic -fsyntax-only`; no diagnostics.
- Exported and visually inspected 1920×1080 analog and 18:00 reminder PNGs.
  Also generated portrait phone-static and orbit samples.
- Native Windows smoke test: applied current-time wallpaper with `--once --auto-size`,
  read back the selected wallpaper path through `SPI_GETDESKWALLPAPER`, and restored
  the previous wallpaper in a `finally` block.
- Runner sample: 1920×1080, solid background, 8 seconds after startup. Process CPU
  delta **0 ms**, working set **17.99 MiB**, application count stayed **1**. Runner
  stopped and previous wallpaper restored. This short idle sample does not measure
  battery life, full-minute update cost, or system compositor/import overhead.
- Removed unnecessary thread-local error storage after the old MinGW runtime crashed
  during teardown. The C bridge explicitly uses serialized calls and cleanly exits.
- macOS source, Linux export build, NDK and iOS cross-compilation were not verified
  in this Windows environment. Mobile apps and a Linux wallpaper setter are not shipped.

No clock runner or startup registration was left active. Build outputs and previews
are under the ignored `build/` directory.
