param([switch]$Test)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$qtRoot = Join-Path $projectRoot '.tools\Qt\6.8.3\msvc2022_64'
$cmakeExe = Join-Path $projectRoot '.tools\python\cmake\data\bin\cmake.exe'
if (!(Test-Path -LiteralPath $cmakeExe) -or !(Test-Path -LiteralPath (Join-Path $qtRoot 'bin\qmake.exe'))) {
    throw 'Project Qt SDK or CMake is missing. See docs/Qt-environment.md.'
}
Push-Location -LiteralPath $projectRoot
try {
    & $cmakeExe --preset qt-desktop
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmakeExe --build --preset qt-release --parallel 4 -- /nodeReuse:false
    if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'build-gui\Release\im_system.exe') -Destination (Join-Path $projectRoot 'bin\im_system.exe') -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'build-gui\server\Release\im_server.exe') -Destination (Join-Path $projectRoot 'bin\im_server.exe') -Force
    & (Join-Path $qtRoot 'bin\windeployqt.exe') --release --no-translations --no-opengl-sw (Join-Path $projectRoot 'bin\im_desktop.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
    & (Join-Path $qtRoot 'bin\windeployqt.exe') --release --no-translations (Join-Path $projectRoot 'bin\im_server.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Server Qt/SQLite deployment failed.' }
    & (Join-Path (Split-Path $cmakeExe) 'ctest.exe') --test-dir build-gui -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed.' }
    if ($Test) {
        $testDir = Join-Path $projectRoot ('tests\gui-output\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
        New-Item -ItemType Directory -Path $testDir | Out-Null
        Start-Process -FilePath (Join-Path $projectRoot 'bin\im_desktop.exe') -ArgumentList @('--self-test', ('"' + $testDir + '"')) -Wait -WindowStyle Hidden
        $report = Get-Content -LiteralPath (Join-Path $testDir 'results.json') -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($report.status -ne 'passed') { throw $report.error }
        Write-Host ('GUI checks passed: ' + $report.count)
    }
    Write-Host 'Ready: bin/im_desktop.exe'
} finally { Pop-Location }
