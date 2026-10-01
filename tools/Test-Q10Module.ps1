param(
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
$config = Get-Content -LiteralPath $ConnectionPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.DeviceHost -ne '192.168.1.61' -or $config.SshUser -ne 'root') {
    throw 'Synthetic validation is restricted to the configured Q10.'
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
$ssh = Join-Path $env:WINDIR 'System32\OpenSSH\ssh.exe'
$scp = Join-Path $env:WINDIR 'System32\OpenSSH\scp.exe'
$target = $config.SshUser + '@' + $config.DeviceHost
function Invoke-Synthetic([string]$Command) {
    $ErrorActionPreference = 'Continue'
    $output = @(& $ssh @options -n -T -p ([string]$config.SshPort) $target $Command 2>&1)
    return @{ output = @($output | ForEach-Object { $_.ToString() }); exitCode = $LASTEXITCODE }
}
$remote = '/tmp/bbime-validation/' + [guid]::NewGuid().ToString('N')
if ($remote -notmatch '^/tmp/bbime-validation/[a-f0-9]{32}$') { throw 'Invalid scratch path' }
$record = [ordered]@{
    timestamp = [DateTimeOffset]::Now.ToOffset([TimeSpan]::FromHours(8)).ToString('o')
    sourceVersion = '0.1.0.15'
    scope = 'ARM32_SYNTHETIC_CORE_ONLY_SSH_ROOT_NOT_APP_PERMISSIONS_OR_UI'
    scratchDirectory = $remote
    status = 'PENDING'
    checks = @()
    artifactSha256 = [ordered]@{}
    inputData = 'SYNTHETIC_ONLY_NO_REAL_DOCUMENT_OR_USER_DICTIONARY'
}
try {
    $files = @('build/arm/decoder_test', 'build/arm/inputmodule_test', 'assets/dict_pinyin.dat')
    foreach ($file in $files) {
        if (!(Test-Path -LiteralPath (Join-Path $root $file))) {
            throw 'Build ARM tests first with build.ps1 -Tests.'
        }
        $record.artifactSha256[$file] =
            (Get-FileHash -LiteralPath (Join-Path $root $file) -Algorithm SHA256).Hash
    }
    $prepare = Invoke-Synthetic "mkdir -p $remote"
    if ($prepare.exitCode -ne 0) { throw 'Q10 connection/scratch creation failed' }
    foreach ($file in $files) {
        & $scp @options -O -P ([string]$config.SshPort) (Join-Path $root $file) ($target + ':' + $remote + '/')
        if ($LASTEXITCODE -ne 0) { throw "Synthetic artifact upload failed: $file" }
    }
    $prepare = Invoke-Synthetic "chmod 700 $remote/decoder_test $remote/inputmodule_test"
    if ($prepare.exitCode -ne 0) { throw 'Synthetic executable preparation failed' }
    foreach ($test in @('decoder_test', 'inputmodule_test')) {
        $result = Invoke-Synthetic "$remote/$test $remote/dict_pinyin.dat $remote/$test-user.dat"
        $output = $result.output
        $exitCode = $result.exitCode
        $pass = $exitCode -eq 0 -and ($output -join "`n") -match 'PASS:'
        $denied = $exitCode -ne 0 -and
            ($output -join "`n") -match '(?m)^sh: .*: Operation not permitted$'
        $record.checks += [ordered]@{
            name = $test
            status = $(if ($pass) { 'PASS' } elseif ($denied) {
                'NOT_RUN_EXECUTION_DENIED'
            } else { 'FAIL' })
            exitCode = $exitCode
            output = $output
        }
        $output | Write-Output
        if ($denied) {
            $record.status = 'EXECUTION_DENIED'
            throw 'Q10 refuses standalone test execution; packaged app validation is required.'
        }
        if (!$pass) { throw "ARM synthetic regression failed: $test" }
    }
    $record.status = 'PASS'
} catch {
    if ($record.status -ne 'EXECUTION_DENIED') { $record.status = 'FAILED_OR_UNAVAILABLE' }
    $record.error = $_.Exception.Message
    throw
} finally {
    $record | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath (Join-Path $root 'research/native-module-arm-validation-0.1.0.15.json') -Encoding UTF8
}
