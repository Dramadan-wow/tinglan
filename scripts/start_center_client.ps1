param([string]$ClientConfig)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
$exe = Join-Path $root "build-package/v$version/runtime/im_desktop.exe"
if (!$ClientConfig) {
    $profiles = @(Get-ChildItem -LiteralPath (Join-Path $root 'lan-profiles') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'client/client.json' } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if ($profiles.Count -eq 1) { $ClientConfig = $profiles[0] }
    else { $ClientConfig = Read-Host 'Enter the existing central client.json path' }
}
if (!$ClientConfig) { throw 'A central client profile is required.' }
$ClientConfig = (Resolve-Path -LiteralPath $ClientConfig).Path
$settings = Get-Content -LiteralPath $ClientConfig -Raw -Encoding UTF8 | ConvertFrom-Json
if (!$settings.tls -or $settings.autoStartLocalServer -or $settings.allowInsecureLoopback) { throw 'A central client requires verified TLS and no local startup.' }
if (!(Test-Path -LiteralPath $exe)) { throw 'Package the current release first.' }
& $exe --client-config $ClientConfig
