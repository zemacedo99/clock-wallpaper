# Test helper only: briefly expose and capture the desktop, restoring windows.
# The engine never captures the screen. Output remains local under build/.
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class WallpaperTestApi {
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SystemParametersInfoW")]
 public static extern bool Get(uint a,uint b,StringBuilder c,uint d);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SystemParametersInfoW")]
 public static extern bool Set(uint a,uint b,string c,uint d);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
}
'@
[void][WallpaperTestApi]::SetProcessDPIAware()
function Get-WallpaperPath {
 $value = New-Object System.Text.StringBuilder 32768
 if (-not [WallpaperTestApi]::Get(0x73,32768,$value,0)) { throw 'Cannot read wallpaper' }
 $value.ToString()
}
function Save-DesktopCapture([string]$Path) {
 $shell = New-Object -ComObject Shell.Application
 try {
  $shell.MinimizeAll()
  Start-Sleep -Milliseconds 700
  $bitmap = New-Object System.Drawing.Bitmap ([WallpaperTestApi]::GetSystemMetrics(0)),([WallpaperTestApi]::GetSystemMetrics(1))
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
   $graphics.CopyFromScreen(0,0,0,0,$bitmap.Size)
   $bitmap.Save($Path,[System.Drawing.Imaging.ImageFormat]::Png)
  } finally { $graphics.Dispose(); $bitmap.Dispose() }
 } finally { $shell.UndoMinimizeAll() }
}
