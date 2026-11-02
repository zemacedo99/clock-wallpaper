[CmdletBinding()]
param(
 [Parameter(Position=0)][ValidateSet('install','run','status','stop','restart','preview','uninstall','startup','theme','cleanup')][string]$Command='status',
 [Parameter(Position=1)][string]$Value,
 [string]$Root=(Join-Path $env:LOCALAPPDATA 'Wallclock'),
 [switch]$NoStartup,[switch]$KeepWallpaper,[string]$At='10:10',[string]$Size='1920x1080'
)
$ErrorActionPreference='Stop'
$Root=[IO.Path]::GetFullPath($Root).TrimEnd('\','/')
if($Root -eq [IO.Path]::GetPathRoot($Root).TrimEnd('\')){throw 'Installation root cannot be a drive root'}
$bin=Join-Path $Root 'bin';$config=Join-Path $Root 'config/active.ini';$prefix=Join-Path $Root 'generated/clock';$engineLog=Join-Path $Root 'logs/engine.log';$launcher=Join-Path $bin 'wallclock_startup.exe'
function Safe([string]$Path){
 $full=[IO.Path]::GetFullPath($Path)
 if(-not $full.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase) -and $full -ne $Root){throw "Path escapes installation: $full"}
 $part=$full
 while($part){
  if(Test-Path -LiteralPath $part){$item=Get-Item -LiteralPath $part -Force;if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw "Refusing reparse point: $part"}}
  $parent=[IO.Path]::GetDirectoryName($part);if($parent -eq $part){break};$part=$parent
 }
 return $full
}
function Directory([string]$Path){[void](Safe $Path);New-Item -ItemType Directory -Force -Path $Path | Out-Null}
function File-Hash([string]$Path){
 $stream=[IO.File]::OpenRead($Path);$hash=[Security.Cryptography.SHA256]::Create()
 try{[BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-','')}finally{$stream.Dispose();$hash.Dispose()}
}
function Package-Target([string]$Relative){
 if([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)'){throw 'Invalid release manifest path'}
 $target=Safe (Join-Path $bin $Relative)
 if(-not $target.StartsWith($bin+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Release manifest escapes bin'}
 $target
}
function Assert-NoEngine {
 $enginePath=Join-Path $bin 'wallclock.exe'
 if(@(Get-CimInstance Win32_Process -Filter "Name='wallclock.exe'" | Where-Object {[string]::Equals($_.ExecutablePath,$enginePath,[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'An engine from this installation is still running; stop the manual runner before modifying files'}
}
function Quote([string]$Text){if($Text.Contains('"')){throw 'Quotes are not allowed in file paths'};'"'+$Text+'"'}
function Owned {
 @(Get-CimInstance Win32_Process -Filter "Name='wallclock_startup.exe'" | Where-Object {$_.ExecutablePath -and [string]::Equals($_.ExecutablePath,$launcher,[StringComparison]::OrdinalIgnoreCase)})
}
function Invoke-Launcher([string]$Arguments){
 $p=Start-Process -FilePath $launcher -ArgumentList $Arguments -WindowStyle Hidden -PassThru -Wait
 if($p.ExitCode -notin 0,3){throw "Launcher failed (exit $($p.ExitCode)); see $Root\logs\startup.log"}
}
function Registration {
 $key=[Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Software\Microsoft\Windows\CurrentVersion\Run')
 if($key){try{[string]$key.GetValue('Wallclock','')}finally{$key.Dispose()}}else{''}
}
function Enable-Startup {Invoke-Launcher ('--install --config '+(Quote $config)+' --output '+(Quote $prefix)+' --log '+(Quote $engineLog))}
function Disable-Startup {if((Registration) -eq ((Quote $launcher)+' --start')){Invoke-Launcher '--uninstall'}}
function Stop-Owned {
 $owned=@(Owned)
 if($owned.Count){$process=Get-Process -Id $owned[0].ProcessId;Invoke-Launcher '--stop';if(-not $process.WaitForExit(20000)){throw 'Supervisor did not stop; leaving files intact'}}
}
function Wallpaper-API {
 if(-not ('WallclockPackageApi' -as [type])){Add-Type -TypeDefinition @"
using System;using System.Text;using System.Runtime.InteropServices;
public static class WallclockPackageApi {
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SystemParametersInfoW")] public static extern bool Get(uint a,uint b,StringBuilder c,uint d);
 [DllImport("user32.dll",CharSet=CharSet.Unicode,EntryPoint="SystemParametersInfoW")] public static extern bool Set(uint a,uint b,string c,uint d);
}
"@}
}
function Selected {
 Wallpaper-API;$value=New-Object Text.StringBuilder 32768
 if(-not [WallclockPackageApi]::Get(0x73,32768,$value,0)){throw 'Cannot read current wallpaper'};$value.ToString()
}
function Is-Generated([string]$Path){$Path.StartsWith($prefix+'-',[StringComparison]::OrdinalIgnoreCase) -and [IO.Path]::GetFileName($Path) -match '^clock-\d+-\d+\.bmp$'}
function Snapshot {
 Directory (Join-Path $Root 'state');$selected=Selected
 if(Is-Generated $selected){return}
 $style=Get-ItemProperty 'HKCU:\Control Panel\Desktop';$restore=$selected
 if($selected -and (Test-Path -LiteralPath $selected -PathType Leaf)){
  $ext=[IO.Path]::GetExtension($selected);if($ext -notin '.jpg','.jpeg','.png','.bmp'){$ext='.jpg'}
  $restore=Join-Path $Root ('state/previous'+$ext);[void](Safe $restore)
  if(-not [string]::Equals($restore,$selected,[StringComparison]::OrdinalIgnoreCase)){Copy-Item -LiteralPath $selected -Destination $restore -Force}
 }
 @{wallpaper=$restore;original=$selected;style=$style.WallpaperStyle;tile=$style.TileWallpaper} | ConvertTo-Json | Set-Content -LiteralPath (Safe (Join-Path $Root 'state/previous.json')) -Encoding UTF8
}
function Restore {
 $state=Join-Path $Root 'state/previous.json'
 if(-not (Test-Path -LiteralPath $state) -or -not (Is-Generated (Selected))){return}
 $saved=Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
 if($saved.wallpaper -and -not (Test-Path -LiteralPath $saved.wallpaper -PathType Leaf)){throw 'Saved wallpaper missing; generated wallpaper retained'}
 Set-ItemProperty 'HKCU:\Control Panel\Desktop' WallpaperStyle $saved.style
 Set-ItemProperty 'HKCU:\Control Panel\Desktop' TileWallpaper $saved.tile
 if(-not [WallclockPackageApi]::Set(0x14,0,$saved.wallpaper,3)){throw 'Wallpaper restore failed; generated files retained'}
}
function Cleanup([switch]$All) {
 $directory=Join-Path $Root 'generated';[void](Safe $directory)
 if(-not (Test-Path -LiteralPath $directory)){return}
 if(@(Owned).Count){throw 'Stop this installation before cleanup'}
 Assert-NoEngine
 $selected=Selected
 $files=@(Get-ChildItem -LiteralPath $directory -File | Where-Object {$_.Name -match '^clock-\d+-\d+\.bmp$'} | Sort-Object LastWriteTimeUtc -Descending)
 $remove=if($All){$files}else{@($files | Select-Object -Skip 2)}
 foreach($file in $remove){
  if(-not [string]::Equals($file.FullName,$selected,[StringComparison]::OrdinalIgnoreCase)){Remove-Item -LiteralPath (Safe $file.FullName)}
 }
 $pending=Join-Path $directory 'clock-pending.bmp';if(Test-Path -LiteralPath $pending){Remove-Item -LiteralPath (Safe $pending)}
}
function Use-Theme([string]$Name){
 $source=Join-Path $Root ('themes/'+$Name+'.ini');[void](Safe $source)
 if(-not (Test-Path -LiteralPath $source -PathType Leaf)){throw "Unknown theme: $Name"}
 $base=[IO.Path]::GetDirectoryName($source);$text=[IO.File]::ReadAllText($source)
 $text=[regex]::Replace($text,'(?m)^(\s*(?:image|background)\s*=\s*)([^;\r\n]+)$',{
  param($match);$path=$match.Groups[2].Value.Trim();if(-not [IO.Path]::IsPathRooted($path)){$path=[IO.Path]::GetFullPath((Join-Path $base $path))};$match.Groups[1].Value+$path
 })
 $temporary=Safe ($config+'.tmp');[IO.File]::WriteAllText($temporary,$text,(New-Object Text.UTF8Encoding $false));Move-Item -LiteralPath $temporary -Destination (Safe $config) -Force
}
try {
 [void](Safe $Root)
 switch($Command){
  'install' {
   $manifestPath=Join-Path $PSScriptRoot 'release.json'
   if(-not (Test-Path -LiteralPath $manifestPath)){throw 'Run install from an extracted release package'}
   $manifest=Get-Content $manifestPath -Raw | ConvertFrom-Json
   # Preflight the entire payload before touching an existing installation.
   foreach($file in $manifest.files){
    [void](Package-Target $file.path);$source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $file.path))
    if((File-Hash $source) -ne $file.sha256){throw "Package checksum failed: $($file.path)"}
   }
   Stop-Owned;Assert-NoEngine
   foreach($name in 'bin','config','themes','backgrounds','generated','logs','state','previews'){Directory (Join-Path $Root $name)}
   foreach($file in $manifest.files){
    if([IO.Path]::IsPathRooted($file.path) -or $file.path -match '(^|[\\/])\.\.([\\/]|$)'){throw 'Invalid release manifest path'}
    $source=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot $file.path));$target=Package-Target $file.path
    if((File-Hash $source) -ne $file.sha256){throw "Package checksum failed: $($file.path)"}
    Directory ([IO.Path]::GetDirectoryName($target))
    if(-not [string]::Equals($source,$target,[StringComparison]::OrdinalIgnoreCase)){Copy-Item -LiteralPath $source -Destination $target -Force}
   }
   if(-not [string]::Equals($manifestPath,(Join-Path $bin 'release.json'),[StringComparison]::OrdinalIgnoreCase)){Copy-Item -LiteralPath $manifestPath -Destination (Safe (Join-Path $bin 'release.json')) -Force}
   foreach($source in Get-ChildItem -LiteralPath (Join-Path $bin 'defaults') -File -Recurse){
    $relative=$source.FullName.Substring((Join-Path $bin 'defaults').Length+1);$target=Safe (Join-Path $Root ('themes/'+$relative))
    Directory ([IO.Path]::GetDirectoryName($target));if(-not (Test-Path -LiteralPath $target)){Copy-Item -LiteralPath $source.FullName -Destination $target}
   }
   if(-not (Test-Path -LiteralPath $config)){Use-Theme 'minimal'}
   Snapshot;if(-not $NoStartup){Enable-Startup}
   Write-Output "Installed $($manifest.version) in $bin; user configuration preserved. Use: & '$bin\wallclock.ps1' run"
  }
  'run' {
   if(@(Owned).Count){Write-Output 'Already running';break}
   Snapshot;Cleanup
   $args='--start --config '+(Quote $config)+' --output '+(Quote $prefix)+' --log '+(Quote $engineLog)
   $p=Start-Process -FilePath $launcher -ArgumentList $args -WindowStyle Hidden -PassThru
   if($p.WaitForExit(1500)){throw "Supervisor exited ($($p.ExitCode)); see logs"}
   Write-Output "Started supervisor PID $($p.Id); engine startup/apply status is recorded in logs"
  }
  'stop' {Stop-Owned;if(-not $KeepWallpaper){Restore};Cleanup;Write-Output 'Stopped'}
  'restart' {Stop-Owned;& $PSCommandPath run -Root $Root}
  'startup' {if($Value -eq 'enable'){Enable-Startup}elseif($Value -eq 'disable'){Disable-Startup}else{throw 'Use startup enable|disable'}}
  'theme' {if(-not $Value){throw 'Use theme minimal|car|sky or a name in themes/ without .ini'};Use-Theme $Value;Write-Output "Selected $Value; valid changes reload automatically"}
  'preview' {
   Directory (Join-Path $Root 'previews');$destination=Safe (Join-Path $Root 'previews/preview.png')
   & (Join-Path $bin 'wallclock.exe') --config $config --render $destination --at $At --size $Size
   if($LASTEXITCODE -ne 0){throw 'Preview failed'};Write-Output $destination
  }
  'cleanup' {Cleanup;Write-Output 'Cleaned old owned files; newest two and current wallpaper retained'}
  'status' {
   $owned=@(Owned);$children=@();if($owned.Count){$children=@(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($owned[0].ProcessId) AND Name='wallclock.exe'")}
   $last=@();if(Test-Path -LiteralPath $engineLog){$last=@(Get-Content -LiteralPath $engineLog -Tail 50 | Where-Object {$_ -match 'event=(applied|retry|fatal|config_reload)'})}
   [pscustomobject]@{installed=Test-Path -LiteralPath $launcher;supervisorPids=@($owned.ProcessId);enginePids=@($children.ProcessId);loginEnabled=(Registration) -eq ((Quote $launcher)+' --start');config=$config;engineLog=$engineLog;lastRecord=if($last.Count){$last[-1]}else{$null}} | ConvertTo-Json -Depth 4
  }
  'uninstall' {
   Stop-Owned;Assert-NoEngine;Disable-Startup;if(-not $KeepWallpaper){Restore};Cleanup -All
   $manifestPath=Join-Path $bin 'release.json'
   if(Test-Path -LiteralPath $manifestPath){
    $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach($file in $manifest.files){[void](Package-Target $file.path)}
    foreach($file in $manifest.files){$target=Package-Target $file.path;if(Test-Path -LiteralPath $target -PathType Leaf){if((File-Hash $target) -eq $file.sha256){Remove-Item -LiteralPath $target}else{Write-Warning "Preserved modified file: $target"}}}
    Remove-Item -LiteralPath (Safe $manifestPath)
   }
   Write-Output 'Removed owned binaries/startup integration. Config, themes, backgrounds, logs, state and previews preserved.'
  }
 }
} catch {Write-Error $_;exit 1}
