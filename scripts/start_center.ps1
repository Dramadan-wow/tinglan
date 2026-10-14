param([string]$ServerConfig)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
$exe = Join-Path $root "build-package/v$version/runtime/im_server.exe"
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Build/package the current release first.' }
if (!$ServerConfig) {
    $profiles = @(Get-ChildItem -LiteralPath (Join-Path $root 'lan-profiles') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'server/server.json' } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if ($profiles.Count -eq 1) { $ServerConfig = $profiles[0] }
    else { $ServerConfig = Read-Host 'Enter existing LAN/cloud server.json path (no automatic address detection)' }
}
if (!$ServerConfig) { throw 'An existing server profile is required. Configure LAN first.' }
$ServerConfig = (Resolve-Path -LiteralPath $ServerConfig).Path
$settings = Get-Content -LiteralPath $ServerConfig -Raw -Encoding UTF8 | ConvertFrom-Json
if (!$settings.tls -or $settings.allowInsecureLoopback) { throw 'Central server requires verified TLS configuration.' }
if ($settings.idleExitMs -ne 0) { throw 'Use an independent server profile with idleExitMs=0.' }
$db = $settings.workspaceFile
if (![IO.Path]::IsPathRooted($db)) { $db = Join-Path (Split-Path $ServerConfig -Parent) $db }
if (!(Test-Path -LiteralPath $db -PathType Leaf)) { throw 'Existing database missing; refusing to create another database.' }
if (Get-NetTCPConnection -State Listen -LocalPort $settings.port -ErrorAction SilentlyContinue) { throw 'Port in use. Stop the previous server gracefully before starting this version.' }
Write-Host "Starting Tinglan $version with existing profile: $ServerConfig"
Write-Host 'Account/database paths and TLS certificates are retained. Stop gracefully with Ctrl+C.'
& $exe --config $ServerConfig
if ($LASTEXITCODE -ne 0) { throw "Server exited: $LASTEXITCODE" }
