param(
    [string]$PlanPath = '',
    [string]$ResultPath = '',
    [switch]$VerifyOnly
)
$ErrorActionPreference = 'Stop'
$workspaceRoot = (Get-Item -LiteralPath (Split-Path -Parent $PSScriptRoot)).FullName
if (-not $PlanPath) { $PlanPath = Join-Path $workspaceRoot 'build\rectification\apply-plan.json' }
if (-not $ResultPath) { $ResultPath = Join-Path $workspaceRoot 'build\rectification\apply-result.json' }
$plan = Get-Content -Raw -Encoding UTF8 -LiteralPath $PlanPath | ConvertFrom-Json
$allowedRoots = @(
    'C:\Users\dove\Documents\BBnote',
    'C:\Users\dove\Documents\IntroOP',
    'C:\Users\dove\Documents\BBFile'
)
$stagingRoot = [IO.Path]::GetFullPath((Join-Path $workspaceRoot 'build\rectification'))
$files = @()
foreach ($app in $plan.apps) {
    $targetRoot = [IO.Path]::GetFullPath($app.targetRoot)
    $sourceRoot = [IO.Path]::GetFullPath($app.sourceRoot)
    if ($targetRoot -notin $allowedRoots) { throw "Unexpected target root: $targetRoot" }
    if (-not $sourceRoot.StartsWith($stagingRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Staging root is outside this workspace: $sourceRoot"
    }
    foreach ($file in $app.files) {
        if ([IO.Path]::IsPathRooted($file.path) -or $file.path -match '(^|[\\/])\.\.?([\\/]|$)' -or
            $file.path -match '(^|[\\/])(\.git|\.codex|\.agents|data|logs|build)([\\/]|$)') {
            throw "Disallowed relative path: $($file.path)"
        }
        $target = [IO.Path]::GetFullPath((Join-Path $targetRoot $file.path))
        $source = [IO.Path]::GetFullPath((Join-Path $sourceRoot $file.path))
        if (-not $target.StartsWith($targetRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
            -not $source.StartsWith($sourceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Escaping file path: $($file.path)"
        }
        if (-not (Test-Path -LiteralPath $source -PathType Leaf) -or
            (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash -ne $file.newSha256) {
            throw "Staged source changed: $source"
        }
        if ($file.oldSha256) {
            if (-not (Test-Path -LiteralPath $target -PathType Leaf) -or
                (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $file.oldSha256) {
                throw "Host file changed since review; no files will be applied: $target"
            }
        } elseif (Test-Path -LiteralPath $target) {
            throw "New-file target already exists; no files will be applied: $target"
        }
        $files += [pscustomobject]@{ app=$app.name; relative=$file.path; source=$source; target=$target;
            oldSha256=$file.oldSha256; newSha256=$file.newSha256; backup=$null }
    }
}
if (($files | Group-Object target | Where-Object Count -gt 1).Count) { throw 'Duplicate target in plan.' }
Write-Output ("Validated {0} prepared source files across {1} applications." -f $files.Count, $plan.apps.Count)
if ($VerifyOnly) { return }
$backupRoot = Join-Path $stagingRoot ('backups\' + [guid]::NewGuid().ToString('N'))
$applied = New-Object System.Collections.Generic.List[object]
$pendingTemp = $null
try {
    foreach ($file in $files) {
        if ($file.oldSha256) {
            $file.backup = Join-Path (Join-Path $backupRoot $file.app) $file.relative
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file.backup) | Out-Null
            Copy-Item -LiteralPath $file.target -Destination $file.backup
        }
    }
    foreach ($file in $files) {
        # Recheck immediately before replacement; preserve other concurrent work.
        if ($file.oldSha256) {
            if ((Get-FileHash -LiteralPath $file.target -Algorithm SHA256).Hash -ne $file.oldSha256) {
                throw "Host file changed during application: $($file.target)"
            }
        } elseif (Test-Path -LiteralPath $file.target) {
            throw "New-file target appeared during application: $($file.target)"
        }
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $file.target) | Out-Null
        # Verify the copied bytes before one atomic replacement. A failed copy
        # must never leave a partially written host source file.
        $pendingTemp = $file.target + '.ime-' + [guid]::NewGuid().ToString('N') + '.tmp'
        Copy-Item -LiteralPath $file.source -Destination $pendingTemp
        if ((Get-FileHash -LiteralPath $pendingTemp -Algorithm SHA256).Hash -ne $file.newSha256) {
            throw "Prepared copy hash mismatch: $($file.target)"
        }
        if ($file.oldSha256) {
            if ((Get-FileHash -LiteralPath $file.target -Algorithm SHA256).Hash -ne $file.oldSha256) {
                throw "Host file changed before atomic replacement: $($file.target)"
            }
            [IO.File]::Replace($pendingTemp, $file.target, [System.Management.Automation.Language.NullString]::Value)
        } else { [IO.File]::Move($pendingTemp, $file.target) }
        $pendingTemp = $null
        $applied.Add($file)
        if ((Get-FileHash -LiteralPath $file.target -Algorithm SHA256).Hash -ne $file.newSha256) {
            throw "Installed source hash mismatch: $($file.target)"
        }
    }
} catch {
    $failure = $_
    if ($pendingTemp -and (Test-Path -LiteralPath $pendingTemp)) {
        Remove-Item -LiteralPath $pendingTemp -Force
    }
    for ($i = $applied.Count - 1; $i -ge 0; --$i) {
        $file = $applied[$i]
        if ((Get-FileHash -LiteralPath $file.target -Algorithm SHA256).Hash -ne $file.newSha256) {
            Write-Warning "Concurrent change preserved; restore manually from $($file.backup): $($file.target)"
            continue
        }
        if ($file.backup) { Copy-Item -LiteralPath $file.backup -Destination $file.target -Force }
        else { Remove-Item -LiteralPath $file.target -Force }
    }
    throw $failure
}
$result = [ordered]@{status='APPLIED';backupRoot=$backupRoot;fileCount=$files.Count;apps=@($plan.apps.name);
    planSha256=(Get-FileHash -LiteralPath $PlanPath -Algorithm SHA256).Hash}
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $ResultPath -Encoding UTF8
Write-Output ("Applied {0} source files; original files saved to {1}" -f $files.Count,$backupRoot)
