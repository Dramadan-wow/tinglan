param([string]$ServerAddress, [string]$ListenAddress, [string]$Python)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (!$ServerAddress) { $ServerAddress = Read-Host 'Enter computer A IPv4 or DNS name (no automatic detection)' }
if (!$ServerAddress) { throw 'Server address is required.' }
if (!$Python) {
    $candidate = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
    if (Test-Path -LiteralPath $candidate) { $Python = $candidate }
    else { $Python = (Get-Command python -ErrorAction Stop).Source }
}
& $Python -c 'import cryptography'
if ($LASTEXITCODE -ne 0) { throw 'Install Python cryptography first: python -m pip install cryptography' }
$arguments = @((Join-Path $PSScriptRoot 'prepare_lan.py'), '--host', $ServerAddress)
if ($ListenAddress) { $arguments += @('--listen', $ListenAddress) }
$env:PYTHONIOENCODING = 'utf-8'
& $Python @arguments
if ($LASTEXITCODE -ne 0) { throw 'LAN configuration failed. Original configuration and database were not replaced.' }
Write-Host 'Open the generated profile. Start-Server.cmd is for A; send only Tinglan-LAN-Client.zip to B.'
