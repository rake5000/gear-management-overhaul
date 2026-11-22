[CmdletBinding()]
param([string]$Zig = 'zig', [switch]$RunTests)
$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
$build = Join-Path $source 'build'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$dll = Join-Path $build 'Rake5000GearManagementOverhaul.dll'
& $Zig cc -shared -target x86_64-windows-gnu -O2 -Wall -Wextra -Werror (Join-Path $source 'gear_management.c') -o $dll
if ($LASTEXITCODE -ne 0) { throw 'Plugin build failed.' }

if ($RunTests) {
    foreach ($test in @('test_copying', 'test_presets', 'test_preset_input')) {
        $exe = Join-Path $build ($test + '.exe')
        $optimization = if ($test -eq 'test_preset_input') { '-O0' } else { '-O2' }
        & $Zig cc -target x86_64-windows-gnu $optimization -Wall -Wextra -Werror (Join-Path $source ($test + '.c')) -o $exe
        if ($LASTEXITCODE -ne 0) { throw "Could not build $test." }
        if ($test -eq 'test_presets') {
            & $exe (Join-Path $build 'preset-test')
        } else {
            & $exe
        }
        if ($LASTEXITCODE -ne 0) { throw "$test failed." }
    }
}

Write-Output "Built $dll"
