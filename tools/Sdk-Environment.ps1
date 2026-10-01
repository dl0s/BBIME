function Get-BBIMESdkRoot {
    param([string]$SdkRoot = '')

    # Read the user setting directly: an already-running app can have a stale
    # or absent process copy after the SDK location changes.
    if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
        $SdkRoot = [Environment]::GetEnvironmentVariable('INTROOP_SDK_ROOT', 'User')
    }
    if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
        $SdkRoot = [Environment]::GetEnvironmentVariable('INTROOP_SDK_ROOT', 'Process')
    }
    if ([string]::IsNullOrWhiteSpace($SdkRoot)) {
        throw 'Set the user environment variable INTROOP_SDK_ROOT or pass -SdkRoot to select the BB10 SDK.'
    }

    $expandedRoot = [Environment]::ExpandEnvironmentVariables($SdkRoot.Trim())
    if (-not (Test-Path -LiteralPath $expandedRoot -PathType Container)) {
        throw "BB10 SDK directory does not exist: $expandedRoot"
    }
    $resolvedRoot = (Get-Item -LiteralPath $expandedRoot).FullName
    foreach ($relativePath in @(
        'host_10_3_1_12/win32/x86/usr/bin/qcc.exe',
        'host_10_3_1_12/win32/x86/usr/bin/moc.exe',
        'host_10_3_1_12/win32/x86/usr/bin/ntoarm-readelf.exe'
    )) {
        $requiredPath = Join-Path $resolvedRoot $relativePath
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
            throw "Required BB10 SDK tool missing: $requiredPath"
        }
    }
    if (-not (Test-Path -LiteralPath (Join-Path $resolvedRoot 'target_10_3_1_995/qnx6/usr/include') -PathType Container)) {
        throw "BB10 SDK target headers missing in: $resolvedRoot"
    }
    return $resolvedRoot
}

function Get-BBIMEJavaBin {
    param([string]$SdkRoot, [string]$JavaBin = '')

    if ([string]::IsNullOrWhiteSpace($JavaBin)) {
        $JavaBin = [Environment]::GetEnvironmentVariable('INTROOP_JAVA_BIN', 'User')
    }
    if ([string]::IsNullOrWhiteSpace($JavaBin)) {
        $JavaBin = [Environment]::GetEnvironmentVariable('INTROOP_JAVA_BIN', 'Process')
    }
    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($JavaBin)) {
        $candidates = @([Environment]::ExpandEnvironmentVariables($JavaBin.Trim()))
    } else {
        # The extracted compiler SDK may have no JRE. Discover the installed
        # Momentics runtime separately, without pinning its versioned directory.
        $features = @((Join-Path $SdkRoot 'features'),
            (Join-Path (Split-Path -Parent $SdkRoot) 'bbndk/features'))
        foreach ($directory in $features) {
            if (Test-Path -LiteralPath $directory -PathType Container) {
                $candidates += @(Get-ChildItem -LiteralPath $directory -Directory -Filter 'com.qnx.tools.jre.*' |
                    Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'jre/bin' })
            }
        }
        if ($env:JAVA_HOME) { $candidates += Join-Path $env:JAVA_HOME 'bin' }
        $onPath = Get-Command java.exe -ErrorAction SilentlyContinue
        if ($onPath) { $candidates += Split-Path -Parent $onPath.Source }
    }
    foreach ($directory in $candidates) {
        $java = Join-Path $directory 'java.exe'
        if (-not (Test-Path -LiteralPath $java -PathType Leaf)) { continue }
        # java -version writes to stderr; judge exit code and reported version.
        $ErrorActionPreference = 'Continue'
        $version = (& $java -version 2>&1 | ForEach-Object { $_.ToString() }) -join "`n"
        $exitCode = $LASTEXITCODE
        if ($exitCode -eq 0 -and $version -match 'version "1\.[78]\.') {
            return (Get-Item -LiteralPath $directory).FullName
        }
    }
    throw 'BB10 BAR packaging requires Java 7/8. Pass -JavaBin or set INTROOP_JAVA_BIN to a compatible java.exe directory.'
}
