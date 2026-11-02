"""Compare real desktop pixels with generated files; Pillow is test-only."""
import hashlib
import json
import statistics
import sys
import argparse
from pathlib import Path
from PIL import Image, ImageChops, ImageStat

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory')
parser.add_argument('--exclude-frame', action='append', default=[], metavar='INDEX:REASON',
                    help='Explicitly report an occluded/unusable capture; never count it as a visual pass')
args = parser.parse_args()
excluded = {}
for item in args.exclude_frame:
    index, separator, reason = item.partition(':')
    if not separator or not reason.strip():
        parser.error('Exclusions require INDEX:REASON')
    excluded[int(index)] = reason
root = Path(args.directory)
measurements = json.loads((root / 'measurements.json').read_text(encoding='utf-8-sig'))
assert set(excluded).issubset({r['index'] for r in measurements['samples']}), 'Unknown excluded frame'
previous_generated = previous_desktop = None
visual = []
for record in measurements['samples']:
    i = record['index']
    if i in excluded:
        continue
    generated = Image.open(root / f'{i}-generated.bmp').convert('RGB')
    desktop = Image.open(root / f'{i}-desktop.png').convert('RGB')
    assert generated.size == desktop.size, 'DPI/size mismatch'
    w, h = generated.size
    # Central dial only: excludes desktop icons, taskbar, and notification areas.
    box = (int(w*.36), int(h*.25), int(w*.64), int(h*.75))
    a, b = generated.crop(box), desktop.crop(box)
    diff = ImageChops.difference(a, b)
    error = statistics.mean(ImageStat.Stat(diff).mean)
    result = {'index': i, 'resolution': [w,h], 'dialMeanAbsoluteError': error,
              'generatedHash': hashlib.sha256(a.tobytes()).hexdigest(),
              'desktopHash': hashlib.sha256(b.tobytes()).hexdigest()}
    if previous_generated is not None:
        changed = ImageChops.difference(a, previous_generated).convert('L').point(lambda x: 255 if x > 16 else 0)
        count = changed.histogram()[255]
        assert count > 0, 'Generated clock did not change'
        changed_error = statistics.mean(ImageStat.Stat(diff, changed).mean)
        assert changed_error < 12, f'Desktop does not match the changed clock pixels: {changed_error}'
        assert b.tobytes() != previous_desktop.tobytes(), 'Visible desktop clock did not change'
        result.update(changedClockPixels=count, changedPixelsMeanAbsoluteError=changed_error)
    assert error < 8, f'Desktop dial differs from generated image: {error}'
    visual.append(result)
    previous_generated, previous_desktop = a,b

timings = []
for line in (root / 'clock.log').read_text().splitlines():
    if 'event=applied' in line:
        fields = dict(part.split('=',1) for part in line.split() if '=' in part)
        timings.append({k:float(fields[k]) for k in ('render_ms','write_hash_ms','apply_ms','total_ms')})
assert len(visual) >= 2, 'Need at least two usable captures to verify changes'
summary = {'visual': visual, 'excludedFrames': excluded, 'timings': timings,
           'timingMediansMs': {k:statistics.median(t[k] for t in timings) for k in timings[0]},
           'idle': measurements['idle'], 'durationSeconds': measurements['durationSeconds'],
           'cpuMs': measurements['cpuMs'],
           'oneCoreAveragePercent': measurements['cpuMs']/measurements['durationSeconds']/10,
           'workingSetMiB': [s['workingSetMiB'] for s in measurements['samples']]}
(root / 'analysis.json').write_text(json.dumps(summary,indent=2))
print(json.dumps(summary,indent=2))
