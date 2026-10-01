param(
    [string]$BarPath = '',
    [string]$OutputPath = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $BarPath) { $BarPath = Join-Path $root 'build/BBIME.bar' }
if (-not [IO.Path]::IsPathRooted($BarPath)) { $BarPath = Join-Path $root $BarPath }
[xml]$descriptor = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'bar-descriptor.xml')
$version = [string]$descriptor.qnx.versionNumber + '.' + [string]$descriptor.qnx.buildId
if ($descriptor.qnx.id -ne 'top.blaccat.BBIME') { throw 'Unexpected package ID' }
if (-not $OutputPath) { $OutputPath = Join-Path $root "research/bbime-app-package-$version.json" }
if (-not [IO.Path]::IsPathRooted($OutputPath)) { $OutputPath = Join-Path $root $OutputPath }
$record = [ordered]@{
    timestamp = [DateTimeOffset]::Now.ToOffset([TimeSpan]::FromHours(8)).ToString('o')
    version = $version
    scope = 'LOCAL_BBIME_BAR_BYTES_ONLY_NOT_DEVICE_VALIDATION'
    barSha256 = (Get-FileHash -LiteralPath $BarPath -Algorithm SHA256).Hash
    customSymQmlPackaged = $false
    appQmlByteMatch = $false
    status = 'RUNNING'
    checkedAssets = @()
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($BarPath)
try {
    $duplicates = @($archive.Entries | Group-Object FullName | Where-Object Count -gt 1)
    if ($duplicates.Count) { throw 'Duplicate BAR archive entry' }
    $manifestEntry = $archive.GetEntry('META-INF/MANIFEST.MF')
    if (-not $manifestEntry) { throw 'BAR manifest is missing' }
    $reader = New-Object IO.StreamReader($manifestEntry.Open())
    try { $manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
    $escapedVersion = [regex]::Escape($version)
    if ($manifest -notmatch '(?m)^Package-Name: top\.blaccat\.BBIME\r?$' -or
        $manifest -notmatch "(?m)^Package-Version: $escapedVersion`r?$" -or
        $manifest -notmatch "(?m)^Application-Version: $escapedVersion`r?$") {
        throw 'BAR version or package identity differs from the source descriptor'
    }
    $record.customSymQmlPackaged = @($archive.Entries | Where-Object {
        $_.FullName -match '(?i)(^|/)NativeSymbolPanel\.qml$'
    }).Count -gt 0
    if ($record.customSymQmlPackaged) { throw 'Removed custom Sym QML was packaged' }
    foreach ($asset in $descriptor.qnx.asset) {
        $source = Join-Path $root ([string]$asset.path)
        $entryName = 'native/' + ([string]$asset.'#text').Replace('\', '/')
        $entry = $archive.GetEntry($entryName)
        if (-not $entry) { throw "Missing declared BAR asset: $entryName" }
        $stream = $entry.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $packagedHash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
        finally { $sha.Dispose(); $stream.Dispose() }
        $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        if ($packagedHash -ne $sourceHash) { throw "Packaged asset differs from source: $entryName" }
        $record.checkedAssets += [ordered]@{source=[string]$asset.path; entry=$entryName; sha256=$sourceHash; status='PASS'}
    }
    $record.appQmlByteMatch = $true
    $record.status = 'PASS'
    Write-Output "PASS: BBIME $version BAR identity, Sym exclusion and all $($record.checkedAssets.Count) declared asset bytes"
} catch {
    $record.status = 'FAIL'
    $record.error = $_.Exception.Message
    throw
} finally {
    $archive.Dispose()
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
    $record | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding UTF8
}
