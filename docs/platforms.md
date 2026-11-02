# Platform support

Research checked 9 September 2026. OS permissions and desktop policy can reject changes.

| Platform | MVP status | Restriction / next integration |
| --- | --- | --- |
| Windows | Native adapter implemented | One image across monitors; primary resolution with `--auto-size` |
| macOS | AppKit adapter supplied, unverified | Same image on attached screens; Spaces and mixed displays need testing |
| Linux | GNOME/KDE adapter supplied, unverified | Other desktops remain export-only |
| Android | Export and shared C ABI | No installable app/JNI host; minute background refresh not guaranteed |
| iOS | Export and shared C ABI | User-configured Shortcuts; no automatic minute-clock promise |

## Windows

[SystemParametersInfoW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfow)
sets the wallpaper using `SPI_SETDESKWALLPAPER`, `SPIF_UPDATEINIFILE` and `SPIF_SENDCHANGE`.
Readback checks Windows' selection; the integration test separately checks visible
pixels. A hidden top-level window receives time/resume/display
events and uses one timer. Display-off/lock events suppress rendering until on/unlock.
[Power notifications](https://learn.microsoft.com/en-us/windows/win32/power/power-setting-guids)
distinguish off, on and dim (dim is treated as visible). Unavailable registrations
fall back to normal scheduled updates. A process launched while already locked may
render once before it learns the state.

`fit=fill` (default) selects Fill once; `fit=keep` preserves the current layout.
A named session mutex rejects a second native engine. Fresh filenames and a retention
manifest avoid path reuse across restarts while retaining two files. Separate
per-monitor images would use an `IDesktopWallpaper` adapter; the core
already renders arbitrary sizes. Optional per-user login startup and conservative
supervision are described in [Windows daily operation](windows-startup.md).
Stopping leaves the last wallpaper in place; tests explicitly restore it.

## macOS

The adapter calls [NSWorkspace](https://developer.apple.com/documentation/appkit/nsworkspace)
and waits on a one-shot run-loop timer with clock/display/resume notifications.
Requires macOS 10.12+ and Xcode command-line tools. This source was not compiled or
tested on macOS here. Screen-off/lock suppression is pending; it updates while awake.
Test Spaces and wallpaper caching behavior before enabling startup. Setter cost is
not benchmarked and may exceed image-generation cost.

## Linux

`linux.cpp` supports GNOME's [GSettings background keys](https://help.gnome.org/system-admin-guide/desktop-background.html)
(including the modern dark key) and KDE's `plasma-apply-wallpaperimage`. Other desktops
fail explicitly rather than guessing. Override detection with
`WALLCLOCK_LINUX_BACKEND=gnome|kde`. Native timerfd waits handle absolute deadlines,
suspend and wall-clock changes; subprocess arguments never pass through a shell and
completion is bounded to 15 seconds. No installed UI library is linked.
Provide `--size`; auto-size and visibility suppression are not implemented on Linux.
The GNOME adapter requires a version with `picture-uri-dark`; partial setter errors
are logged/retried. There is no universal X11/Wayland setter. This source was not
compiled here: available WSL distributions have no C++ compiler installed.

## Android

[WallpaperManager](https://developer.android.com/reference/android/app/WallpaperManager)
can set a bitmap with `SET_WALLPAPER`; a Kotlin host must check wallpaper support
and permission. Render using the actual execution time, apply once, release bitmap.

[WorkManager periodic jobs](https://developer.android.com/develop/background-work/background-tasks/persistent/getting-started/define-work)
have a minimum 15-minute interval and may be deferred by Doze. That cannot deliver an
accurate minute clock. [Exact alarms](https://developer.android.com/develop/background-work/services/alarms)
carry permission/use-case restrictions and idle limits; they are not used as a bypass.
A foreground service adds a persistent notification and resource obligations.

Closest lightweight option: a few scheduled scene changes, accepting delays, or
render on explicit user action. A future native `WallpaperService` could draw on
visible minute boundaries and stop when hidden, but that is a separate event-driven
live-wallpaper mode, not this static-image MVP. Neither ordinary jobs nor this export
workflow guarantee that a reminder is delivered at exactly 18:00.

Build the core with the NDK and `-DWALLCLOCK_BUILD_DESKTOP=OFF`. Add a thin JNI bridge
to `bridge.h`; Android owns permissions, jobs, visibility, screen size and applying
the image. No Android classes belong in the renderer. An APK/JNI host is not included.

## iOS

Apple supports [time-of-day automations](https://support.apple.com/guide/shortcuts/apd932ff833f/ios),
[automatic execution](https://support.apple.com/guide/shortcuts/apd602971e63/ios), and a
[daily wallpaper Shortcut workflow](https://apps.apple.com/cn/story/id1561118008?l=en-GB).
There is no public general-purpose third-party wallpaper setter. The user creates
and authorizes the automation; action names/targets vary with iOS version.
[BackgroundTasks](https://developer.apple.com/documentation/backgroundtasks/refreshing-and-maintaining-your-app-using-background-tasks)
does not supply a precise repeating minute scheduler.

Practical workflow now:

1. Export portrait PNGs at desired transition times with `examples/phone-static.ini`.
2. Transfer them to Photos.
3. Create a Time of Day automation in Shortcuts, choose the corresponding image,
   add the wallpaper action and select immediate execution where available.

**Exported hands stay at their sampled time.** The phone-static example omits hands
so automations do not display a misleading frozen clock. A future Swift App Intent
could call the shared core when invoked and return an image to Shortcuts; this still
cannot guarantee per-minute refresh. No private APIs or unsupported background modes.
An installable iOS app or Shortcut bundle is not included.

## Shared native host contract

`bridge.h` provides create/destroy, render-at-Unix-time, next-deadline and error access.
Link `wallclock_core`; serialize calls on one queue because C runtime local-time
conversion is shared. A host supplies current time/resolution and renders only when
visible and permitted. Recompute the deadline on time-zone/resume events. OS-permitted
delays must be described as degraded scheduling. PNG file output avoids UI dependencies
and native bitmap ownership across the boundary.
