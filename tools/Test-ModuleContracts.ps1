$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Assert-Contract([bool]$condition, [string]$name) {
    if (-not $condition) { throw "Contract failed: $name" }
    Write-Output "PASS: $name"
}
$qml = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'assets/main.qml')
$backend = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/backend.cpp')
$profile = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/moduleprofile.h')
$settings = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/modulesettings.cpp')
[xml]$descriptor = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'bar-descriptor.xml')
Assert-Contract ($qml.Contains('import QtQuick 1.0 as Quick')) 'QtQuick import retained'
Assert-Contract ($qml.Contains('inputMode: TextAreaInputMode.Custom') -and
    $qml -notmatch 'TextAreaInputMode\.Text\b' -and
    $backend -notmatch 'TextAreaInputMode::Text\b') 'Custom-only editor'
Assert-Contract ($qml.Contains('actionBarVisibility: backend.imeEnabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible') -and
    $qml.Contains('onClicked: backend.toggleIme()')) 'Menu visibility and custom IME gate retained'
Assert-Contract ($qml.Contains(('backend.mode === "english" ? "EN" : "' + [char]0x4e2d + '"')) -and
    $qml.Contains('imageSource: backend.imeEnabled ? "" : "asset:///tools.png"')) 'Upper-right status button retained'
Assert-Contract ($backend.Contains('if (next == m_symbolCycleStart)')) 'Finite symbol round retained'
Assert-Contract ($backend.Contains('m_decoder.choose(index, !m_testing && learnSelections())')) 'Synthetic selection cannot learn'
Assert-Contract ($backend.Contains('!imeEnabled() && m_code.isEmpty()') -and
    $backend.Contains('!m_symbolsVisible && m_pressed.isEmpty()')) 'Settings changes require paused idle session'
Assert-Contract ($qml.Contains('backend.disableIme(); settingsSheet.open();') -and
    $qml.Contains('onClosed: backend.restoreFocus()')) 'Settings sheet cannot implicitly enable input'
Assert-Contract ($profile -notmatch 'dictionary/path|local/learnSelections|input/imeEnabled|input/composition') 'Shared schema excludes private state and file paths'
Assert-Contract ($settings.Contains('source.read(8193)') -and
    $settings.Contains('snapshot.write(bytes)') -and
    $settings.Contains('std::rename(') -and $settings.Contains('::fsync(')) 'Bounded parsing and atomic snapshot publication'
Assert-Contract ($descriptor.qnx.buildId -eq '11' -and
    @($descriptor.qnx.permission | Where-Object { $_.'#text' -eq 'access_shared' }).Count -eq 1) 'Version and explicit shared-file permission'
$hash = (Get-FileHash -LiteralPath (Join-Path $root 'assets/dict_pinyin.dat') -Algorithm SHA256).Hash
Assert-Contract ($hash -eq '179311C55AB9B912A07EF040E5A95AF97E704E2248B7B97735F17A3E05D13B67') 'Packaged ARM dictionary unchanged'
$core = Get-Content -Raw -LiteralPath (Join-Path $root 'src/inputmodule.cpp')
$adapter = Get-Content -Raw -LiteralPath (Join-Path $root 'src/nativeadapter.cpp')
$controller = Get-Content -Raw -LiteralPath (Join-Path $root 'src/nativecontroller.cpp')
$strip = Get-Content -Raw -LiteralPath (Join-Path $root 'assets/CandidateStrip.qml')
$symbols = Get-Content -Raw -LiteralPath (Join-Path $root 'assets/NativeSymbolPanel.qml')
Assert-Contract ($core -notmatch 'draft\.txt|QSettings|Clipboard|saveDraft' -and
    $adapter -notmatch '->setText\(') 'Module never owns host draft or replaces whole document'
Assert-Contract ($core.Contains('pthread_equal(') -and $core.Contains('ticket.session == id_') -and
    $core.Contains('ticket.revision == revision_') -and
    $core.Contains('editor_.revision() == ticket.document')) 'Thread and session/document ticket checks'
Assert-Contract ($adapter.Contains('area->isEditable()') -and
    $adapter.Contains('TextFieldInputMode::Custom') -and
    $adapter.Contains('static_cast<TextFieldInputMode::Type>(originalMode_)') -and
    $adapter.Contains('emit retired()')) 'Read-only, input-mode restoration and control destruction'
Assert-Contract ($controller.Contains('BindingCall call(*this, binding)') -and
    $controller.Contains('binding->retired = true') -and
    $controller.Contains('binding->adapter->deleteLater()')) 'In-flight host callbacks cannot delete their binding'
Assert-Contract ($strip.Contains('candidate.session, candidate.revision, candidate.document') -and
    $symbols.Contains('panel.ime.chooseSymbol(item.session, item.revision,') -and
    $controller.Contains('active_->session->accepts(ticket)') -and
    $controller.Contains('panel != symbolRevision_')) 'Candidate and symbol UI carry stale-result tickets'
Assert-Contract ($controller.Contains('event->isCtrlPressed() || (enter && event->isAltPressed())') -and
    $controller.Contains('submitRequested(control)') -and
    $backend.Contains('!m_nativeModule->enabled()') -and
    $backend.Contains('if (m_nativePageOpen || !m_active') -and
    $qml.Contains('onClosed: backend.stopNativeModule()')) 'Host shortcut routing, idle settings and private-page capture guard'
Write-Output 'Source contracts only; device UI and cross-application permissions still require runtime tests.'
