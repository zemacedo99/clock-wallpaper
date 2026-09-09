# Android

## What the OS allows

* Setting the wallpaper requires an installed app holding `SET_WALLPAPER` and
  calling `WallpaperManager.setBitmap()` / `setStream()`.  A shell script cannot do it.
* Background execution is throttled hard.  `WorkManager` periodic jobs run at
  most every 15 minutes; `AlarmManager.setExact*` every minute needs the
  `SCHEDULE_EXACT_ALARM` permission (Android 12+), is denied by default on 14+
  for most apps, and is exactly the wake-up pattern Doze exists to stop.
* Writing a PNG to disk and calling `setBitmap()` every minute also makes the
  system re-decode and re-blur (for the launcher, lock screen, and dark-theme
  extraction) each time.  That is *more* work than drawing one frame.

## Practical approaches

### 1. Termux (no app to build)

Install Termux and Termux:API, then `pkg install python termux-api pillow`.
`platforms/android.py` calls `termux-wallpaper -f <file>` automatically.

    python -m wallclock -c ~/.wallclock/config.json once

To repeat it you need Termux to stay alive (`termux-wake-lock` + `wallclock run`),
which costs battery, or `termux-job-scheduler` with a 15-minute period.  Good for a
schedule-only theme (backgrounds that change at 06:00, 09:00, 18:00, ...) and a
clock that is allowed to be up to 15 minutes late.  Not good for a proper minute hand.

### 2. Minimal native app (recommended for a real minute hand)

The battery-correct design on Android is a `WallpaperService` - a "live wallpaper"
in name, but not an engine: it draws **one frame per minute, only while visible**,
using the same theme JSON and schedule files.

Skeleton (Kotlin):

```kotlin
class ClockWallpaper : WallpaperService() {
    override fun onCreateEngine() = object : Engine() {
        private val handler = Handler(Looper.getMainLooper())
        private var visible = false
        private val draw = object : Runnable {
            override fun run() {
                val holder = surfaceHolder
                val canvas = holder.lockCanvas() ?: return
                try { renderer.draw(canvas, System.currentTimeMillis()) }   // port of renderer/ + themes/
                finally { holder.unlockCanvasAndPost(canvas) }
                if (visible) {
                    val now = System.currentTimeMillis()
                    val next = renderer.nextChangeMillis(now)               // same Layer.next_change() contract
                    handler.postDelayed(this, next - now)
                }
            }
        }
        override fun onVisibilityChanged(v: Boolean) {
            visible = v
            handler.removeCallbacks(draw)
            if (v) handler.post(draw)                                       // nothing runs while hidden
        }
        override fun onSurfaceDestroyed(h: SurfaceHolder) { visible = false; handler.removeCallbacks(draw) }
    }
}
```

Porting notes:

* `core/` (angles, schedule, sun) is ~300 lines of dependency-free maths.
* `renderer/canvas.py` maps 1:1 onto `android.graphics.Canvas` (which is already
  anti-aliased, so the supersampling layer is unnecessary).
* Layer classes map onto Kotlin classes registered in a map by type name; the JSON
  theme files are shared unchanged.
* Use `wallclock export --size 1080x2400 --step 1` from a desktop to produce test
  frames and compare renderings pixel-for-pixel while porting.

### 3. Pre-rendered frames + automation apps

Tasker / MacroDroid can set the wallpaper from a file on a time trigger.  With
`wallclock export --schedule-only` you get one image per schedule slot (a handful of
changes a day, cheap).  Per-minute triggers are technically possible but wake the
device 1440 times a day; do not do that.
