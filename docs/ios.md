# iOS

## What the OS allows

* There is **no public API** for an app to set the wallpaper.  Nothing in this repo
  can change it for you, and no third-party app can either.
* Apps cannot run a timer in the background every minute; background refresh is
  opportunistic and rate-limited by the system.
* The Shortcuts app can change wallpapers from a *Personal Automation* with a
  "Time of Day" trigger.  Since iOS 16 the reliable action is **Switch Between
  Wallpapers**, which selects one of the wallpaper sets you created in Settings;
  an action that sets an arbitrary photo has come and gone between versions, so
  check what your iOS version offers.  Automations can run without confirmation.

So: the *schedule* part of this project (06:00 morning, 09:00 work, 18:00 "Leave
office", ...) maps well onto iOS.  A minute hand does not - that would need 1440
automations firing every minute, which the system will not run reliably and
which would drain the battery even if it did.

## Practical approach

1. On a desktop, render one frame per schedule slot at your screen size
   (iPhone sizes are e.g. 1170x2532, 1179x2556, 1290x2796):

       wallclock export --schedule-only --size 1179x2556 --out frames/

   The hands in each frame show the slot's start time; if you prefer a
   hand-less look for phones, make a copy of the theme without the `hands` layer
   and pass it with `-t`.

2. AirDrop / sync the frames to the phone and create one wallpaper set per frame
   in Settings > Wallpaper.

3. In Shortcuts > Automation, add a "Time of Day" automation for each schedule
   time with the action "Switch Between Wallpapers" -> that wallpaper set, and
   turn off "Ask Before Running".

## Alternatives that show a real clock

* The Lock Screen already shows a large clock over your wallpaper; a theme with
  only `background` + `markers` layers gives the "minimal dial" look while iOS
  draws the time itself.
* Lock Screen widgets (WidgetKit) can display time-based content with the
  system deciding when to refresh - the closest thing to a custom hand you can
  ship on iOS, but it lives in a widget slot, not in the wallpaper.
