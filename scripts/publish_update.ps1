param([string]$ServerConfig, [string]$ReleaseDirectory, [string]$Python)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$ReleaseDirectory) {
    $version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
    $ReleaseDirectory = Join-Path $root "dist/v$version"
}
if (!$ServerConfig) {
    $profiles = @(Get-ChildItem -LiteralPath (Join-Path $root 'lan-profiles') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'server/server.json' } | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf })
    if ($profiles.Count -eq 1) { $ServerConfig = $profiles[0] }
    else { $ServerConfig = Read-Host 'Enter the existing TLS server.json path' }
}
if (!$ServerConfig) { throw 'An existing TLS server configuration is required.' }
if (!$Python) {
    $bundled = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (Test-Path -LiteralPath $bundled) { $Python = $bundled }
    else { $Python = (Get-Command python -ErrorAction Stop).Source }
}
& $Python (Join-Path $PSScriptRoot 'publish_client_update.py') --release-directory $ReleaseDirectory --server-config $ServerConfig
if ($LASTEXITCODE -ne 0) { throw 'Publication failed; the current update feed was retained.' }
Write-Host 'Published ordinary-client update. Running Tinglan 5.5+ TLS servers load it without restart.'
