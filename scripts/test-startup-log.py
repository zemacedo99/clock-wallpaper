"""Native fatal-startup logging regression; does not change wallpaper."""
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parent.parent
folder = root / 'build' / 'startup-log-test'
folder.mkdir(exist_ok=True)
log = folder / 'clock.log'
log.write_text('x' * 270000)
result = subprocess.run([str(root/'build/wallclock.exe'), '--config', str(folder/'missing.ini'), '--run', str(folder/'clock')], capture_output=True, text=True)
assert result.returncode == 1, result
assert log.stat().st_size < 4096
assert log.with_suffix('.log.1').stat().st_size == 270000
assert 'event=fatal' in log.read_text()
print('Passed fatal-startup log rotation regression (no desktop change)')
