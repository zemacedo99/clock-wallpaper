"""Command line interface.

    wallclock init [DIR]              copy the example config/theme/backgrounds to ~/.wallclock
    wallclock once                    render for now, set as wallpaper, exit (for cron/Task Scheduler)
    wallclock run                     stay resident, sleeping until the next visual change
    wallclock render [--time HH:MM]   render to a file without touching the wallpaper
    wallclock export --out DIR        pre-render a day of frames (phones / Shortcuts)
    wallclock info                    platform, resolution, active slot, next change
"""
from __future__ import annotations

import argparse
import logging
import shutil
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Tuple

from . import __version__
from .core.config import DEFAULT_CONFIG_DIR, DEFAULT_CONFIG_FILE, Config, parse_resolution
from .core.timeinfo import local_now, parse_hhmm
from .platforms import NotSupported, get_adapter
from .themes.registry import registered_types

EXAMPLES_DIR = Path(__file__).resolve().parent.parent / "examples"


def _parse_time(value: Optional[str]) -> datetime:
    now = local_now()
    if not value:
        return now
    if "T" in value or "-" in value:
        dt = datetime.fromisoformat(value)
        return dt if dt.tzinfo else dt.replace(tzinfo=now.tzinfo)
    t = parse_hhmm(value)
    return now.replace(hour=t.hour, minute=t.minute, second=t.second, microsecond=0)


def _size(value: Optional[str]) -> Optional[Tuple[int, int]]:
    r = parse_resolution(value) if value else "auto"
    return None if r == "auto" else r  # type: ignore[return-value]


def _load_app(args):
    from .app import App
    cfg = Config.load(args.config)
    if getattr(args, "theme", None):
        cfg.theme = args.theme
    adapter = get_adapter(args.platform) if getattr(args, "platform", None) else None
    return App(cfg, adapter=adapter, resolution=_size(getattr(args, "size", None)))


def cmd_init(args) -> int:
    target = Path(args.dir).expanduser() if args.dir else DEFAULT_CONFIG_DIR
    if target.exists() and any(target.iterdir()) and not args.force:
        print(f"{target} already exists (use --force to overwrite)")
        return 1
    shutil.copytree(EXAMPLES_DIR, target, dirs_exist_ok=True)
    print(f"example configuration copied to {target}")
    print(f"edit {target / 'config.json'} and {target / 'schedule.txt'}, then run: wallclock once")
    return 0


def cmd_once(args) -> int:
    app = _load_app(args)
    nxt = app.run_once()
    print(f"next change at {nxt.strftime('%Y-%m-%d %H:%M:%S')}")
    return 0


def cmd_run(args) -> int:
    app = _load_app(args)
    logging.getLogger().info("wallclock %s running on %s at %sx%s", __version__, app.adapter.describe(), *app.resolution())
    try:
        app.run_forever()
    except KeyboardInterrupt:
        pass
    return 0


def cmd_render(args) -> int:
    args.platform = "file"
    app = _load_app(args)
    now = _parse_time(args.time)
    img = app.render(now)
    out = Path(args.out).expanduser()
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"rendered {now.strftime('%H:%M:%S')} -> {out}")
    return 0


def cmd_export(args) -> int:
    args.platform = "file"
    app = _load_app(args)
    out = Path(args.out).expanduser()
    out.mkdir(parents=True, exist_ok=True)
    base = local_now().replace(hour=0, minute=0, second=0, microsecond=0)
    times = []
    if args.schedule_only:
        if not app.config.schedule:
            print("no schedule configured")
            return 1
        times = [base.replace(hour=s.start.hour, minute=s.start.minute) for s in app.config.schedule.slots]
    else:
        step = timedelta(minutes=args.step)
        t = base
        while t < base + timedelta(days=1):
            times.append(t)
            t += step
    for t in times:
        path = out / f"{t.strftime('%H%M')}.png"
        app.render(t).save(path)
    print(f"exported {len(times)} frames to {out}")
    return 0


def cmd_info(args) -> int:
    app = _load_app(args)
    now = local_now()
    ctx = app.context(now)
    print(f"wallclock {__version__}")
    print(f"platform   : {app.adapter.describe()}")
    print(f"resolution : {'x'.join(map(str, app.resolution()))}")
    print(f"theme      : {app.theme.name} ({app.config.theme_path})")
    print(f"layers     : {', '.join(l.name for l in app.theme.layers)}")
    print(f"layer types: {', '.join(registered_types())}")
    if app.config.schedule:
        slot = ctx.slot
        print(f"schedule   : {len(app.config.schedule)} slots; active {slot.label if slot else '-'}"
              f" image={slot.image if slot else '-'} caption={slot.caption if slot else '-'}")
    if app.sun:
        dl = app.sun.daylight(now)
        print(f"sun        : rise {dl[0].strftime('%H:%M') if dl else '-'} set {dl[1].strftime('%H:%M') if dl else '-'}")
    nxt = app.next_change(now)
    print(f"next change: {nxt.strftime('%H:%M:%S') if nxt else 'never (static theme)'}")
    print(f"output dir : {app.config.output_path}")
    return 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="wallclock", description="Battery-friendly dynamic clock wallpaper.")
    p.add_argument("-c", "--config", default=str(DEFAULT_CONFIG_FILE), help="path to config.json")
    p.add_argument("-t", "--theme", help="theme file, overriding the one in config.json")
    p.add_argument("-v", "--verbose", action="store_true")
    p.add_argument("--version", action="version", version=f"wallclock {__version__}")
    sub = p.add_subparsers(dest="command", required=True)

    s = sub.add_parser("init", help="copy the example configuration")
    s.add_argument("dir", nargs="?")
    s.add_argument("--force", action="store_true")
    s.set_defaults(func=cmd_init)

    for name, func, help_ in (("once", cmd_once, "update the wallpaper once and exit"),
                              ("run", cmd_run, "run resident, updating only when needed"),
                              ("info", cmd_info, "show detected settings")):
        s = sub.add_parser(name, help=help_)
        s.add_argument("--platform", help="windows|macos|linux|android|ios|file (default: auto)")
        s.add_argument("--size", help="override resolution, e.g. 2560x1440")
        s.set_defaults(func=func)

    s = sub.add_parser("render", help="render a preview image")
    s.add_argument("--time", help="HH:MM or ISO timestamp (default: now)")
    s.add_argument("--size", help="e.g. 1920x1080")
    s.add_argument("-o", "--out", default="wallclock-preview.png")
    s.set_defaults(func=cmd_render)

    s = sub.add_parser("export", help="pre-render frames for platforms without a wallpaper API")
    s.add_argument("--out", required=True)
    s.add_argument("--size", help="e.g. 1170x2532 for a phone")
    s.add_argument("--step", type=int, default=1, help="minutes between frames (default 1 -> 1440 frames)")
    s.add_argument("--schedule-only", action="store_true", help="one frame per schedule slot")
    s.set_defaults(func=cmd_export)
    return p


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)
    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    try:
        return args.func(args)
    except NotSupported as exc:
        print(f"not supported: {exc}", file=sys.stderr)
        return 2
    except FileNotFoundError as exc:
        print(f"missing file: {exc}\n(run `wallclock init` to create an example configuration)", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
