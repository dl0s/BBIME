param(
    [string]$BarPath = '',
    [string]$ConnectionPath = 'C:\Users\dove1\AppData\Local\Q10Manager\connection.json'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (!$BarPath) { $BarPath = Join-Path $root 'build\BBIME.bar' }
[xml]$descriptor = Get-Content -LiteralPath (Join-Path $root 'bar-descriptor.xml') -Raw -Encoding UTF8
$id = [string]$descriptor.qnx.id
if ($id -ne 'top.blaccat.BBIME') { throw 'Unexpected package ID' }
$version = [string]$descriptor.qnx.versionNumber + '.' + [string]$descriptor.qnx.buildId
$config = Get-Content -LiteralPath $ConnectionPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.SshUser -ne 'root' -or $config.DeviceHost -ne '192.168.1.61') {
    throw 'This deployment is restricted to the existing Q10.'
}
$options = @('-F', 'none', '-o', 'BatchMode=yes', '-o', 'IdentitiesOnly=yes',
    '-o', 'StrictHostKeyChecking=yes',
    '-o', ('UserKnownHostsFile=' + $config.KnownHostsPath),
    '-o', 'GlobalKnownHostsFile=none', '-o', 'ConnectTimeout=4',
    '-o', 'ServerAliveInterval=5', '-o', 'ServerAliveCountMax=1',
    '-i', $config.SshKeyPath)
if ($config.LegacyAlgorithms) {
    $options += @('-o', 'HostKeyAlgorithms=+ssh-rsa',
        '-o', 'PubkeyAcceptedKeyTypes=+ssh-rsa', '-o', 'MACs=+hmac-sha1')
}
$target = $config.SshUser + '@' + $config.DeviceHost
$ssh = Join-Path $env:WINDIR 'System32\OpenSSH\ssh.exe'
$scp = Join-Path $env:WINDIR 'System32\OpenSSH\scp.exe'
function Invoke-Q10([string]$Command) {
    $result = & $ssh @options -n -T -p ([string]$config.SshPort) $target $Command
    if ($LASTEXITCODE -ne 0) { throw 'Q10 SSH command failed' }
    return $result
}
$hash = (Get-FileHash -LiteralPath $BarPath -Algorithm SHA256).Hash.ToLowerInvariant()
$name = 'BBIME-' + $hash.Substring(0, 12) + '.bar'
$remote = '/tmp/q10deploy/' + $name
$uid = (Invoke-Q10 'ls -nd /proc/$$') -join "`n"
if ($uid -notmatch '^\S+\s+\d+\s+0\s') { throw 'SSH UID 0 not confirmed' }
Invoke-Q10 'test -r /base/scripts/sudtools.sh && mkdir -p /tmp/q10deploy' | Out-Null
& $scp @options -O -P ([string]$config.SshPort) $BarPath ($target + ':' + $remote)
if ($LASTEXITCODE -ne 0) { throw 'BAR upload failed' }
$readback = Join-Path $root ('build\readback-' + $name)
& $scp @options -O -P ([string]$config.SshPort) ($target + ':' + $remote) $readback
if ($LASTEXITCODE -ne 0) { throw 'BAR readback failed' }
if ((Get-FileHash -LiteralPath $readback -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash) {
    throw 'BAR round-trip hash mismatch'
}
Invoke-Q10 ('. /base/scripts/sudtools.sh; sud_install_package_2 ' + $remote)
$job = (Invoke-Q10 ('cat /pps/system/installer/upd/current/job.' + $name)) -join "`n"
Write-Output $job
if ($job -notmatch 'result::success' -or $job -notmatch 'progress::100' -or
    $job -notmatch ('actual_app_version::' + [regex]::Escape($version))) {
    throw 'Installer success/version not confirmed'
}
$match = [regex]::Match($job, 'actual_dname::([^\r\n]+)')
if (!$match.Success -or $match.Groups[1].Value -notmatch
    ('^' + [regex]::Escape($id) + '\.[A-Za-z0-9_]+$')) {
    throw 'Unexpected application directory'
}
$dname = $match.Groups[1].Value
Invoke-Q10 ('ls -ld /apps/' + $dname)
[ordered]@{
    timestamp = [DateTimeOffset]::Now.ToString('o')
    device = $config.DeviceHost
    package = $id
    version = $version
    sha256 = $hash
    hashReadback = 'PASS'
    installation = 'PASS'
    dname = $dname
    job = $job
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'research\q10-app-deployment.json') -Encoding UTF8
Write-Output 'BBIME installed; system input services were not modified.'
