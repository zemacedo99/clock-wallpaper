# Serialize expensive validation with measurement. The OS releases the handle on
# exit/crash/reboot; the empty file is deliberately retained (no stale-lock race).
function Get-ValidationLease {
 $path=Join-Path $PSScriptRoot '../build/validation.lock'
 New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
 try { [IO.File]::Open($path,[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None) }
 catch {throw 'Another build, package test or recorder owns the validation slot. Wait for it to finish; do not run them together.'}
}
