# Windows reliability iteration — 9 September 2026

This iteration adds daily Windows startup and process-exit supervision to
`codex/best-of-both`; it does not replace the rendering/theme architecture.
[Installation and controls](windows-startup.md) describe the per-user Run entry,
logs, conservative restart policy, and removal procedure.

## Reliability bugs found and fixed

1. **Sent notifications could leave the engine asleep.** In the first live
   lifecycle run, synthetic unlock/resume/time-change messages reached the window
   procedure, but rendering did not resume. `GetMessage` dispatches sent messages
   internally and may continue blocking instead of returning to the caller. A
   posted wake message now accompanies relevant events. The cross-thread test
   fails against the pre-fix adapter and passes with the fix. It tests an event
   arriving while the wait is blocked, unlike the previous same-thread tests.
   Evidence from the interrupted reproduction is under
   `build/lifecycle-20260909-181010`. It is not counted as a successful soak.
2. **Clock rollback during a failure could extend backoff.** A pending retry used
   an absolute deadline from before the clock correction. The scheduler now
   preserves a bounded remaining retry when an environment/visibility event
   accompanies a backwards correction. Normal time events still force current-time
   rendering; events do not bypass the error retry budget.
3. **Hidden engine lacked a normal supervisor stop path.** `WM_CLOSE` and session
   end now stop the loop and produce lifecycle/stopped records. The supervisor
   requests close, waits up to ten seconds, and logs any forced termination.
4. **Fatal startup logs could grow indefinitely.** The fatal/duplicate exit paths
   now use the bounded logger, including during repeated failed supervisor starts.
   A regression fills a log past 256 KiB and verifies rotation on a startup error.
5. **Native tests could address another process's window.** The lifecycle unit
   test now finds its own thread's window, so running it cannot send synthetic
   lifecycle/stop events to an active user's clock.

The supervisor waits on the child process and a stop event. Launch failures and
unexpected nonzero exits retry after 60/120/240/480/900 seconds (900 maximum), with
failure streak reset after ten minutes of child uptime. Clean/duplicate exits are
not restarted. A kill-on-close job prevents orphaned children. No heartbeat or
periodic supervision polling is added. Existing engine update retries stay inside
the engine, and both logs remain bounded.

## Automated coverage

- 58 core/render/scheduler checks, including a simulated seven-day sequence of
  10,081 updates spanning the autumn DST fold without accumulated deadline drift.
- 21 native event checks: blocked cross-thread sent-message wake, lock/unlock,
  display off/on/dim, synthetic suspend/resume, time/locale changes, timer and stop.
- 17 supervision checks: Windows command-line quoting round trips, capped restart
  timing, uptime reset, and clean/duplicate/crash exit policy.
- Fatal-startup log rotation regression passed without changing the desktop.
- Project source warning checks and final whitespace validation are recorded below.

The synthetic week is not an actual seven-day process run. DST/time tests do not
change Windows' system clock or timezone. The user selected automated lifecycle
checks only: physical lock, screen power, suspend/resume, actual logoff/login, and
an overnight duration are not claimed.

## Live integration evidence

The corrected-build run crossed **15 real minute boundaries**, from 18:20 to 18:35
local time (887.39 seconds measured). It captured 13 distinct generated frames:
12 desktop captures passed pixel comparison; frame 3 was visibly covered by the
Windows task switcher and was explicitly excluded, not counted as a pass. The
original screenshot is preserved. Changed-hand-pixel mean absolute error in the
usable captures was 2.47–3.94/255. The analyzer keeps its original strict threshold.
Evidence: `build/lifecycle-20260909-182020`.

- Both duplicate launch attempts exited 2; no extra engine was created.
- Output-file lock: failure at 18:22, bounded retry and successful recovery at 18:23.
- Synthetic session lock held the wallpaper unchanged across the next boundary.
  Unlock/suspend/resume/time notifications at 18:25 refreshed the clock within
  seconds. These messages changed the engine's test state, not the laptop state.
- The first child was deliberately terminated at 18:27:07. The supervisor logged
  the unexpected exit, waited exactly 60 seconds, and started its replacement at
  18:28:07. The replacement then updated at each minute through 18:35, including
  seven consecutive normal minute boundaries after restart.
- Normal update log timestamps sampled actual time at `:00`; no accumulating
  minute drift or unexplained stop was observed. The intentional failure/hidden/
  restart intervals are not counted as missing-update bugs.
