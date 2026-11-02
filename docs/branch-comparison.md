# Branch review and selection

Reviewed original commits `3592d1b` (Claude/Python) and `367a7cb` (Astra/C++),
verified against origin on 9 September 2026. Both original branches remain intact;
work continues on `codex/best-of-both`. All project source, configs, tests, example
generation and platform documentation were inspected. Python bytecode is generated
output, not an independent implementation; unchanged vendored stb codecs were
checked by provenance/hash rather than re-audited as an image-codec project.

| Area | Claude/Python | Astra/C++ | Combined decision |
| --- | --- | --- | --- |
| Architecture | Clear Layer.draw + next_change; Canvas abstraction; registry | Small native library, custom raster primitives, C ABI | Keep native core; adapt paired layer callbacks |
| Scheduling | Injected clock/sleep; exception capture; hourly max-sleep | Native Windows event wait; DST-aware calendar; global cadence | Injectable, tested runner + per-layer deadlines + native waits |
| Failure handling | Logs exceptions but retries after an hour | First exception exits process | 60/120/240/300-second bounded failure retries; explicit recovery/fatal logs |
| Windows | Fill, profile update + broadcast, PNG A/B files | Existing fit, broadcast only, BMP A/B files; visibility events | Fill/keep option, update+broadcast, readback, unique bounded files, duplicate-instance guard |
| Drawing | More shapes/fonts/palette/tint; dial-region supersampling | Cheap analytic AA lines/dots; tiny font | Retain cheap primitives; add sprite rotation/pivot, tapered pointer, transparent crescent, formatted text |
| Cache | Raw/scaled/sprite plus disk cache, unbounded | One scaled background, uncached sprites | One background + 16 MiB sprite-cache budget; no growing disk cache |
| Config | Friendly JSON + schedule text, palette references; weak numeric validation | Strict small INI, strong bounds | Keep validated INI; add selected layer fields; no second runtime parser |
| Extensions | Imports arbitrary Python modules from theme data | Compiled draw registration; global time bucket could suppress custom changes | Compiled draw/deadline registration; remove runner's global visual-key shortcut |
| macOS | AppleScript process per change, once-only resolution | AppKit, native notifications | Keep AppKit; initialize application before notifications |
| Linux | Many CLI backends, some unbounded or ignored subprocess failures | Export-only stub | Small GNOME/KDE adapter with bounded subprocess wait and timerfd |
| Mobile | Termux path, Kotlin sketch, export; renderer/core need ports | Shared C ABI + export; no app | Keep real reusable C ABI; export without frozen hands; native mobile hosts still pending |
| Tests | 39 tests, useful fake-clock and pixel tests, no actual retry-delay assertion | 41 checks, DST and C ABI, no multi-minute visible test | Preserve native checks; adapt Python behavior tests, add failure/visibility/deadline regressions |
| Documentation | Broader and friendlier but overclaims zero battery, hour-hand cadence, static wake count | Explicit limits, conservative resource statements | Keep honest limits and add reproducible measurements/visual evidence |

## Confirmed bugs and evidence

1. **Python hour-long recovery delay.** `run_loop` catches an exception and sets
   `proposed=None`. `clamp_next_wake` maps None to `now + max_sleep` (3600 seconds).
   Its log incorrectly promises the next boundary. The original test only counts
   update calls, with a no-op sleep, so it cannot catch the stall. The preserved
   probe `scripts/reproduce_python.py` yields 10:00, 10:01(error), **11:01**.
   This is a confirmed failure mode, not proof of which transient error occurred
   in the user's historical run; that run's logs are unavailable.
2. **Python sun-only missing future appearance.** After sunset the layer returns
   None and relies on other layers/hourly cap to wake tomorrow. It is not a
   self-contained next-change contract. Timed visibility in the combined layer
   always returns future calendar/appearance boundaries, even while hidden.
3. **Python DST/timezone and cache weaknesses.** `local_now().astimezone()` carries
   a fixed current offset; future schedule arithmetic does not resolve upcoming
   folds/gaps. Sleep is not interrupted by native time/visibility events. Sun cache
   keys omit timezone; image and sun caches have no eviction. Config accepts zero
   or negative max sleep and arbitrary resolution/supersampling. Export step <=0
   never advances the export loop. These are inspected defects; not all explain
   the reported clock freeze. They were not copied into the native runtime.
4. **Astra runtime failure exits.** One file/apply error exits and leaves a stale
   wallpaper. The combined run mode retries; bad startup config still fails clearly.
5. **Astra extension scheduling mismatch.** A custom renderer could need a change
   inside the global theme bucket, but `visual_key` would skip it. The runner now
   trusts explicit layer deadlines; invalid deadlines are rejected with backoff.
6. **Astra visible verification was incomplete.** The original test only read
   `SPI_GETDESKWALLPAPER`. That confirms Windows selection, not visible pixels.
   A new test captures the exposed desktop, compares its central dial with the
   generated image, and checks changed pixels across real minute boundaries.
7. **Native event loss between render and wait.** Draining the Windows message
   queue in `visible()` could consume a time-change event before the next wait
   reset its state, leaving the old deadline armed. Pending environment and stop
   events now survive that drain; dedicated native event regression tests cover it.

See [validation](validation.md) for desktop captures, recovery evidence,
uninterrupted minute changes, resource measurements and explicit test limits.

## Why the original Astra result is not assigned a fabricated cause

The original executable visibly rendered the clock during the local comparison.
Profile flags, PNG vs BMP, and caching therefore were not demonstrated to be the
cause of the historical failure. An older Astra `--run build/clock --auto-size`
process was found still running and locking the executable; it was stopped before
testing the replacement. Such competing processes can overwrite each other, but
there is no evidence establishing that as the cause of the earlier user test.
The old report's immediate restoration also meant its successful smoke test did
not leave a clock installed. `--render` is export-only; `--once` is one update;
`--run` must stay alive. The new CLI logs these distinctions explicitly.

Fresh names avoid same-path reuse across restarts; a manifest retains only two
generated wallpapers. Windows readback detects a differing selected path but is
still not advertised as proof of visible pixels. The visual test is separate.

## Deliberately not copied

No full Python runtime, JSON executable plugins, supersampling canvas, unbounded
asset/disk caches, second-hand mode, 1,440-frame default export, or broad guesses at
Linux desktop backends. The sun/moon example uses explicit local dawn/dusk, not a
new astronomical-accuracy claim. Location-derived solar events can later implement
the same draw/deadline interface after polar/date/time-zone behavior is specified.
Fonts/palettes and a UI remain optional future work, not reliability prerequisites.
