# Wallclock Windows 0.1.0-rc1

An unsigned native C++ static-wallpaper clock for Windows. This x86 build runs on
Windows x64 as well. No Python, compiler, source checkout, or third-party runtime
installation is required. The command script uses Windows PowerShell 5.1 or newer
only while executing commands; the running clock and supervisor are native.

Extract the ZIP, open PowerShell in the extracted folder, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\wallclock.ps1 install
$clock = "$env:LOCALAPPDATA\Wallclock\bin\wallclock.ps1"
& $clock run
& $clock status
& $clock stop
& $clock restart
& $clock preview -At 18:00
& $clock startup disable
& $clock startup enable
& $clock theme car
& $clock theme sky
& $clock theme minimal
& $clock cleanup
& $clock uninstall
```

`ExecutionPolicy Bypass` above applies only to that PowerShell process; it does not
change your policy globally. If your policy blocks later script calls, use the same
`powershell -NoProfile -ExecutionPolicy Bypass -File "$clock" COMMAND` form. Corporate
policy may still prohibit unsigned scripts. There is no GUI or silent privilege elevation.

`install` enables login startup unless `-NoStartup` is supplied. It copies the
package into your local application data; the extracted ZIP/source tree can then
be moved or deleted. It preserves existing config/themes/backgrounds. Updates
replace package-owned binaries/default templates, not user copies of templates.
`run` is idempotent for this installation. `status` reports supervisor/child PIDs,
startup registration, paths and the latest applied/error/reload log entry.
`stop` restores the saved previous wallpaper only if this clock still owns the
selected image; use `-KeepWallpaper` to keep the clock. A wallpaper manually chosen
while the clock is stopped is not overwritten by cleanup. `uninstall` stops,
restores where appropriate, removes login registration and unmodified owned binary
files. User configuration, themes, backgrounds, state, logs and previews remain.
No recursive deletion is used; reparse-point/junction paths are rejected.

## Locations

All folders are under `%LOCALAPPDATA%\Wallclock`:

| Folder | Purpose |
| --- | --- |
| `bin` | Installed binaries, command script, immutable release defaults/docs |
| `config/active.ini` | Active user configuration; live reload watches this folder |
| `themes` | Editable complete theme configurations and pointer sprites |
| `backgrounds` | User photographs/images; never cleaned automatically |
| `generated` | Owned static wallpapers and retention manifest |
| `logs` | Engine and supervisor logs, each about 256 KiB + one rotated file |
| `state` | Previous wallpaper backup/restoration metadata |
| `previews` | Preview image; never applied by the preview command |

## Configure the design

Edit `config/active.ini` as UTF-8 without a BOM. Geometry is relative to the shorter
screen edge. The default is a plain background, 12 points, and two simple hands.
The supplied daily schedule includes an 18:00 **Leave office** state. Preview it
with `preview -At 18:00`; preview never sets the desktop wallpaper.

For a background photo in your `backgrounds` directory:

```ini
[rule 18:00]
background = ../backgrounds/leave-office.jpg
text = Leave office
```

Change rule times, `color`, `background`, or `text` to schedule scenes. Each rule
overrides the selected theme, not the preceding rule. Change `hour`/`minute` hand
length, width, color and shape in element sections. Use `shape = image`, an `image`
path and `pivot_x`/`pivot_y` for a custom pointer. `theme car` selects a rotating
sprite example; `theme sky` selects configured day/night arcs. Sun/moon appearance
uses explicit local dawn/dusk times, not location-based astronomical calculations.
Copy a theme in `themes`, give it a new `.ini` name, and select it using `theme NAME`.
Full element options are in `docs/themes.md` (installed under `bin/docs`).

## Reload and recovery

The engine waits on Windows directory-change notifications, not a polling timer.
Saving/replacing `active.ini` reloads valid configuration and clears asset caches.
Invalid edits leave the previous working configuration active and produce a
`config_reload_rejected` record. Fix and save again. Files in other directories
are not watched: after replacing a photo/sprite, save/touch the active configuration
or use `restart`. Changing `fit` requires restart. Edits while hidden are applied
when rendering resumes. Keep unrelated high-churn files out of the config folder.

Normal hands update once per minute, with native lock/display/time/resume events.
Output/apply errors retry at 60/120/240/300 seconds. Unexpected child exits restart
at 60/120/240/480/900 seconds; no rapid restart loop or periodic supervisor polling.
No process is kept awake through laptop sleep. Startup may be delayed/disabled by
Windows; `status` shows registration, not a promise that a real login was tested.

Generated files retain two recent images. Cleanup removes only strictly named
owned files in `generated`, preserves the selected image, and requires the engine
to be stopped. It never cleans themes/backgrounds. Inspect `logs` for retry,
reload, lifecycle, startup and exit records; logs contain no desktop screenshots.

## Release-candidate limitations

One image is shared across monitors; mixed-monitor behavior is not yet validated.
This build's legacy narrow file I/O has not been validated with non-ASCII Windows
user/asset paths. Do not infer physical suspend/login or overnight certification
from simulated events. There is no hang watchdog or battery-life guarantee.
The included command script is unsigned. Review `release.json` / ZIP SHA256 when
transporting the package; checksums detect corruption, not publisher identity.
