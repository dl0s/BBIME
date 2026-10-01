param(
    [switch]$Device,
    [string]$SdkRoot = '',
    [string]$QmlVerifierScript = '',
    [string]$OutputPath = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'Sdk-Environment.ps1')
$SdkRoot = Get-BBIMESdkRoot -SdkRoot $SdkRoot
if (-not $QmlVerifierScript) {
    $QmlVerifierScript = Join-Path (Split-Path -Parent $root) 'BBarmin/bb10-native/verify_qml_syntax.ps1'
}
if (-not (Test-Path -LiteralPath $QmlVerifierScript -PathType Leaf)) {
    throw "QML verifier not found. Pass -QmlVerifierScript: $QmlVerifierScript"
}
if (-not $OutputPath) { $OutputPath = Join-Path $root 'research/phase-ab-validation-0.1.0.12.json' }
elseif (-not [IO.Path]::IsPathRooted($OutputPath)) { $OutputPath = Join-Path $root $OutputPath }
$record = [ordered]@{
    timestamp = [DateTimeOffset]::Now.ToOffset([TimeSpan]::FromHours(8)).ToString('o')
    sourceVersion = '0.1.0.12'
    sdkRoot = $SdkRoot
    packagingJavaBin = (Get-BBIMEJavaBin -SdkRoot $SdkRoot)
    qmlVerifierScript = $QmlVerifierScript
    status = 'RUNNING'
    checks = @()
    device = 'NOT_RUN'
    releaseReady = $false
    remainingGates = @('ARM_SYNTHETIC_CORE_RUNTIME', 'Q10_NATIVE_UI_FOCUS_KEYS_AND_SYMBOLS', 'TWO_ORDINARY_HOST_APPS',
        'SHARED_PERMISSION_FAILURES_IF_ENABLED', 'LIFECYCLE_AND_STORAGE_STRESS')
    sourceAndArtifactSha256 = [ordered]@{}
}
function Invoke-Logged([string]$Script, [string[]]$Arguments, [string]$Log) {
    # Windows PowerShell surfaces native compiler warnings as error records.
    # Only the child process exit code decides whether a check passed.
    $ErrorActionPreference = 'Continue'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $root $Script) @Arguments *> $Log
    return $LASTEXITCODE
}
function Invoke-Check([string]$Name, [string]$Script, [string[]]$Arguments) {
    $log = Join-Path $root ("build/$Name-validation.log")
    $exitCode = Invoke-Logged $Script $Arguments $log
    $record.checks += [ordered]@{
        name = $Name
        status = $(if ($exitCode -eq 0) { 'PASS' } else { 'FAIL' })
        exitCode = $exitCode
        log = $log
    }
    if ($exitCode -ne 0) { throw "Validation failed: $Name. See $log" }
    Write-Output "PASS: $Name"
}
New-Item -ItemType Directory -Force -Path (Join-Path $root 'build') | Out-Null
try {
    Invoke-Check 'host-regression' 'tools/Test-Decoder.ps1' @()
    Invoke-Check 'module-contracts' 'tools/Test-ModuleContracts.ps1' @()
    Invoke-Check 'arm-build' 'build.ps1' @('-Package', '-Tests', '-SdkRoot', $SdkRoot)
    $paths = @('main.qml', 'NativeModulePage.qml', 'CandidateStrip.qml', 'NativeSymbolPanel.qml', 'ImeToggle.qml') |
        ForEach-Object { Join-Path $root "assets/$_" }
    $syntaxLog = Join-Path $root 'build/qml-validation.log'
    & $QmlVerifierScript -Paths $paths *> $syntaxLog
    $record.checks += [ordered]@{ name = 'qml-syntax'; status = 'PASS'; log = $syntaxLog;
        scope = 'SYNTAX_ONLY_NOT_RUNTIME_TYPE_OR_LAYOUT_VALIDATION' }
    Write-Output 'PASS: qml-syntax'
    if ($Device) {
        $deviceLog = Join-Path $root 'build/arm-device-validation.log'
        $deviceExit = Invoke-Logged 'tools/Test-Q10Module.ps1' @() $deviceLog
        $deviceFile = Join-Path $root 'research/native-module-arm-validation-0.1.0.12.json'
        if (Test-Path -LiteralPath $deviceFile) {
            $deviceRecord = Get-Content -Raw -Encoding UTF8 -LiteralPath $deviceFile | ConvertFrom-Json
            $record.device = $deviceRecord.status
            $record.deviceEvidence = $deviceFile
            if ($record.device -eq 'PASS') {
                $record.remainingGates = @($record.remainingGates |
                    Where-Object { $_ -ne 'ARM_SYNTHETIC_CORE_RUNTIME' })
            }
        } else { $record.device = 'UNAVAILABLE_WITHOUT_EVIDENCE' }
        Write-Output ("ARM synthetic runtime: " + $record.device)
    } else {
        $priorDeviceFile = Join-Path $root 'research/native-module-arm-validation-0.1.0.12.json'
        if (Test-Path -LiteralPath $priorDeviceFile) {
            $priorDevice = Get-Content -Raw -Encoding UTF8 -LiteralPath $priorDeviceFile | ConvertFrom-Json
            $record.priorDeviceEvidence = $priorDeviceFile
            $record.priorDeviceStatus = $priorDevice.status
        }
    }
    $record.status = 'LOCAL_PASS_DEVICE_AND_HOST_INTEGRATION_GATES_REMAIN'
} catch {
    $record.status = 'FAIL'
    $record.error = $_.Exception.Message
    throw
} finally {
    $files = @('src/decoder.h', 'src/decoder.cpp', 'src/inputmodule.h', 'src/inputmodule.cpp',
        'src/nativeadapter.h', 'src/nativeadapter.cpp', 'src/nativecontroller.h',
        'src/nativecontroller.cpp', 'src/textpositions.h', 'src/moduleprofile.h',
        'src/symbols.h', 'tests/decoder_test.cpp',
        'tests/inputmodule_test.cpp', 'assets/NativeModulePage.qml', 'assets/CandidateStrip.qml',
        'assets/NativeSymbolPanel.qml', 'assets/ImeToggle.qml', 'assets/ime-menu.png',
        'src/modulesettings.h', 'src/modulesettings.cpp', 'vendor/libgooglepinyin-0.1.2/src/matrixsearch.cpp',
        'vendor/libgooglepinyin-0.1.2/src/dicttrie.cpp', 'src/backend.cpp',
        'src/main.cpp', 'src/backend.h', 'assets/main.qml', 'bar-descriptor.xml', 'module/BBIME-Sources.ps1',
        'tools/Test-Decoder.ps1', 'tools/Test-ModuleContracts.ps1',
        'tools/Test-PhaseAB.ps1', 'tools/Test-Q10Module.ps1', 'tools/Sdk-Environment.ps1',
        'tools/Test-SdkApi.ps1', 'build.ps1', 'build/BBIME.bar',
        'build/host/decoder_test.exe', 'build/host/inputmodule_test.exe',
        'build/arm/decoder_test', 'build/arm/inputmodule_test', 'assets/dict_pinyin.dat')
    foreach ($file in $files) {
        $path = Join-Path $root $file
        if (Test-Path -LiteralPath $path) {
            $record.sourceAndArtifactSha256[$file] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        }
    }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutputPath) | Out-Null
    $record | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $OutputPath -Encoding UTF8
}
