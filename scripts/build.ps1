$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'validation-lease.ps1')
$buildLease=Get-ValidationLease
Push-Location (Join-Path $PSScriptRoot '..')
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $shared = @('src/core.cpp', 'src/config.cpp', 'src/renderer.cpp', 'src/codecs.cpp', 'src/bridge.cpp', 'src/scheduler.cpp')
    & g++ -std=gnu++14 -O2 -DNDEBUG -static-libgcc -static-libstdc++ -Iinclude -Ithird_party @shared src/main.cpp src/platform/windows.cpp -luser32 -ladvapi32 -o build/wallclock.exe
    if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed' }
    & g++ -std=gnu++14 -O2 -Iinclude -Ithird_party @shared tests/tests.cpp -o build/wallclock_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & ./build/wallclock_tests.exe examples/minimal.ini
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    & g++ -std=gnu++14 -O2 -Iinclude tests/windows_events.cpp src/platform/windows.cpp -luser32 -ladvapi32 -o build/wallclock_events_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native event test compilation failed' }
    & ./build/wallclock_events_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Native event tests failed' }
    & g++ -std=gnu++14 -O2 -static-libgcc -static-libstdc++ -mwindows -Iinclude src/windows_supervisor.cpp -luser32 -ladvapi32 -lshell32 -o build/wallclock_startup.exe
    if ($LASTEXITCODE -ne 0) { throw 'Supervisor compilation failed' }
    & g++ -std=gnu++14 -O2 -Iinclude tests/windows_supervision.cpp -lshell32 -o build/wallclock_supervision_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Supervisor test compilation failed' }
    & ./build/wallclock_supervision_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Supervisor tests failed' }
} finally { Pop-Location; $buildLease.Dispose() }
