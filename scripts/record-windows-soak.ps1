param([ValidateRange(1,1440)][int]$Minutes=3,[string]$Root=(Join-Path $env:LOCALAPPDATA 'Wallclock'),[switch]$AllowLongRun)
# Long validation is opt-in following the 2026-09-09 laptop slowdown report.
if($Minutes -gt 10 -and -not $AllowLongRun){throw 'Long tests are paused. Use a short controlled observation first; explicitly pass -AllowLongRun only after agreeing to resume.'}
& (Join-Path $PSScriptRoot 'observe-windows.ps1') -Minutes $Minutes -Root $Root
