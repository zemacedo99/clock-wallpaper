$ErrorActionPreference = 'Stop'
Push-Location (Join-Path $PSScriptRoot '..')
try {
    New-Item -ItemType Directory -Force build | Out-Null
    $shared = @('src/core.cpp', 'src/config.cpp', 'src/renderer.cpp', 'src/codecs.cpp', 'src/bridge.cpp')
    & g++ -std=gnu++14 -O2 -DNDEBUG -static-libgcc -static-libstdc++ -Iinclude -Ithird_party @shared src/main.cpp src/platform/windows.cpp -luser32 -o build/wallclock.exe
    if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed' }
    & g++ -std=gnu++14 -O2 -Iinclude -Ithird_party @shared tests/tests.cpp -o build/wallclock_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & ./build/wallclock_tests.exe examples/minimal.ini
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
