# Explicit integration checks. Leaves login enabled only when -EnableLogin is supplied.
param([switch]$EnableLogin)
$ErrorActionPreference='Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
 . ./scripts/windows-capture.ps1
 if(Get-Process wallclock,wallclock_startup -ErrorAction SilentlyContinue){throw 'Stop active clock processes before this isolated test'}
 $root=(Get-Location).Path;$launcher=Join-Path $root 'build/wallclock_startup.exe'
 $evidence=Join-Path $root ('build/startup-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
 New-Item -ItemType Directory $evidence | Out-Null
 $previous=Get-WallpaperPath;$style=Get-ItemProperty 'HKCU:\Control Panel\Desktop'
 @{wallpaper=$previous;style=$style.WallpaperStyle;tile=$style.TileWallpaper} | ConvertTo-Json | Set-Content (Join-Path $evidence 'restore.json')
 $runKey='HKCU:\Software\Microsoft\Windows\CurrentVersion\Run';$settings='HKCU:\Software\Wallclock'
 function Read-OptionalProperty([string]$Path,[string]$Name){
  $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($Path.Substring(6))
  if($key){try{$key.GetValue($Name,$null)}finally{$key.Dispose()}}
 }
 $oldRun=Read-OptionalProperty $runKey 'Wallclock'
 $oldConfig=Read-OptionalProperty $settings 'Config'
 $oldOutput=Read-OptionalProperty $settings 'Output'
 $supervisor=$null;$success=$false;$results=@{}
 function Invoke-Launcher([string]$Arguments){$p=Start-Process $launcher -ArgumentList $Arguments -WindowStyle Hidden -PassThru -Wait;if($p.ExitCode -ne 0){throw "Launcher exit $($p.ExitCode): $Arguments"}}
 function Stop-Owned {
  if($supervisor -and -not $supervisor.HasExited){Invoke-Launcher '--stop';if(-not $supervisor.WaitForExit(15000)){Stop-Process -Id $supervisor.Id;throw 'Supervisor did not stop gracefully'}}
 }
 try {
  # Missing config is a real fatal startup failure before wallpaper configuration.
  $badArgs='--start --config "'+(Join-Path $evidence 'missing.ini')+'" --output "'+(Join-Path $evidence 'missing')+'"'
  $supervisor=Start-Process $launcher -ArgumentList $badArgs -WindowStyle Hidden -PassThru
  Start-Sleep -Seconds 4
  $failureLog=Join-Path $evidence 'missing.log'
  if(-not (Test-Path $failureLog)){throw 'Startup failure log missing'}
  $initial=@(Get-Content $failureLog | Where-Object {$_ -match 'event=fatal'}).Count
  if($initial -ne 1){throw 'Unexpected initial startup failure count'}
  Write-Output 'Confirmed fatal startup logging; waiting for the conservative 60-second retry'
  for($i=0;$i -lt 8;$i++){Start-Sleep -Seconds 8}
  $fatal=@(Get-Content $failureLog | Where-Object {$_ -match 'event=fatal'})
  if($fatal.Count -ne 2){throw "Expected exactly two attempts, got $($fatal.Count)"}
  $epochs=@($fatal | ForEach-Object {[long]([regex]::Match($_,'epoch=(\d+)').Groups[1].Value)})
  if($epochs[1]-$epochs[0] -lt 60){throw 'Rapid restart detected'}
  $watch=[Diagnostics.Stopwatch]::StartNew();Stop-Owned;$watch.Stop()
  $results.failureAttempts=$fatal.Count;$results.failureIntervalSeconds=$epochs[1]-$epochs[0];$results.stopDuringBackoffMs=$watch.Elapsed.TotalMilliseconds
  Write-Output 'Verified bounded restart and immediate stop during the 120-second backoff'
  # Abrupt parent termination must not leave its child alive.
  $config=Join-Path $root 'examples/minimal.ini'
  $args='--start --config "'+$config+'" --output "'+(Join-Path $evidence 'owned child')+'"'
  $supervisor=Start-Process $launcher -ArgumentList $args -WindowStyle Hidden -PassThru
  Start-Sleep -Seconds 6
  $child=Get-CimInstance Win32_Process -Filter "ParentProcessId=$($supervisor.Id) AND Name='wallclock.exe'"
  if(-not $child){throw 'Owned child not started'}
  $owned=Get-Process -Id $child.ProcessId
  Stop-Process -Id $supervisor.Id
  if(-not $owned.WaitForExit(5000)){throw 'Supervisor death left an orphan child'}
  $results.jobCleanup=$true
  # Exercise install and the exact no-argument startup mode stored in Run.
  $daily=Join-Path $root 'build/daily';New-Item -ItemType Directory -Force $daily | Out-Null
  Invoke-Launcher ('--install --config "'+$config+'" --output "'+(Join-Path $daily 'clock')+'"')
  $registered=Get-ItemPropertyValue $runKey Wallclock
  if($registered -ne ('"'+$launcher+'" --start')){throw 'Unexpected registered command'}
  $supervisor=Start-Process $launcher -ArgumentList '--start' -WindowStyle Hidden -PassThru
  Start-Sleep -Seconds 7
  if($supervisor.HasExited -or -not (Get-WallpaperPath).StartsWith((Join-Path $daily 'clock-'))){throw 'Registered startup command did not apply wallpaper'}
  Invoke-Launcher '--status'
  Stop-Owned
  $dailyLog=Get-Content (Join-Path $daily 'clock.log') -Raw
  if($dailyLog -notmatch 'event=stopped'){throw 'Engine did not record graceful stop'}
  $results.registeredCommand=$registered;$results.registeredCommandApplied=$true;$results.gracefulStop=$true;$results.loginEnabled=[bool]$EnableLogin
  $results | ConvertTo-Json | Set-Content (Join-Path $evidence 'results.json')
  $success=$true
  Write-Output "Startup integration passed: $evidence"
 } finally {
  try { Stop-Owned } finally {
   try { if(-not $EnableLogin -or -not $success){
   if($null -eq $oldRun){Remove-ItemProperty $runKey Wallclock -ErrorAction SilentlyContinue}else{Set-ItemProperty $runKey Wallclock $oldRun}
   foreach($entry in @(@('Config',$oldConfig),@('Output',$oldOutput))){if($null -eq $entry[1]){Remove-ItemProperty $settings $entry[0] -ErrorAction SilentlyContinue}else{Set-ItemProperty $settings $entry[0] $entry[1]}}
   } } finally {
  Copy-Item -LiteralPath (Join-Path $env:LOCALAPPDATA 'Wallclock/startup.log') -Destination (Join-Path $evidence 'supervisor.log') -ErrorAction SilentlyContinue
  try {Set-ItemProperty 'HKCU:\Control Panel\Desktop' WallpaperStyle $style.WallpaperStyle;Set-ItemProperty 'HKCU:\Control Panel\Desktop' TileWallpaper $style.TileWallpaper}
  finally {if(-not [WallpaperTestApi]::Set(0x14,0,$previous,3)){throw 'RESTORATION FAILED: use restore.json'}}
  Write-Output 'Previous wallpaper restored; all test processes stopped'
   }
  }
 }
} finally {Pop-Location}