- Stop completed gracefully and the previous wallpaper/layout was restored.

Reproduce visual analysis with the documented occlusion annotation:

```powershell
python scripts/analyze-verification.py build/lifecycle-20260909-182020 --exclude-frame "3:Windows task switcher visibly occludes the dial; manually inspected, not a visual pass"
```

The real Windows adapter recovery test also passed: a missing image was rejected,
then two actual setter calls succeeded with the previously selected wallpaper.
The shared loop logged failure/recovery and requested a 60-second retry using an
injected clock. This tests adapter-error recovery, not an actual Windows policy
denial or a real 60-second delay in that particular test.

## Resources during the longer run

3840×2160 solid-background analog/reminder theme, with other desktop work present.
CPU refers only to the engine/supervisor processes, excluding Explorer/compositor
and other Windows work. Samples are not a battery-discharge test.

| Measurement | Engine | Supervisor |
| --- | ---: | ---: |
| Idle CPU, twelve 8-second samples | 0 ms each | 0 ms each |
| Sampled resident working set | 41.96–42.38 MiB | 8.26–8.36 MiB |
| Total sampled CPU during run | 16.109 s, across both children | 0.188 s |
| Mean percentage of one CPU core, including updates/restart | 1.82% | 0.021% |
| Handle count | 149–151 | 106 throughout |

Engine private memory stayed within 33.48–33.86 MiB. For the replacement child,
the first/last samples were 42.09/42.06 MiB resident, 33.85/33.50 MiB private,
and 151/149 handles. The supervisor retained 106 handles even after restart. There
was no sustained growth in this observation window; it does not establish that no
leak exists over days. Cached photos/sprites can raise memory above this solid-theme
sample. Only two production wallpaper files remained; copied test evidence is
separate and intentionally retained under ignored `build/`.

Median wall times: drawing **59.26 ms**, write/hash **1,148.28 ms**, Windows apply
**1,096.65 ms**, complete update **2,332.00 ms**. Complete updates ranged from
1,049 to 6,158 ms. Windows import and file writing dominate; visible freshness can
lag a boundary by several seconds even though the sampled time is aligned. Idle
CPU below process-counter resolution is not a claim of literally zero energy use.

## Startup integration

The separate integration test passed; evidence is under
`build/startup-20260909-183808`:

- Missing config produced exactly two logged fatal startup attempts, **60 seconds
  apart**. The second failure scheduled a 120-second wait.
- Stopping during that wait took **177.75 ms**, with no rapid restart loop.
- Deliberately killing the supervisor terminated its owned child: no orphan runner.
- Installation wrote the per-user Run value and stored absolute config/output
  paths. Manually executing its exact `--start` command loaded those settings and
  actually applied wallpaper. A status check succeeded and graceful stop was logged.
- Login startup was left **enabled**, pointing to the current checkout's
  `build/wallclock_startup.exe`, `examples/minimal.ini`, and `build/daily/clock`.
  All test processes stopped and the previous wallpaper/layout was restored.

The final `scripts/build.ps1` run passed all **96 checks** (58 + 21 + 17).
Source warning checks passed without diagnostics, and `git diff --check` passed.
Both original branches remain preserved; changes are local on `codex/best-of-both`.
No actual logoff/login or physical suspend was performed, per the user's choice
to use automated lifecycle checks only.

Run the startup integration harness with `-EnableLogin` only when registration
should remain enabled. Without it, the harness restores the previous Run/settings
values. It includes real wallpaper-setting calls and restores the previous desktop.

## Limits and next validation

A supervisor cannot log before Windows manages to launch its executable. Moving
or deleting the installed binaries/config can break startup. Windows can delay or
disable a Run entry. Registration and manually executing its exact command do not
prove an actual login session has been exercised.

This is exit supervision; a hung but living process is not automatically killed.
External termination of the supervisor terminates its child and requires another
start/login. Physical suspend/resume and overnight operation are the next highest-
value checks. Only after those should a heartbeat/watchdog be considered, with a
contract that distinguishes a hidden or schedule-only theme from a hung renderer.
The current Windows build still shares one image across monitors. General Unicode
file-path support and mixed-display behavior were not expanded in this iteration.
No macOS/Linux/mobile host work or GUI was added, and no battery-life result is claimed.
