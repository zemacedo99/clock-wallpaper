param([Parameter(Mandatory=$true)][string]$Zip)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../scripts/validation-lease.ps1')
$testLease=Get-ValidationLease
try {
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$work=Join-Path $repo ('build/package-test-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
$homeRoot=Join-Path $work 'user home';$extract=Join-Path $work 'extracted'
New-Item -ItemType Directory -Path $work | Out-Null
Expand-Archive -LiteralPath $Zip -DestinationPath $extract
$script:checks=0;$script:invocations=0
function Check([bool]$Condition,[string]$Reason){$script:checks++;if(-not $Condition){throw $Reason}}
function Invoke-Package([string]$Script,[string]$Arguments,[switch]$Fail){
 $script:invocations++;$out=Join-Path $work "$script:invocations.out";$err=Join-Path $work "$script:invocations.err"
 $cmd='-NoProfile -ExecutionPolicy Bypass -File "'+$Script+'" '+$Arguments+' -Root "'+$homeRoot+'"'
 $p=Start-Process "$env:WINDIR/System32/WindowsPowerShell/v1.0/powershell.exe" -ArgumentList $cmd -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput $out -RedirectStandardError $err
 if($Fail){Check ($p.ExitCode -ne 0) 'Expected safe rejection'}else{if($p.ExitCode -ne 0){throw (Get-Content $err -Raw)}}
}
$package=Join-Path $extract 'wallclock.ps1'
Invoke-Package $package 'install -NoStartup'
$installed=Join-Path $homeRoot 'bin/wallclock.ps1'
Check (Test-Path $installed) 'Installed script missing'
Check (Test-Path (Join-Path $homeRoot 'config/active.ini')) 'Default config missing'
$canary=Join-Path $homeRoot 'backgrounds/user-photo.jpg';[IO.File]::WriteAllText($canary,'user-owned sentinel')
$theme=Join-Path $homeRoot 'themes/user.ini';[IO.File]::WriteAllText($theme,'user-owned theme')
$config=Join-Path $homeRoot 'config/active.ini';[IO.File]::AppendAllText($config,"`n; USER CUSTOMIZATION`n")
$hash=(Get-FileHash $config).Hash
Invoke-Package $package 'install -NoStartup'
Check ((Get-FileHash $config).Hash -eq $hash) 'Upgrade changed user configuration'
# Relocate the extracted source package; installed preview must remain independent.
$moved=Join-Path $work 'relocated release';Move-Item -LiteralPath $extract -Destination $moved
Invoke-Package $installed 'preview -At 18:00 -Size 320x180'
Check (Test-Path (Join-Path $homeRoot 'previews/preview.png')) 'Installed preview depended on original package location'
Invoke-Package $installed 'theme car'
Invoke-Package $installed 'preview -At 10:10 -Size 320x180'
Check ((Get-Content $config -Raw).Contains((Join-Path $homeRoot 'themes/sprites/car.png'))) 'Sprite path not rebased to installed themes'
Invoke-Package $installed 'theme sky'
Invoke-Package $installed 'preview -At 22:00 -Size 180x320'
Check (Test-Path (Join-Path $homeRoot 'previews/preview.png')) 'Sky portrait preview failed'
$generated=Join-Path $homeRoot 'generated'
foreach($n in 1..5){[IO.File]::WriteAllText((Join-Path $generated "clock-123-$n.bmp"),'owned test file');Start-Sleep -Milliseconds 20}
[IO.File]::WriteAllText((Join-Path $generated 'my-background.bmp'),'do not delete')
Invoke-Package $installed 'cleanup'
Check (@(Get-ChildItem $generated -Filter 'clock-*.bmp').Count -eq 2) 'Cleanup did not retain exactly two owned files'
Check (Test-Path (Join-Path $generated 'my-background.bmp')) 'Cleanup deleted an unrelated file'
# A corrupt/tampered manifest must never reach user themes through ../ traversal.
$manifestPath=Join-Path $homeRoot 'bin/release.json';$original=Get-Content $manifestPath -Raw
$manifest=$original | ConvertFrom-Json;$manifest.files+=@{path='..\themes\user.ini';sha256=(Get-FileHash $theme).Hash}
$manifest | ConvertTo-Json -Depth 5 | Set-Content $manifestPath -Encoding UTF8
Invoke-Package $installed 'uninstall' -Fail
Check (Test-Path $theme) 'Manifest traversal deleted user theme'
[IO.File]::WriteAllText($manifestPath,$original)
Invoke-Package $installed 'uninstall'
Check (-not (Test-Path (Join-Path $homeRoot 'bin/wallclock.exe'))) 'Owned binary was not uninstalled'
Check ((Get-Content $canary -Raw) -eq 'user-owned sentinel') 'Uninstall deleted user photo'
Check ((Get-Content $theme -Raw) -eq 'user-owned theme') 'Uninstall deleted user theme'
Check (Test-Path $config) 'Uninstall deleted user config'
Check (Test-Path (Join-Path $generated 'my-background.bmp')) 'Uninstall deleted unrelated generated-folder file'
Check (@(Get-ChildItem $generated -Filter 'clock-*.bmp').Count -eq 0) 'Uninstall left old owned generated files'
@{checks=$checks;invocations=$invocations;status='passed'} | ConvertTo-Json | Set-Content (Join-Path $work 'results.json')
Write-Output "Passed $checks package checks through Windows PowerShell 5.1; evidence=$work"
} finally {$testLease.Dispose()}
