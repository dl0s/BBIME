param(
    [switch]$Package,
    [switch]$Tests,
    [string]$SdkRoot = '',
    [string]$JavaBin = ''
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'tools/Sdk-Environment.ps1')
$SdkRoot = Get-BBIMESdkRoot -SdkRoot $SdkRoot
if ($Package) { $JavaBin = Get-BBIMEJavaBin -SdkRoot $SdkRoot -JavaBin $JavaBin }
$savedHost = $env:QNX_HOST
$savedTarget = $env:QNX_TARGET
$savedPath = $env:PATH
Push-Location $PSScriptRoot
try {
    $env:QNX_HOST = Join-Path $SdkRoot 'host_10_3_1_12\win32\x86'
    $env:QNX_TARGET = Join-Path $SdkRoot 'target_10_3_1_995\qnx6'
    $env:PATH = (Join-Path $env:QNX_HOST 'usr\bin') + ';' + $savedPath
    New-Item -ItemType Directory -Force -Path 'build\arm' | Out-Null
    $qcc = Join-Path $env:QNX_HOST 'usr\bin\qcc.exe'
    $moc = Join-Path $env:QNX_HOST 'usr\bin\moc.exe'
    $qt = Join-Path $env:QNX_TARGET 'usr\include\qt4'
    $libs = Join-Path $env:QNX_TARGET 'armle-v7\usr\lib\qt4\lib'
    $flags = @('-V4.6.3,gcc_ntoarmv7le_cpp', '-O2', '-g', '-Wall', '-Wextra',
        '-Wno-psabi', '-Isrc', '-Ivendor/libgooglepinyin-0.1.2/include',
        "-I$qt", "-I$qt/QtCore", "-L$libs", "-Wl,-rpath-link,$libs")
    $objects = @()
    . (Join-Path $PSScriptRoot 'module/BBIME-Sources.ps1')
    foreach ($source in $BBIMEEngineSources) {
        $object = 'build/arm/' + [IO.Path]::GetFileNameWithoutExtension($source) + '.o'
        & $qcc @flags -w -c $source -o $object
        if ($LASTEXITCODE -ne 0) { throw "Engine ARM compile failed: $source" }
        $objects += $object
    }
    & $moc src/backend.h -o build/arm/moc_backend.cpp
    if ($LASTEXITCODE -ne 0) { throw 'Backend moc failed' }
    $mocSources = @('build/arm/moc_backend.cpp')
    foreach ($header in $BBIMEMocHeaders) {
        $generated = 'build/arm/moc_' + [IO.Path]::GetFileNameWithoutExtension($header) + '.cpp'
        & $moc $header -o $generated
        if ($LASTEXITCODE -ne 0) { throw "Module moc failed: $header" }
        $mocSources += $generated
    }
    & $qcc @flags -o build/bbime src/main.cpp src/backend.cpp @BBIMECoreSources @BBIMENativeSources `
        @mocSources @objects `
        -lbbcascades -lbbsystem -lbb -lQtCore -lQtDeclarative -lQtGui -lm
    if ($LASTEXITCODE -ne 0) { throw 'BBIME ARM link failed' }
    $readelf = Join-Path $env:QNX_HOST 'usr\bin\ntoarm-readelf.exe'
    $dynamic = (& $readelf -d build/bbime) -join "`n"
    if ($LASTEXITCODE -ne 0 -or $dynamic -match 'libstdc\+\+' -or
        $dynamic -notmatch 'libcpp\.so\.4') { throw 'Wrong BB10 C++ runtime ABI' }
    if ($Tests) {
        & $qcc @flags -o build/arm/decoder_test src/decoder.cpp tests/decoder_test.cpp @objects -lm
        if ($LASTEXITCODE -ne 0) { throw 'ARM decoder test link failed' }
        & $qcc @flags -o build/arm/inputmodule_test src/decoder.cpp src/inputmodule.cpp `
            tests/inputmodule_test.cpp @objects -lm
        if ($LASTEXITCODE -ne 0) { throw 'ARM input module test link failed' }
        foreach ($test in @('decoder_test', 'inputmodule_test')) {
            $testDynamic = (& $readelf -d "build/arm/$test") -join "`n"
            if ($LASTEXITCODE -ne 0 -or $testDynamic -match 'libstdc\+\+' -or
                $testDynamic -notmatch 'libcpp\.so\.4') { throw "Wrong test ABI: $test" }
        }
    }
    if ($Package) {
        $env:PATH = $JavaBin + ';' + $env:PATH
        Write-Output "Packaging Java: $JavaBin"
        & (Join-Path $env:QNX_HOST 'usr\bin\blackberry-nativepackager.bat') `
            -package -devMode build/BBIME.bar bar-descriptor.xml
        if ($LASTEXITCODE -ne 0) { throw 'BAR packaging failed' }
        Get-FileHash -LiteralPath build/BBIME.bar -Algorithm SHA256
    }
} finally {
    $env:PATH = $savedPath
    if ($null -eq $savedHost) { Remove-Item Env:QNX_HOST -ErrorAction SilentlyContinue }
    else { $env:QNX_HOST = $savedHost }
    if ($null -eq $savedTarget) { Remove-Item Env:QNX_TARGET -ErrorAction SilentlyContinue }
    else { $env:QNX_TARGET = $savedTarget }
    Pop-Location
}
