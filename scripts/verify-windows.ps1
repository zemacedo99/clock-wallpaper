param([int]$Samples = 5, [switch]$InjectFailure)
$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
 . ./scripts/windows-capture.ps1
 $root = (Get-Location).Path
 $evidence = Join-Path $root ('build/verification-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
 New-Item -ItemType Directory $evidence | Out-Null
 $prefix = Join-Path $evidence 'clock'
 $previous = Get-WallpaperPath
 $style = Get-ItemProperty 'HKCU:\Control Panel\Desktop'
 @{wallpaper=$previous;style=$style.WallpaperStyle;tile=$style.TileWallpaper} | ConvertTo-Json | Set-Content (Join-Path $evidence 'restore.json')
 $runner = $null; $block = $null; $records = @(); $idle = @(); $last = ''; $injected = $false
 try {
  $arguments = '--config "' + (Join-Path $root 'examples/minimal.ini') + '" --run "' + $prefix + '" --auto-size'
  $runner = Start-Process -FilePath (Join-Path $root 'build/wallclock.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $evidence 'stdout.log') -RedirectStandardError (Join-Path $evidence 'stderr.log')
  $started = [DateTimeOffset]::UtcNow
  Start-Sleep -Seconds 3
  for ($attempt = 0; $attempt -lt ($Samples + 3) -and $records.Count -lt $Samples; $attempt++) {
   $runner.Refresh()
   if ($runner.HasExited) { throw ('Runner exited early: ' + (Get-Content (Join-Path $evidence 'stderr.log') -Raw)) }
   $selected = Get-WallpaperPath
   if ($selected.StartsWith($prefix + '-') -and $selected -ne $last) {
    $index = $records.Count
    Copy-Item -LiteralPath $selected -Destination (Join-Path $evidence "$index-generated.bmp")
    Save-DesktopCapture (Join-Path $evidence "$index-desktop.png")
    $runner.Refresh()
    $records += [pscustomobject]@{index=$index;time=[DateTimeOffset]::Now.ToString('o');file=$selected;cpuMs=$runner.TotalProcessorTime.TotalMilliseconds;workingSetMiB=$runner.WorkingSet64/1MB}
    $last = $selected
    Write-Output "Verified capture $index at $([DateTime]::Now.ToString('HH:mm:ss'))"
   }
   if ($block) { $block.Dispose(); $block = $null; Write-Output 'Released injected output-file lock.' }
   if ($records.Count -ge $Samples) { break }
   $secondsLeft = 60 - [DateTime]::Now.Second
   if ($secondsLeft -gt 12) {
    $runner.Refresh(); $cpuBefore = $runner.TotalProcessorTime.TotalMilliseconds
    Start-Sleep -Seconds 8
    $runner.Refresh()
    $idle += [pscustomobject]@{seconds=8;cpuMs=$runner.TotalProcessorTime.TotalMilliseconds-$cpuBefore;workingSetMiB=$runner.WorkingSet64/1MB}
   }
   if ($InjectFailure -and -not $injected -and $records.Count -ge 2) {
    $block = [System.IO.File]::Open($prefix+'-pending.bmp',[System.IO.FileMode]::OpenOrCreate,[System.IO.FileAccess]::ReadWrite,[System.IO.FileShare]::None)
    $injected = $true
    Write-Output 'Injected one transient output-file lock for the next update.'
   }
   # Verification harness sleeps to the next minute plus a compositor grace period.
   # This is not the engine's scheduler; the engine uses its native one-shot wait.
   $wake = [DateTime]::Now.AddMinutes(1)
   $wake = $wake.AddSeconds(-$wake.Second).AddMilliseconds(-$wake.Millisecond).AddSeconds(3)
   $delay = [Math]::Max(1,($wake-[DateTime]::Now).TotalMilliseconds)
   Start-Sleep -Milliseconds ([int]$delay)
  }
  if ($records.Count -lt $Samples) { throw 'Not enough distinct wallpaper captures' }
  $engineLog = Get-Content ($prefix+'.log')
  if ($InjectFailure -and (-not ($engineLog -match 'event=retry') -or -not ($engineLog -match 'event=recovered'))) { throw 'Recovery was not observed' }
  $runner.Refresh()
  @{samples=$records;idle=$idle;durationSeconds=([DateTimeOffset]::UtcNow-$started).TotalSeconds;cpuMs=$runner.TotalProcessorTime.TotalMilliseconds;injected=$injected} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $evidence 'measurements.json')
  Write-Output "Evidence: $evidence"
 } finally {
  if ($block) { $block.Dispose() }
  if ($runner -and -not $runner.HasExited) { Stop-Process -Id $runner.Id }
  try {
   Set-ItemProperty 'HKCU:\Control Panel\Desktop' WallpaperStyle $style.WallpaperStyle
   Set-ItemProperty 'HKCU:\Control Panel\Desktop' TileWallpaper $style.TileWallpaper
  } finally {
   if (-not [WallpaperTestApi]::Set(0x14,0,$previous,3)) { throw 'RESTORATION FAILED: use restore.json' }
  }
  Write-Output 'Verification runner stopped; previous wallpaper and layout restored.'
 }
} finally { Pop-Location }
