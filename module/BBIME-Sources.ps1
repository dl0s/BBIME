$BBIMEModuleRoot = Split-Path -Parent $PSScriptRoot
$BBIMECoreSources = @('src/decoder.cpp', 'src/inputmodule.cpp') |
    ForEach-Object { Join-Path $BBIMEModuleRoot $_ }
$BBIMENativeSources = @('src/nativeadapter.cpp', 'src/nativecontroller.cpp', 'src/modulesettings.cpp') |
    ForEach-Object { Join-Path $BBIMEModuleRoot $_ }
$BBIMEMocHeaders = @('src/nativeadapter.h', 'src/nativecontroller.h') |
    ForEach-Object { Join-Path $BBIMEModuleRoot $_ }
$BBIMEEngineSources = @(Get-ChildItem -LiteralPath (Join-Path $BBIMEModuleRoot 'vendor/libgooglepinyin-0.1.2/src') -Filter '*.cpp' |
    ForEach-Object { $_.FullName })
$BBIMEIncludeDirectories = @(
    (Join-Path $BBIMEModuleRoot 'src'),
    (Join-Path $BBIMEModuleRoot 'vendor/libgooglepinyin-0.1.2/include')
)
$BBIMEAssets = @('assets/CandidateStrip.qml', 'assets/NativeSymbolPanel.qml',
    'assets/cancel.png', 'assets/dict_pinyin.dat') |
    ForEach-Object { Join-Path $BBIMEModuleRoot $_ }
$BBIMELicenseAssets = @('vendor/libgooglepinyin-0.1.2/LICENSE',
    'vendor/libgooglepinyin-0.1.2/COPYRIGHT', 'vendor/libgooglepinyin-0.1.2/BBIME-MODIFICATIONS.md') |
    ForEach-Object { Join-Path $BBIMEModuleRoot $_ }
