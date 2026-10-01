param(
    [string]$BarPath = '',
    [string]$ConnectionPath = '',
    [switch]$LegacyAlgorithms
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $ConnectionPath) {
    $ConnectionPath = if ($env:Q10DEPLOY_CONFIG) { $env:Q10DEPLOY_CONFIG } else {
        Join-Path $env:LOCALAPPDATA 'Q10Deploy\config.json'
    }
}
if (!$BarPath) { $BarPath = Join-Path $root 'build\BBIME.bar' }
[xml]$descriptor = Get-Content -LiteralPath (Join-Path $root 'bar-descriptor.xml') -Raw -Encoding UTF8
$id = [string]$descriptor.qnx.id
if ($id -ne 'top.blaccat.BBIME') { throw 'Unexpected package ID' }
$version = [string]$descriptor.qnx.versionNumber + '.' + [string]$descriptor.qnx.buildId
if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Unexpected BBIME version' }
$evidenceDirectory = Join-Path $root ('build\bbime-device-' + $version)
New-Item -ItemType Directory -Force -Path $evidenceDirectory | Out-Null
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
if ($LegacyAlgorithms -or $config.LegacyAlgorithms) {
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
$processes = @(Invoke-Q10 'pidin ar')
if (@($processes | Where-Object { $_ -match '(?i)(?:/app/native/|/native/|\s)bbime(?:\s|$)' }).Count) {
    throw 'BBIME is running. Exit it normally before updating; no process will be stopped by this script.'
}
$existing = @(Invoke-Q10 ('ls -d /apps/' + $id + '.* 2>/dev/null || true'))
$backupFiles = @()
foreach ($directory in $existing) {
    $existingMatch = [regex]::Match($directory, '^/apps/(top\.blaccat\.BBIME\.[A-Za-z0-9_]+)$')
    if (-not $existingMatch.Success) { throw 'Unexpected existing BBIME directory' }
    $existingDname = $existingMatch.Groups[1].Value
    $remoteBackup = '/tmp/' + $existingDname + '-before-' + $version + '-' + $hash.Substring(0,12) + '.tar'
    $dataRoot = '/accounts/1000/appdata/' + $existingDname
    $backupState = (Invoke-Q10 ('if test -d ' + $dataRoot + '/data; then cd ' + $dataRoot +
        ' && tar -cf ' + $remoteBackup + ' data && echo BBIME_BACKUP_CREATED; else echo BBIME_NO_EXISTING_DATA; fi')) -join "`n"
    if ($backupState -match 'BBIME_BACKUP_CREATED') {
        $localBackup = Join-Path $evidenceDirectory ($existingDname + '-data-before-update.tar')
        & $scp @options -O -P ([string]$config.SshPort) ($target + ':' + $remoteBackup) $localBackup
        if ($LASTEXITCODE -ne 0) { throw 'BBIME private-data backup download failed' }
        $backupFiles += [ordered]@{ path = $localBackup;
            sha256 = (Get-FileHash -LiteralPath $localBackup -Algorithm SHA256).Hash }
    } elseif ($backupState -notmatch 'BBIME_NO_EXISTING_DATA') { throw 'BBIME backup state not confirmed' }
}
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
$installation = [ordered]@{
    timestamp = [DateTimeOffset]::Now.ToString('o')
    device = $config.DeviceHost
    package = $id
    version = $version
    sha256 = $hash
    hashReadback = 'PASS'
    installation = 'PASS'
    runningApplicationGuard = 'PASS_NOT_RUNNING'
    uninstallPerformed = $false
    privateDataBackups = $backupFiles
    dname = $dname
    job = $job
}
$installation | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $evidenceDirectory 'installation.json') -Encoding UTF8
$installation | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath (Join-Path $root ('research\q10-app-deployment-' + $version + '.json')) -Encoding UTF8
Write-Output 'BBIME installed; system input services were not modified.'
