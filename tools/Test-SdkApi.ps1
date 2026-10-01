param(
    [string]$SdkRoot = 'C:\Users\dove1\Documents\BBarmin\sdk'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$hostRoot = Join-Path $SdkRoot 'host_10_3_1_12\win32\x86'
$targetRoot = Join-Path $SdkRoot 'target_10_3_1_995\qnx6'
$qcc = Join-Path $hostRoot 'usr\bin\qcc.exe'
$readelf = Join-Path $hostRoot 'usr\bin\ntoarm-readelf.exe'
$source = Join-Path $root 'research\api_link_probe.c'
$build = Join-Path $root 'research\build'
$output = Join-Path $build 'api_link_probe'
foreach ($path in @($qcc, $readelf, $source)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required file missing: $path"
    }
}
New-Item -ItemType Directory -Force -Path $build | Out-Null
$savedHost = $env:QNX_HOST
$savedTarget = $env:QNX_TARGET
$savedPath = $env:PATH
try {
    $env:QNX_HOST = $hostRoot
    $env:QNX_TARGET = $targetRoot
    $env:PATH = (Join-Path $hostRoot 'usr\bin') + ';' + $savedPath
    & $qcc '-V4.8.3,gcc_ntoarmv7le' -Wall -Wextra -Werror `
        -o $output $source -lscreen -lsqlite3 -lbps
    if ($LASTEXITCODE -ne 0) { throw 'ARM API link probe failed' }
    $elf = & $readelf -W -h -d -s $output
    if ($LASTEXITCODE -ne 0) { throw 'ELF inspection failed' }
    $text = $elf -join "`n"
    if ($text -notmatch 'Machine:\s+ARM') { throw 'Not an ARM ELF' }
    foreach ($symbol in @('screen_create_context', 'screen_create_session_type',
        'screen_inject_event', 'screen_send_event', 'sqlite3_libversion')) {
        if ($text -notmatch [regex]::Escape($symbol)) {
            throw "Required linked symbol missing: $symbol"
        }
    }
    $evidence = [ordered]@{
        auditDate = '2026-10-01'
        timezone = 'Asia/Shanghai'
        sdkRoot = $SdkRoot
        compiler = '4.8.3,gcc_ntoarmv7le'
        result = 'COMPILE_AND_LINK_PASS'
        deviceExecution = 'NOT_PERFORMED'
        sourceSha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        binarySha256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
        elf = $elf
    }
    $evidence | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath (Join-Path $root 'research\sdk-api-evidence.json') -Encoding UTF8
    Write-Output 'PASS: ARM compile/link and symbol checks. Not deployed or executed.'
} finally {
    $env:PATH = $savedPath
    if ($null -eq $savedHost) {
        Remove-Item Env:QNX_HOST -ErrorAction SilentlyContinue
    } else { $env:QNX_HOST = $savedHost }
    if ($null -eq $savedTarget) {
        Remove-Item Env:QNX_TARGET -ErrorAction SilentlyContinue
    } else { $env:QNX_TARGET = $savedTarget }
}
