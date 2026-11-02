"""Baseline-only benchmark. Requires Pillow; never applies a wallpaper."""
import ctypes
import json
import sys
import time
from datetime import timedelta
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]).resolve()))
from wallclock.app import App
from wallclock.core.config import Config
from wallclock.core.timeinfo import local_now
from wallclock.platforms.file import FileAdapter

root = Path(sys.argv[1]).resolve()
cfg = Config.load(root / "examples/config.json")
cfg.output_dir = root / "benchmark-output"
app = App(cfg, adapter=FileAdapter(), resolution=(3840, 2160))
class Counters(ctypes.Structure):
    _fields_ = [("cb", ctypes.c_ulong), ("PageFaultCount", ctypes.c_ulong)] + [(n, ctypes.c_size_t) for n in
        ("PeakWorkingSetSize", "WorkingSetSize", "QuotaPeakPagedPoolUsage", "QuotaPagedPoolUsage",
         "QuotaPeakNonPagedPoolUsage", "QuotaNonPagedPoolUsage", "PagefileUsage", "PeakPagefileUsage")]
def memory():
    c = Counters(); c.cb = ctypes.sizeof(c)
    kernel = ctypes.WinDLL("kernel32"); kernel.GetCurrentProcess.restype = ctypes.c_void_p
    psapi = ctypes.WinDLL("psapi"); psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_ulong]
    if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(c), c.cb):
        raise ctypes.WinError()
    return {"workingSetMiB": c.WorkingSetSize / 2**20, "peakMiB": c.PeakWorkingSetSize / 2**20}
records = []
now = local_now()
for i in range(5):
    cpu, wall = time.process_time(), time.perf_counter()
    app.update(now + timedelta(minutes=i))
    records.append({"wallMs": (time.perf_counter()-wall)*1000,
                    "cpuMs": (time.process_time()-cpu)*1000, **memory()})
print(json.dumps({"resolution": [3840, 2160], "adapter": "file (no desktop apply)", "samples": records}, indent=2))
