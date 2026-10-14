param(
    [Parameter(Mandatory=$true)][string]$LocalAddress,
    [Parameter(Mandatory=$true)][string]$FriendAddress,
    [Parameter(Mandatory=$true)][string]$InterfaceAlias,
    [switch]$Apply,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$name = 'Tinglan-Trusted-LAN-56110'
if ($Remove) { Remove-NetFirewallRule -Name $name -ErrorAction Stop; return }
$localIp = [System.Net.IPAddress]::Parse($LocalAddress)
$friendIp = [System.Net.IPAddress]::Parse($FriendAddress)
if ($localIp.AddressFamily -ne 'InterNetwork' -or $friendIp.AddressFamily -ne 'InterNetwork' -or
    $LocalAddress -eq '0.0.0.0' -or $FriendAddress -eq '0.0.0.0' -or $FriendAddress -eq '255.255.255.255') { throw 'Use two individual IPv4 addresses.' }
$adapter = Get-NetIPAddress -AddressFamily IPv4 -InterfaceAlias $InterfaceAlias | Where-Object IPAddress -eq $LocalAddress
if (!$adapter) { throw 'Local address does not belong to this interface.' }
$profile = Get-NetConnectionProfile -InterfaceAlias $InterfaceAlias
if ($profile.NetworkCategory -ne 'Private') { throw 'This script only permits a trusted Private network. Do not change a public/campus Wi-Fi to Private just to pass this check.' }
$params = @{
    Name=$name; DisplayName='Tinglan: trusted friend only (TCP 56110)'; Direction='Inbound'; Action='Allow';
    Protocol='TCP'; LocalPort=56110; LocalAddress=$LocalAddress; RemoteAddress=$FriendAddress;
    InterfaceAlias=$InterfaceAlias; Profile='Private'; EdgeTraversalPolicy='Block'
}
if (!$Apply) { $params | Format-Table; Write-Host 'Preview only. Run in an elevated PowerShell with -Apply to create this exact rule.'; return }
if (Get-NetFirewallRule -Name $name -ErrorAction SilentlyContinue) { throw 'Rule exists. Review/remove the old Tinglan rule before applying a different address.' }
New-NetFirewallRule @params
