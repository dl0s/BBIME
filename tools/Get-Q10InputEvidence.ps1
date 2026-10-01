param(
    [string]$ConnectionPath = 'C:\Users\dove1\AppData\Local\Q10Manager\connection.json'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$config = Get-Content -LiteralPath $ConnectionPath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($config.SshUser -ne 'root' -or $config.DeviceHost -ne '192.168.1.61') {
    throw 'This audit is restricted to the existing Q10 root SSH target.'
}
foreach ($field in @('SshKeyPath', 'KnownHostsPath')) {
    if (-not (Test-Path -LiteralPath $config.$field -PathType Leaf)) {
        throw "Missing existing SSH configuration file: $field"
    }
}
$options = @('-F', 'none', '-n', '-T',
    '-o', 'BatchMode=yes', '-o', 'IdentitiesOnly=yes',
    '-o', 'StrictHostKeyChecking=yes',
    '-o', ('UserKnownHostsFile=' + $config.KnownHostsPath),
    '-o', 'GlobalKnownHostsFile=none',
    '-o', 'ConnectTimeout=4', '-o', 'ServerAliveInterval=5',
    '-o', 'ServerAliveCountMax=1')
if ($config.LegacyAlgorithms) {
    $options += @('-o', 'HostKeyAlgorithms=+ssh-rsa',
        '-o', 'PubkeyAcceptedKeyTypes=+ssh-rsa', '-o', 'MACs=+hmac-sha1')
}
$options += @('-i', $config.SshKeyPath, '-p', [string]$config.SshPort)
$remote = @'
echo ---identity---
uname -a
ls -nd /proc/$$
cat /etc/os.version
echo ---input-processes---
pidin ar | grep -E 'keyboard-imf|input_service|sys.keyboard|/base/sbin/screen'
echo ---screen-endpoint-access---
for f in /dev/screen/.provider /dev/screen/.inject /dev/screen/.winmgr /dev/screen/.inmgr; do
    ls -ln $f
    if test -r $f; then echo readable:$f; fi
    if test -w $f; then echo writable:$f; fi
done
echo ---pps-metadata-only---
ls -ln /pps/services/input/ /pps/system/keyboard/
echo ---imf-libraries---
ls /lib/libimf* /lib/libinput_method* /lib/libfluency_input_method* 2>/dev/null
echo ---system-keyboard-actions---
grep -nE '^Entry-Point-(System|User)-Actions:' /accounts/1000/appdata/sys.keyboard/app/META-INF/MANIFEST.MF
echo ---static-pinyin-resources---
ls -l /base/usr/share/imf/data/swiftkey/zh_CN/pinyin/charactermap.json
ls -l /accounts/1000/appdata/sys.keyboard/app/native/data/keymaps/chinese_pinyin/pinyin.xml
ls -l /accounts/1000/appdata/sys.keyboard/app/native/data/predictionbar/pkb/res/720x720/chinese/portrait.xml
echo ---audit-complete---
'@
$ssh = Join-Path $env:WINDIR 'System32\OpenSSH\ssh.exe'
$lines = & $ssh @options ($config.SshUser + '@' + $config.DeviceHost) $remote
if ($LASTEXITCODE -ne 0) { throw 'Read-only SSH audit failed' }
$text = $lines -join "`n"
if ($text -notmatch '---audit-complete---' -or
    $text -notmatch '(?m)^\S+\s+\d+\s+0\s+0\s+.*?/proc/\d+') {
    throw 'Audit completion or shell UID 0 evidence missing'
}
foreach ($name in @('provider', 'inject', 'winmgr', 'inmgr')) {
    if ($text -notmatch [regex]::Escape("writable:/dev/screen/.$name")) {
        throw "Screen endpoint access not confirmed: $name"
    }
}
$evidence = [ordered]@{
    auditDate = '2026-10-01'
    timezone = 'Asia/Shanghai'
    host = $config.DeviceHost
    user = $config.SshUser
    hostKeyChecking = 'STRICT_EXISTING_PIN'
    result = 'READ_ONLY_METADATA_PASS'
    inputInjection = 'NOT_PERFORMED'
    deviceModification = 'NOT_PERFORMED'
    output = $lines
}
New-Item -ItemType Directory -Force -Path (Join-Path $root 'research') | Out-Null
$evidence | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $root 'research\device-input-evidence.json') -Encoding UTF8
Write-Output $text
