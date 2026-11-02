param([ValidateRange(1,1440)][int]$Minutes=3,[string]$Root=(Join-Path $env:LOCALAPPDATA 'Wallclock'))
# Bounded diagnostic only: no WMI, image hashing, capture, builds or child shells.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validation-lease.ps1')
$observerLease=Get-ValidationLease
try {
Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;
public static class QuietProbe {
 public sealed class Row { public string name; public int pid,handles; public double cpuMs; public long resident,priv; public bool ioValid; public ulong readBytes,writeBytes; }
 public static int processCount;
 public static Row[] Sample(int self) {
  var rows=new System.Collections.Generic.List<Row>();
  var processes=System.Diagnostics.Process.GetProcesses();processCount=processes.Length;
  foreach(var p in processes) using(p) {
   try {
    var name=p.ProcessName;
    if(p.Id!=self && name!="wallclock" && name!="wallclock_startup")continue;
    IO io;bool ok=GetProcessIoCounters(p.Handle,out io);
    rows.Add(new Row{name=name,pid=p.Id,handles=p.HandleCount,cpuMs=p.TotalProcessorTime.TotalMilliseconds,resident=p.WorkingSet64,priv=p.PrivateMemorySize64,ioValid=ok,readBytes=io.rb,writeBytes=io.wb});
   } catch(System.InvalidOperationException) {} catch(System.ComponentModel.Win32Exception) {}
  }
  return rows.ToArray();
 }
 [StructLayout(LayoutKind.Sequential)] public struct IO {public ulong ro,wo,oo,rb,wb,ob;}
 [DllImport("kernel32.dll")] public static extern bool GetProcessIoCounters(IntPtr p,out IO io);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool SystemParametersInfoW(uint a,uint b,StringBuilder c,uint d);
}
'@
$folder=Join-Path $PSScriptRoot ('../build/controlled-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory $folder | Out-Null
$timer=[Diagnostics.Stopwatch]::StartNew()
Write-Output "Observing for $Minutes minutes: $folder"
do {
 $rows=[QuietProbe]::Sample($PID)
 $selected=New-Object Text.StringBuilder 32768
 if(-not [QuietProbe]::SystemParametersInfoW(0x73,32768,$selected,0)){throw 'Wallpaper query failed'}
 $files=@(Get-ChildItem (Join-Path $Root 'generated') -File)
 @{utc=[DateTimeOffset]::UtcNow.ToString('o');seconds=$timer.Elapsed.TotalSeconds;processCount=[QuietProbe]::processCount;processes=$rows;selected=$selected.ToString();cacheCount=$files.Count;cacheBytes=($files|Measure-Object Length -Sum).Sum;logBytes=(Get-ChildItem (Join-Path $Root 'logs') -File|Measure-Object Length -Sum).Sum} | ConvertTo-Json -Depth 4 -Compress | Add-Content (Join-Path $folder 'samples.jsonl')
 if($timer.Elapsed.TotalMinutes -ge $Minutes){break}
 Start-Sleep -Seconds 20
} while($true)
Copy-Item (Join-Path $Root 'logs/*.log') $folder
Write-Output "Completed: $folder"
} finally {$observerLease.Dispose()}

