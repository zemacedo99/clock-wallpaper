$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
 . ./scripts/windows-capture.ps1
 $previous = Get-WallpaperPath
 $desktopKey = 'HKCU:\Control Panel\Desktop'
 $style = Get-ItemProperty $desktopKey
 $changedStyle = $false
 @{wallpaper=$previous;style=$style.WallpaperStyle;tile=$style.TileWallpaper} | ConvertTo-Json | Set-Content build/restore-original.json
 try {
  & ./build/astra-original.exe --config examples/minimal.ini --once build/original --auto-size
  if ($LASTEXITCODE -ne 0) { throw 'Original executable failed' }
  Start-Sleep -Seconds 1
  Save-DesktopCapture ((Join-Path (Get-Location) 'build/original-visible.png'))
  # Change just the application flags: same BMP and same layout.
  $bmp = (Resolve-Path build/original-0.bmp).Path
  if (-not [WallpaperTestApi]::Set(0x14,0,$bmp,3)) { throw 'Flags probe failed' }
  Start-Sleep -Seconds 1
  Save-DesktopCapture ((Join-Path (Get-Location) 'build/flags-visible.png'))
  # Python's style and PNG path, separate from the flags experiment.
  $changedStyle = $true
  Set-ItemProperty $desktopKey WallpaperStyle '10'
  Set-ItemProperty $desktopKey TileWallpaper '0'
  & ./build/astra-original.exe --config examples/minimal.ini --render build/original-png.png --auto-size
  if (-not [WallpaperTestApi]::Set(0x14,0,(Resolve-Path build/original-png.png).Path,3)) { throw 'PNG probe failed' }
  Start-Sleep -Seconds 1
  Save-DesktopCapture ((Join-Path (Get-Location) 'build/png-visible.png'))
  'Original/flags/PNG desktop captures saved.'
 } finally {
  try {
   if ($changedStyle) {
    Set-ItemProperty $desktopKey WallpaperStyle $style.WallpaperStyle
    Set-ItemProperty $desktopKey TileWallpaper $style.TileWallpaper
   }
  } finally {
   if (-not [WallpaperTestApi]::Set(0x14,0,$previous,3)) { throw 'RESTORE FAILED' }
  }
  'Previous wallpaper and layout restored.'
 }
} finally { Pop-Location }
