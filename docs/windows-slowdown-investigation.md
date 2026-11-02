# Slowdown and reboot investigation — 9 September 2026

Release qualification and the one-hour soak are paused. No reboot, logoff,
suspend, lock or system-clock change was triggered during this investigation.

## Evidence before reboot

`build/soak-20260909-190434/samples.jsonl` survived. Its 12 observations span
19:04:35–19:15:40 local time (665 seconds between first and last).
Every observation has one engine (36636) and one supervisor (18080).
The engine consumed 5.703 CPU seconds over that interval (0.857% of one core),
with 42.04–42.19 MiB working set, and handles decreased from 152 to 150.
Supervisor CPU stayed at 93.75 ms accumulated; handles stayed at 106.
All observed minute images had different hashes. Engine logs continued through
19:15 without a retry or restart loop. Two generated BMPs occupied 66,355,610
bytes including the ownership ledger; log growth was only 3,099 bytes.

This does not support an engine leak or runaway logging as the slowdown cause.
It does not rule out Windows wallpaper import cost or short unobserved spikes.
The old recorder did NOT capture its own CPU/IO, compiler CPU, global disk latency,
or historical memory pressure. Therefore a specific cause cannot be established.
Concurrent compiler work, archive creation and observer work were an avoidable
validation mistake, but attributing the slowdown to one of them would be speculation.

The second package test failed on its first install command with Access denied
(`build/package-test-20260909-191331/1.err`); repeated preview/install cycles did
not run. Old lifecycle evidence occupies about 679 MiB in two fixed directories;
the interrupted soak itself occupies about 0.51 MiB. These files were retained as
evidence, not deleted. Installed application data totals about 99.3 MiB.

## Real reboot / startup

Windows records the user's restart request at 19:15:46 (event 1074), event-log
shutdown at 19:16:13, and boot time 19:16:47. There was no matching resource
exhaustion event 2004 in the examined System log interval; absence is not proof
of adequate resources at every instant.

At 19:38 there were **zero Wallclock engines or supervisors** and no surviving
recorder, compiler or test PowerShell process (only the current inspection shell).
There were 372 processes and about 13.6 GiB free physical memory out of 31.7 GiB.
The HKCU Run entry, settings and installed files were present in both registry
views. No Wallclock StartupApproved disable value was found. Installed logs had
no post-reboot invocation. Thus login startup FAILED this real test; its cause is
unresolved. Manual execution of the exact registered command at 19:40 succeeded.
No automatic-login success is claimed. A new earliest-entry launcher log helps
distinguish invocation from settings/child failures at the next natural login.

A post-reboot five-second process CPU sample was led by ChatGPT and Edge
processes. This describes current activity, not historical causation.

## Changes

- Windows QPC backs performance measurements, monotonic log timestamps and
  supervisor uptime. Old MinGW's `steady_clock` had behaved like wall time.
- Production engine retry budgets now use monotonic milliseconds, converting
  remaining duration to a calendar deadline only when arming the platform wait.
  Forward/backward wall-clock corrections cannot shorten or extend the budget.
  Calendar/theme selection deliberately continues to use actual local time.
- Supervisor retry waits remain native relative waits, independent of UTC.
  A native wait may pause during low-power states; this is conservative backoff,
  not a wall-time-derived duration. Real suspend behavior remains unverified.
- Replaced the old recorder with a bounded observer: no WMI loop, desktop
  captures or full-wallpaper hashing. It samples process counters, selected
  wallpaper path, file/log sizes and its own resource use every 20 seconds,
  then exits. Process objects are disposed. It is diagnostic tooling, not a
  component of the installed engine.
- Shared OS-released file lease prevents overlap among recorder, build,
  packaging and package tests. The empty lock file is intentionally retained;
  it cannot create a stale lock after reboot.
- Recorder defaults to three minutes. Runs above ten minutes require explicit
  `-AllowLongRun`; that is a guard, not authorization to resume a long test.

## Verification

One sequential build passed 60 shared checks, 27 native-event checks and 19
supervision checks: **106 automated checks**. The two new shared checks exercise
forward/backward clock changes with ten seconds already spent in retry backoff.
Two additional tooling checks verified overlap rejection and long-run opt-in.
All changed PowerShell scripts parsed successfully. Full package tests were NOT
rerun during this incident investigation.

The corrected binaries were copied into the existing stopped installation and
their installed manifest hashes refreshed. User configuration/assets and startup
registration were preserved. A duplicate supervisor launch exited 2; one engine
and one supervisor remained. The earlier release ZIP is not refreshed or qualified
by this diagnostic update.

Controlled observations are in `build/controlled-20260909-194051` (installed old
build) and `build/controlled-20260909-194535` (corrected build). Builds and packaging
did not run during either observation. Selected wallpaper paths and engine pixel
hashes establish publication/selection changes; this investigation did not capture
the physical display and does not claim independent visual confirmation.

### Completed corrected-build observation

180.28 seconds, 19:45:35–19:48:35, three minute boundaries (19:46/47/48), no
missed boundary, no error/restart, one engine PID 29304 and supervisor PID 28504.
Selected BMP paths changed with the three distinct logged pixel hashes. Updates
took 335.08, 476.26 and 439.12 ms, completing within the intended second.

| Process | CPU delta / one-core average | Working set | Handles first → last |
|---|---|---|---|
| Engine | 0.844 s / 0.468% | 41.63–41.68 MiB | 145 → 143 |
| Supervisor | 0 s / 0% | 7.88–7.96 MiB | 101 → 101 |
| Observer | 2.063 s / 1.144% | 159.38–179.96 MiB | 597 → 688 |

Engine private bytes decreased 33.77→33.40 MiB; six of nine 20-second intervals
had zero measured engine CPU. All nine supervisor intervals had zero measured CPU.
The engine wrote 99,535,308 bytes (three ~31.64 MiB BMPs plus small metadata),
read zero additional process-IO bytes and retained two BMPs plus a ledger.
These process IO counters include cached IO, not measured physical disk writes.
Logs grew by 825 bytes. No engine/supervisor resource growth was observed.

Observer runtime resources did grow during the short measurement; this is not a
long-term leak test and is not labeled stable. It exited at completion and no
recorder/compiler/test child was left behind. Its footprint is much larger than
the engine's, despite removal of image hashing and captures. A small native or
less frequent diagnostic sampler is preferable before overnight qualification.
The validation lease was successfully reacquired after the observer exited.

A separate instantaneous system snapshot at 19:47:54 reported 14,469 MiB available
RAM, 223,763 disk bytes/sec, 19 transfers/sec and a zero disk queue. It cannot
reconstruct pre-reboot disk contention. Detailed measurements are saved beside
the samples in `summary.json`.

Investigate actual login launch on the next natural login before calling Windows
v0.1 ready. The one-hour test remains paused; the short results justify further
controlled investigation, not a guarantee against another system slowdown.
No overnight, battery-life or real suspend/resume result is established here.
