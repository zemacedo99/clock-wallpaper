param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
 if(-not $SkipBuild){& ./scripts/build.ps1}
 . (Join-Path $PSScriptRoot 'validation-lease.ps1')
 $packageLease=Get-ValidationLease
 $version='0.1.0-rc1'
 $stage=Join-Path (Get-Location) ('build/package-'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
 New-Item -ItemType Directory -Path $stage,(Join-Path $stage 'defaults'),(Join-Path $stage 'defaults/sprites'),(Join-Path $stage 'docs') | Out-Null
 Copy-Item build/wallclock.exe,build/wallclock_startup.exe,packaging/wallclock.ps1 -Destination $stage
 Copy-Item examples/minimal.ini,examples/car.ini,examples/sky.ini,examples/ATTRIBUTION.md -Destination (Join-Path $stage 'defaults')
 Copy-Item examples/sprites/car.png -Destination (Join-Path $stage 'defaults/sprites')
 Copy-Item docs/themes.md -Destination (Join-Path $stage 'docs')
 Copy-Item packaging/README.md -Destination (Join-Path $stage 'README.md')
 Copy-Item third_party/README.md -Destination (Join-Path $stage 'docs/codecs.md')
 Copy-Item packaging/STB-LICENSE.txt -Destination (Join-Path $stage 'docs/STB-LICENSE.txt')
 $files=@(Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {@{path=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
 @{version=$version;architecture='windows-x86';files=$files} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $stage 'release.json') -Encoding UTF8
 $release=Join-Path (Get-Location) 'build/releases';New-Item -ItemType Directory -Force $release | Out-Null
 $zip=Join-Path $release ('wallclock-'+$version+'-windows-x86.zip')
 Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
 (Get-FileHash $zip).Hash+'  '+[IO.Path]::GetFileName($zip) | Set-Content ($zip+'.sha256')
 Write-Output "Stage: $stage"
 Write-Output "Release: $zip"
} finally {if($packageLease){$packageLease.Dispose()};Pop-Location}
