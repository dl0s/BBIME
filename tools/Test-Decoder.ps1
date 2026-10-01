$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Push-Location $root
$oldPath = $env:PATH
try {
    $env:PATH = 'C:\cygwin64\bin;' + $oldPath
    $gpp = 'C:\cygwin64\bin\x86_64-w64-mingw32-g++.exe'
    $vendor = 'vendor\libgooglepinyin-0.1.2'
    $sources = @(Get-ChildItem -LiteralPath "$vendor\src" -Filter '*.cpp' |
        ForEach-Object { $_.FullName })
    New-Item -ItemType Directory -Force -Path 'build\host' | Out-Null
    & $gpp -std=c++98 -O2 -static -I"$vendor\include" @sources `
        "$vendor\tools\pinyinime_dictbuilder.cpp" -o build/host/dictbuilder.exe -lpthread
    if ($LASTEXITCODE -ne 0) { throw 'Host dictionary builder failed' }
    & '.\build\host\dictbuilder.exe' "$vendor/data/rawdict_utf16_65105_freq.txt" `
        "$vendor/data/valid_utf16.txt" build/host/dict_pinyin.dat
    if ($LASTEXITCODE -ne 0) { throw 'Host dictionary build failed' }
    & $gpp -std=c++98 -O2 -static -Wall -I"$vendor\include" @sources `
        src/decoder.cpp tests/decoder_test.cpp -o build/host/decoder_test.exe -lpthread
    if ($LASTEXITCODE -ne 0) { throw 'Host decoder compile failed' }
    $user = 'build/host/test-user-' + [guid]::NewGuid().ToString('N') + '.dat'
    & '.\build\host\decoder_test.exe' build/host/dict_pinyin.dat $user
    if ($LASTEXITCODE -ne 0) { throw 'Decoder regression failed' }
    & $gpp -std=c++98 -O2 -static -Wall -I"$vendor\include" @sources `
        src/decoder.cpp src/inputmodule.cpp tests/inputmodule_test.cpp `
        -o build/host/inputmodule_test.exe -lpthread
    if ($LASTEXITCODE -ne 0) { throw 'Host input module compile failed' }
    $moduleUser = 'build/host/module-user-' + [guid]::NewGuid().ToString('N') + '.dat'
    & '.\build\host\inputmodule_test.exe' build/host/dict_pinyin.dat $moduleUser
    if ($LASTEXITCODE -ne 0) { throw 'Input module regression failed' }
} finally {
    $env:PATH = $oldPath
    Pop-Location
}
