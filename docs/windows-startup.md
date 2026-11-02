# Windows daily operation

The engine remains a native, event-driven C++ application. `wallclock_startup.exe`
is a separate small Windows-subsystem executable: it opens no console, links no
UI framework, and waits on native process/event handles rather than polling.

## Install and control

Build with `scripts/build.ps1` or CMake. Keep `wallclock_startup.exe` beside
`wallclock.exe`. Choose an existing writable output directory and stable config
location. From PowerShell, `Start-Process -Wait -PassThru` exposes the native
launcher's exit code; shelling a Windows-subsystem executable directly may return
before it finishes.

```powershell
New-Item -ItemType Directory -Force build/daily | Out-Null
$setup = Start-Process ./build/wallclock_startup.exe -ArgumentList '--install --config "C:\Users\jamacedo\Git\wallpaperClock\examples\minimal.ini" --output "C:\Users\jamacedo\Git\wallpaperClock\build\daily\clock"' -WindowStyle Hidden -Wait -PassThru
$setup.ExitCode
Start-Process ./build/wallclock_startup.exe -ArgumentList '--start' -WindowStyle Hidden
# Stop gracefully (leaves the last wallpaper in place):
Start-Process ./build/wallclock_startup.exe -ArgumentList '--stop' -WindowStyle Hidden -Wait
# Check supervisor presence: exit 0 = running, 3 = absent; also writes a status log.
Start-Process ./build/wallclock_startup.exe -ArgumentList '--status' -WindowStyle Hidden -Wait -PassThru
# Remove login registration; does not stop an already-running process:
Start-Process ./build/wallclock_startup.exe -ArgumentList '--uninstall' -WindowStyle Hidden -Wait
```

Substitute paths for your checkout. Do not delete/move the binaries or config after
installation without reinstalling. No files are copied into a system directory.
The user can also disable the startup entry in Windows Settings/Task Manager.
Run entries are [per-user login programs](https://learn.microsoft.com/en-us/windows/win32/setupapi/run-and-runonce-registry-keys),
not services; Windows can delay their execution. The command is limited to 260
characters, so the Run value contains only the quoted launcher path and `--start`.
Config/output absolute paths are stored separately under `HKCU\Software\Wallclock`.
The single startup value is `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Wallclock`.
Uninstall removes this value and retains settings/logs for diagnosis.

For a temporary run without registration, supply both paths to `--start --config
FILE --output PREFIX`. A second supervisor exits 2. The engine independently
rejects a second runner with exit 2. If a manually launched engine already owns
the desktop mutex, the supervisor logs that fact and exits; it does not adopt or
kill that existing process. Stop the manual runner before enabling supervision.

## Recovery and observability

- Healthy engine: supervisor waits indefinitely on the child process and stop
  event, with no periodic checks or heartbeat writes.
- Failed launch or unexpected nonzero exit: restart after 60, 120, 240, 480, then
  at most once per 900 seconds. Ten minutes of child uptime resets the failure
  streak. Waits are interruptible by `--stop`; there is no rapid restart loop.
- Exit 0 is intentional; exit 2 means duplicate. Neither is restarted.
- Render/write/apply exceptions stay inside the engine's existing 60/120/240/300
  second recovery policy. Native events cannot bypass this backoff. Backwards
  wall-clock corrections no longer stretch a short pending retry into hours.
- `--stop` sends a close message to the owned child's windows and waits up to ten
  seconds. If necessary it terminates that child job and records a forced stop.
- The child is created suspended, assigned to a
  [kill-on-close job](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects),
  and then resumed. If the supervisor dies, its child cannot remain orphaned.
  The supervisor itself is not automatically restarted until another launch/login.

Launcher logs: `%LOCALAPPDATA%\Wallclock\startup.log` and one rotated `.1` file.
Engine logs: `PREFIX.log` and `.1`. Both rotate at approximately 256 KiB, including
repeated fatal startup failures. Entries record launch/exit codes, child PIDs,
restart delays, stop actions, and engine render/apply/recovery information.
Windows lifecycle records identify session lock/unlock, display state, time/locale
changes, suspend and resume. Sent OS messages also post a queued wake: this ensures
`GetMessage` returns instead of dispatching the event internally and continuing
to sleep on a stale deadline. No desktop captures occur in the installed engine.
If an output directory is missing, its engine log cannot be written; the independent
launcher log still records the child failure and scheduled restart. If Windows
cannot launch the supervisor at all (for example its binary was deleted), it
cannot write a log; check the registered path and Windows startup settings.

This is process-exit supervision, not an arbitrary hang detector. It deliberately
does not treat a hidden clock's long sleep as a hang or wake a sleeping laptop.
A persisted invalid configuration continues retrying at the capped interval until
fixed. Installation checks file presence, not every referenced image/theme; use
`--render` to preflight a newly edited theme. Config/assets reload on restart.

## Lifecycle tests

`windows_events_tests` sends messages only to its own test window, covering
lock/unlock, display off/on/dim, suspend/resume, time/locale change, timer and stop.
These are **simulated events**, not proof of physical sleep/resume or a real
Windows timezone change. Core tests exercise DST folds/gaps and clock corrections
without changing the host's clock or timezone.

```powershell
./scripts/build.ps1
./scripts/verify-windows-lifecycle.ps1 -Minutes 15
python scripts/analyze-verification.py build/lifecycle-YYYYMMDD-HHMMSS
# Separate startup integration; omit -EnableLogin to restore previous registration:
./scripts/verify-windows-startup.ps1 -EnableLogin
```

The explicit integration harness changes wallpaper, captures the exposed desktop,
injects an output-file lock, holds a synthetic lock across a minute, sends synthetic
resume/time events, kills one child, and checks supervised restart and continued
minute changes. It restores the previous wallpaper/layout and stops both processes
in `finally`. Screenshots and measurements remain under ignored `build/`.
It requires no actual suspend, lock, clock adjustment, or logoff.
An occluded screenshot must not be treated as a visible-wallpaper pass. After
manual inspection, `--exclude-frame INDEX:REASON` records an explicit exclusion
without relaxing pixel thresholds or removing the original evidence.

An additional explicit adapter integration test can be built from
`tests/windows_apply_recovery.cpp` with `src/platform/windows.cpp` and
`src/scheduler.cpp` (`-Iinclude -luser32 -ladvapi32`). It verifies missing-file
rejection followed by two successful actual wallpaper API calls with the original
selected image. Retry time is simulated. It does not reproduce an OS policy denial.
It is intentionally excluded from default tests because it calls the real setter.

See [the current reliability report](windows-reliability.md) for measured results
and remaining limitations. Physical suspend/resume and overnight duration must be
verified separately before claiming them. No battery-life results are implied.
