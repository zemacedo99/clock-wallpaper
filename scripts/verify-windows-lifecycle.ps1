param([int]$Minutes = 15)
$ErrorActionPreference = 'Stop'
if ($Minutes -lt 12) { throw 'Use at least 12 minutes to cover fault recovery and steady operation' }
Push-Location (Join-Path $PSScriptRoot '..')
try {
 . ./scripts/windows-capture.ps1
 Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public static class LifecycleProbe {
 public delegate bool EnumProc(IntPtr h, IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr param);
 [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
 [DllImport("user32.dll",SetLastError=true)] static extern IntPtr SendMessageTimeout(IntPtr h,uint msg,IntPtr wp,IntPtr lp,uint flags,uint timeout,out IntPtr result);
 public static void Send(int pid,uint msg,int wp) {
  bool found=false;
  EnumWindows((h,p)=>{uint owner;GetWindowThreadProcessId(h,out owner);if(owner==(uint)pid){IntPtr result;if(SendMessageTimeout(h,msg,(IntPtr)wp,IntPtr.Zero,2,5000,out result)==IntPtr.Zero)throw new Exception("Event delivery timed out");found=true;}return true;},IntPtr.Zero);
  if(!found)throw new Exception("Engine event window not found");
 }
}
"@
 $root = (Get-Location).Path
 $evidence = Join-Path $root ('build/lifecycle-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
 New-Item -ItemType Directory $evidence | Out-Null
 $prefix = Join-Path $evidence 'clock'
 $startup = Join-Path $root 'build/wallclock_startup.exe'
 if (Get-Process wallclock,wallclock_startup -ErrorAction SilentlyContinue) { throw 'Stop existing clock instances before this isolated test' }
 $previous = Get-WallpaperPath; $style = Get-ItemProperty 'HKCU:\Control Panel\Desktop'
 @{wallpaper=$previous;style=$style.WallpaperStyle;tile=$style.TileWallpaper} | ConvertTo-Json | Set-Content (Join-Path $evidence 'restore.json')
 $supervisor=$null; $block=$null; $samples=@(); $idle=@(); $processSamples=@(); $last=''; $retiredCpu=0; $lockedFile=''
 function Child {
  $children=@(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($supervisor.Id) AND Name='wallclock.exe'")
  if($children.Count -gt 1){throw 'Multiple engine children'}
  if($children.Count){Get-Process -Id $children[0].ProcessId -ErrorAction Stop}
 }
 try {
  $args='--start --config "'+(Join-Path $root 'examples/minimal.ini')+'" --output "'+$prefix+'"'
  $supervisor=Start-Process $startup -ArgumentList $args -WindowStyle Hidden -PassThru
  $started=[DateTimeOffset]::UtcNow
  Start-Sleep -Seconds 6
  $duplicate=Start-Process $startup -ArgumentList $args -WindowStyle Hidden -PassThru -Wait
  if($duplicate.ExitCode -ne 2){throw 'Duplicate supervisor was not rejected'}
  $dupEngine=Start-Process (Join-Path $root 'build/wallclock.exe') -ArgumentList ('--config "'+(Join-Path $root 'examples/minimal.ini')+'" --run "'+(Join-Path $evidence 'duplicate')+'" --auto-size') -WindowStyle Hidden -PassThru -Wait
  if($dupEngine.ExitCode -ne 2){throw 'Duplicate engine was not rejected'}
  Write-Output "Long supervised test started; evidence=$evidence"
  for($minute=0;$minute -le $Minutes;$minute++) {
   $supervisor.Refresh();if($supervisor.HasExited){throw 'Supervisor exited unexpectedly'}
   $engine=Child
   $selected=Get-WallpaperPath
   if($minute -eq 5 -and $selected -ne $lockedFile){throw 'Wallpaper changed while synthetically locked'}
   if($engine) {
    $engine.Refresh();$supervisor.Refresh()
    $processSamples += [pscustomobject]@{minute=$minute;time=[DateTimeOffset]::Now.ToString('o');pid=$engine.Id;cpuMs=$engine.TotalProcessorTime.TotalMilliseconds;workingSetMiB=$engine.WorkingSet64/1MB;privateMiB=$engine.PrivateMemorySize64/1MB;handles=$engine.HandleCount;supervisorCpuMs=$supervisor.TotalProcessorTime.TotalMilliseconds;supervisorMiB=$supervisor.WorkingSet64/1MB;supervisorHandles=$supervisor.HandleCount}
   }
   if($selected.StartsWith($prefix+'-') -and $selected -ne $last) {
    $index=$samples.Count
    Copy-Item -LiteralPath $selected -Destination (Join-Path $evidence "$index-generated.bmp")
    Save-DesktopCapture (Join-Path $evidence "$index-desktop.png")
    $samples += [pscustomobject]@{index=$index;minute=$minute;time=[DateTimeOffset]::Now.ToString('o');file=$selected;workingSetMiB=if($engine){$engine.WorkingSet64/1MB}else{0}}
    $last=$selected
   }
   if($minute -eq 2){$block.Dispose();$block=$null;Write-Output 'Released output lock after one failed minute'}
   if($minute -eq 1){$block=[IO.File]::Open($prefix+'-pending.bmp',[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);Write-Output 'Injected output lock'}
   if($minute -eq 4){[LifecycleProbe]::Send($engine.Id,0x02B1,7);$lockedFile=Get-WallpaperPath;Write-Output 'Synthetic session lock for one full boundary'}
   if($minute -eq 5){
    [LifecycleProbe]::Send($engine.Id,0x02B1,8);[LifecycleProbe]::Send($engine.Id,0x0218,4);[LifecycleProbe]::Send($engine.Id,0x0218,18);[LifecycleProbe]::Send($engine.Id,0x001E,0)
    Start-Sleep -Seconds 6
    if((Get-WallpaperPath) -eq $lockedFile){throw 'Unlock/resume failed to apply immediately; sent-message wake may be lost'}
    Write-Output 'Synthetic unlock/suspend/resume/time-change refreshed immediately; system state unchanged'
   }
   if($minute -eq 7){$engine.Refresh();$retiredCpu+=$engine.TotalProcessorTime.TotalMilliseconds;Stop-Process -Id $engine.Id;Write-Output 'Terminated child to verify conservative supervisor restart';$engine=$null}
   Write-Output "minute=$minute captures=$($samples.Count) child=$(if($engine){$engine.Id}else{'backoff'})"
   if($minute -eq $Minutes){break}
   if($engine -and $minute -ne 5) {
    $engine.Refresh();$supervisor.Refresh();$cpu=$engine.TotalProcessorTime.TotalMilliseconds;$superCpu=$supervisor.TotalProcessorTime.TotalMilliseconds
    Start-Sleep -Seconds 8
    $engine.Refresh();$supervisor.Refresh()
    $idle += [pscustomobject]@{minute=$minute;seconds=8;cpuMs=$engine.TotalProcessorTime.TotalMilliseconds-$cpu;supervisorCpuMs=$supervisor.TotalProcessorTime.TotalMilliseconds-$superCpu;workingSetMiB=$engine.WorkingSet64/1MB}
   }
   $wake=[DateTime]::Now.AddMinutes(1);$wake=$wake.AddSeconds(-$wake.Second).AddMilliseconds(-$wake.Millisecond).AddSeconds(5)
   Start-Sleep -Milliseconds ([int][Math]::Max(1,($wake-[DateTime]::Now).TotalMilliseconds))
  }
  $engine=Child;if(-not $engine){throw 'No child after restart'}
  $engine.Refresh();$supervisor.Refresh()
  $log=Get-Content ($prefix+'.log') -Raw
  if($log -notmatch 'event=retry' -or $log -notmatch 'event=recovered'){throw 'Output recovery not observed'}
  if(@($processSamples.pid | Select-Object -Unique).Count -ne 2){throw 'Expected exactly one supervised child restart'}
  if($samples.Count -lt $Minutes-4){throw 'Too few changing wallpapers'}
  @{samples=$samples;idle=$idle;processSamples=$processSamples;durationSeconds=([DateTimeOffset]::UtcNow-$started).TotalSeconds;cpuMs=$retiredCpu+$engine.TotalProcessorTime.TotalMilliseconds;supervisorCpuMs=$supervisor.TotalProcessorTime.TotalMilliseconds;physicalLifecycleTested=$false} | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $evidence 'measurements.json')
  Write-Output "Long run passed: $evidence"
 } finally {
  if($block){$block.Dispose()}
  if($supervisor -and -not $supervisor.HasExited){$stop=Start-Process $startup -ArgumentList '--stop' -WindowStyle Hidden -PassThru -Wait;if(-not $supervisor.WaitForExit(20000)){Stop-Process -Id $supervisor.Id}}
  Copy-Item -LiteralPath (Join-Path $env:LOCALAPPDATA 'Wallclock/startup.log') -Destination (Join-Path $evidence 'supervisor.log') -ErrorAction SilentlyContinue
  try {Set-ItemProperty 'HKCU:\Control Panel\Desktop' WallpaperStyle $style.WallpaperStyle;Set-ItemProperty 'HKCU:\Control Panel\Desktop' TileWallpaper $style.TileWallpaper}
  finally {if(-not [WallpaperTestApi]::Set(0x14,0,$previous,3)){throw 'RESTORATION FAILED: use restore.json'}}
  Write-Output 'Supervisor/child stopped; previous wallpaper/layout restored'
 }
} finally {Pop-Location}
