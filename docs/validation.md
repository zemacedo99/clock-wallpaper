# Local validation — 9 September 2026

This is the earlier combined-engine baseline. The subsequent startup/lifecycle
iteration is covered in [Windows reliability](windows-reliability.md).

Tested the combined implementation on Windows at 3840×2160. These are local
measurements, not a battery-life claim or a guarantee against process termination.
Original commits and review scope: [branch comparison](branch-comparison.md).

## Build and regression checks

- MinGW GCC 6.3.0, optimized C++14 native build succeeded.
- **56 core/render/scheduler checks passed**, preserving the original 41 checks.
  Added exact retry delays/backoff, missed boundaries, hidden resume, backwards
  time changes, per-layer/invalid deadlines, timed visibility and sprite/cache tests.
- **8 Windows event checks passed**, including time-change and stop messages
  consumed before entering the wait. These tests do not change the desktop.
- The original Python branch's **39 tests passed** independently. A separate
  injected-clock reproduction demonstrates its untested hour-long error delay:
  update at 10:00, failure at 10:01, next update at 11:01.
- Native source warning checks and `git diff --check` passed.
- Analog, reminder, portrait, car and day/night example renders were inspected.

## Actual Windows desktop verification

The harness saves wallpaper/layout, starts a hidden runner, exposes and captures
actual desktop pixels at real minute boundaries, records image/process measurements,
then stops the runner and restores wallpaper/layout in `finally`. No startup task
or runner was left active. Captures and restoration metadata remain local under
ignored `build/`; they are not source-controlled.

1. **Recovery run, 17:28–17:33 local time:** five visibly distinct images over
   286.51 seconds. An exclusive lock on the pending output caused the 17:30 update
   to fail. The log recorded a 60-second retry and recovery at 17:31, followed by
   successful 17:32 and 17:33 updates. Evidence directory:
   `build/verification-20260909-172816`.
2. **Final-build uninterrupted run, 17:38–17:41:** all three consecutive minute
   boundaries changed both the generated image and actual desktop. Four captured
   frames over 149.31 seconds. Five apply records include one extra startup
   refresh from a native event; this was not a recurring idle loop. Evidence:
   `build/verification-20260909-173833`.

Both directories contain `clock.log`, `measurements.json`, `analysis.json` and
captured/generated images. The analyzer compares the central dial, avoiding
icons/taskbar, and specifically checks pixels where hands should have changed.
In the final run each boundary changed over 10,500 clock pixels.
Generated-versus-desktop mean absolute error was 0.39–0.40/255 over the dial and
2.39–2.68/255 over changed hand pixels. All captured frame hashes were distinct;
screenshots were also visually inspected. This verifies desktop pixels beyond
just the setter's selected-path readback.

The **original Astra executable also visibly applied** during an isolated probe.
BMP/PNG and profile-flag probes did not reproduce the user's historical failure.
An old Astra runner was identified and stopped before replacement testing. Its
presence suggests possible competition, not proof of the historical cause.

## Resource and update costs

Solid-background analog theme, 4K output. CPU values are engine-process CPU time;
they exclude Explorer/compositor/other OS processes. Apply wall-time measurements
include the synchronous API call, not every subsequent system import operation.
Desktop capture and other host activity can affect timing.

| Measurement | Recovery run | Final uninterrupted build |
| --- | ---: | ---: |
| Idle CPU per 8-second sample | 0 ms (5 samples) | 0 ms (3 samples) |
| Sampled working set | 42.02–42.25 MiB | 41.97–42.02 MiB |
| Total process CPU | 2.609 s / 286.51 s | 3.844 s / 149.31 s |
| Average of one CPU core, including updates | 0.91% | 2.57% |
| Median render wall time | 30.21 ms | 86.30 ms |
| Median write + hash wall time | 437.87 ms | 763.73 ms |
| Median Windows apply wall time | 555.81 ms | 1,338.57 ms |
| Median complete update wall time | 1,014.81 ms | 2,176.72 ms |
| Complete update range | 780–1,259 ms | 660–4,003 ms |

The second run includes two startup applies and illustrates host variability.
Minute updates began at the boundary but the visible result can arrive a few
seconds later. Idle CPU was below process-counter resolution during the samples;
this is not proof of literally zero energy consumption. Working set is resident
memory, not peak allocation: two 4K RGBA canvases alone need about 63 MiB of
addressable storage, plus codec/asset overhead. Each 4K BMP is about 32 MiB, so disk
writes and Windows import remain the principal optimization opportunity.

For context, the original Python example's five 4K file renders took 937–1,543 ms,
with a 63.3 MiB sampled working set and about 450.6 MiB peak working set. That
workload used its gradient, 3× supersampling and PNG output, and excluded Windows
application. It is **not an equivalent end-to-end performance comparison**.
Raw results: `build/python-benchmark.json`.

## Reproduce and limits

```powershell
./scripts/build.ps1
./scripts/verify-windows.ps1 -Samples 5 -InjectFailure
./scripts/verify-windows.ps1 -Samples 4
python scripts/analyze-verification.py build/verification-YYYYMMDD-HHMMSS
# Requires a checkout/extraction of the original Python branch:
python scripts/reproduce_python.py build/review-python
```

macOS, Linux, Android NDK and iOS builds were not verified here. The available WSL
distributions have no C++ compiler. Native event unit tests cover lock/time-change
logic; physical suspend/resume, extended overnight execution, mixed-monitor setups,
and battery discharge were not measured. Mobile native hosts are not included.
The next useful reliability step is an optional native login launcher/supervisor
with status and restart reporting, followed by an overnight/suspend soak test.
