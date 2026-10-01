param(
    [string[]]$Paths,
    [string]$SdkPluginsPath = 'C:\bbndk\plugins',
    [string]$JdkRoot = '',
    [switch]$SelfTest
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not $JdkRoot) { $JdkRoot = [Environment]::GetEnvironmentVariable('INTROOP_JDK_ROOT', 'User') }
if (-not $JdkRoot) { $JdkRoot = $env:JAVA_HOME }
if (-not $JdkRoot) {
    $candidates = @(Get-ChildItem -LiteralPath 'C:\Program Files\Microsoft' -Directory -Filter 'jdk-*' -ErrorAction SilentlyContinue)
    $candidate = $candidates | Sort-Object Name -Descending | Select-Object -First 1
    if ($candidate) { $JdkRoot = $candidate.FullName }
}
if (-not $JdkRoot) {
    $onPath = Get-Command javac.exe -ErrorAction SilentlyContinue
    if ($onPath) { $JdkRoot = Split-Path -Parent (Split-Path -Parent $onPath.Source) }
}
$javac = Join-Path $JdkRoot 'bin\javac.exe'
$java = Join-Path $JdkRoot 'bin\java.exe'
if (-not (Test-Path -LiteralPath $javac) -or -not (Test-Path -LiteralPath $java)) {
    throw 'Pass -JdkRoot pointing to an installed JDK for the SDK QML parser.'
}
$parser = Get-ChildItem -LiteralPath $SdkPluginsPath -Filter 'com.rim.tad.tools.qml.parser_*.jar' |
    Sort-Object Name -Descending | Select-Object -First 1
$antlr = Get-ChildItem -LiteralPath $SdkPluginsPath -Filter 'org.antlr.runtime_3.4*.jar' |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $parser -or -not $antlr) { throw 'Official SDK QML parser and ANTLR jars are required.' }
$classes = Join-Path $root 'build\qml-parser\classes'
New-Item -ItemType Directory -Force -Path $classes | Out-Null
$classpath = $parser.FullName + [IO.Path]::PathSeparator + $antlr.FullName
& $javac -encoding UTF-8 -classpath $classpath -d $classes (Join-Path $PSScriptRoot 'verify_qml_syntax.java')
if ($LASTEXITCODE -ne 0) { throw 'SDK QML verifier compilation failed.' }
if (-not $Paths) {
    $Paths = @('main.qml','NativeModulePage.qml','CandidateStrip.qml','NativeSymbolPanel.qml','ImeToggle.qml') |
        ForEach-Object { Join-Path $root "assets\$_" }
}
$arguments = @()
if ($SelfTest) { $arguments += '--self-test' }
foreach ($path in $Paths) { $arguments += (Resolve-Path -LiteralPath $path).Path }
& $java '-Djava.awt.headless=true' -classpath ($classes + [IO.Path]::PathSeparator + $classpath) verify_qml_syntax @arguments
if ($LASTEXITCODE -ne 0) { throw "SDK QML syntax check failed: $LASTEXITCODE" }
