param([switch]$Test, [ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$vswherePath = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) { throw '未找到Visual Studio Installer，请安装Visual Studio的C++桌面开发组件。' }
$buildPath = & $vswherePath -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\Current\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $buildPath) { throw '未找到MSBuild。' }
& $buildPath (Join-Path $projectRoot '即时通信系统.vcxproj') /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { throw '构建失败。' }
if ($Test) {
    & $buildPath (Join-Path $projectRoot 'tests\core_tests.vcxproj') /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /nologo /v:minimal
    if ($LASTEXITCODE -ne 0) { throw '测试构建失败。' }
    Push-Location $projectRoot
    try {
        & (Join-Path $projectRoot "build\tests\$Configuration\core_tests.exe")
        if ($LASTEXITCODE -ne 0) { throw '场景验证失败。' }
    } finally { Pop-Location }
}
