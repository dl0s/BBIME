$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
function Assert-Contract([bool]$condition, [string]$name) {
    if (-not $condition) { throw "Contract failed: $name" }
    Write-Output "PASS: $name"
}
function Get-CppBody([string]$source, [string]$signature) {
    $start = $source.IndexOf($signature, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production function: $signature" }
    $begin = $source.IndexOf('{', $start)
    $depth = 1
    $quote = ''
    $comment = ''
    for ($i = $begin + 1; $i -lt $source.Length; ++$i) {
        $c = $source[$i]
        $next = $(if ($i + 1 -lt $source.Length) { $source[$i + 1] } else { [char]0 })
        if ($comment -eq 'line') { if ($c -eq "`n") { $comment = '' }; continue }
        if ($comment -eq 'block') {
            if ($c -eq '*' -and $next -eq '/') { $comment = ''; ++$i }
            continue
        }
        if ($quote) {
            if ($c -eq [char]92) { ++$i }
            elseif ($c -eq $quote) { $quote = '' }
            continue
        }
        if ($c -eq '/' -and $next -eq '/') { $comment = 'line'; ++$i; continue }
        if ($c -eq '/' -and $next -eq '*') { $comment = 'block'; ++$i; continue }
        if ($c -eq '"' -or $c -eq "'") { $quote = $c; continue }
        if ($c -eq '{') { ++$depth }
        elseif ($c -eq '}') {
            --$depth
            if ($depth -eq 0) { return $source.Substring($begin + 1, $i - $begin - 1) }
        }
    }
    throw "Unclosed production function: $signature"
}
$qml = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'assets/main.qml')
$backend = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/backend.cpp')
$profile = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/moduleprofile.h')
$settings = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/modulesettings.cpp')
$toggle = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'assets/ImeToggle.qml')
$nativePage = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'assets/NativeModulePage.qml')
$manifest = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'module/BBIME-Sources.ps1')
. (Join-Path $root 'module/BBIME-Sources.ps1')
[xml]$descriptor = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'bar-descriptor.xml')
Assert-Contract ($qml.Contains('import QtQuick 1.0 as Quick')) 'QtQuick import retained'
Assert-Contract ($qml.Contains('inputMode: TextAreaInputMode.Custom') -and
    $qml -notmatch 'TextAreaInputMode\.Text\b' -and
    $backend -notmatch 'setInputMode\s*\(\s*TextAreaInputMode::Text\b') 'Custom-only editor; eligibility reads do not enable system input'
Assert-Contract ($qml.Contains('actionBarVisibility: backend.imeEnabled ? ChromeVisibility.Hidden : ChromeVisibility.Visible') -and
    $qml.Contains('onToggleRequested: backend.toggleIme()')) 'Menu visibility and custom IME gate retained'
Assert-Contract ($toggle.Contains(('inputMode === "english" ? "EN" : "' + [char]0x4e2d + '"')) -and
    $toggle.Contains('imageSource: inputEnabled ? "" : "asset:///ime-menu.png"') -and
    $toggle.Contains('toggleRequested();') -and
    $toggle.Contains('focusPolicy: FocusPolicy.None') -and
    $toggle.Contains('maxWidth: 76') -and $toggle.Contains('maxHeight: 64') -and
    $toggle.Contains('Container {') -and $toggle.Contains('ImageView {') -and
    $toggle.Contains('maxWidth: 36') -and $toggle.Contains('maxHeight: 36') -and
    $toggle -notmatch '(?m)^Button\s*\{|cycle\(|setMode\(|inputEnabled\s*=') 'Stateless fixed outer frame with independently sized icon'
Assert-Contract ($qml.Contains('ImeToggle {') -and $nativePage.Contains('ImeToggle {') -and
    $nativePage.Contains('onToggleRequested: backend.toggleNativeIme()') -and
    $nativePage.Contains('keysIgnoreFocusInActionBar: ime.enabled')) 'Shared rightmost button and native menu gate'
Assert-Contract ($qml -notmatch 'value:\s*"full"|value:\s*"system"' -and
    $nativePage -notmatch 'value:\s*"full"|value:\s*"system"' -and
    $profile.Contains('out["profile/version"] = "2"') -and
    $profile -notmatch 'out\["input/chineseMode"\]') 'Only natural/English exposed; schema v2'
Assert-Contract ($settings.Contains('!(learn && ModuleProfile::migrateLegacy(values, next))')) 'Legacy migration limited to private settings'
Assert-Contract ($manifest.Contains('assets/ImeToggle.qml') -and $manifest.Contains('assets/ime-menu.png') -and
    @($descriptor.qnx.asset | Where-Object { $_.path -eq 'assets/ImeToggle.qml' }).Count -eq 1 -and
    @($descriptor.qnx.asset | Where-Object { $_.path -eq 'assets/ime-menu.png' }).Count -eq 1) 'Shared button and icon exported and packaged'
Assert-Contract ($qml -notmatch '\bDialog\s*\{|NativeSymbolPanel\s*\{|\bcycleSymbols\s*\(|\bchooseSymbol\s*\(|\bonSymbolsChanged\s*:' -and
    $nativePage -notmatch '\bDialog\s*\{|NativeSymbolPanel\s*\{|\bcycleSymbols\s*\(|\bchooseSymbol\s*\(' -and
    @($descriptor.qnx.asset | Where-Object { $_.path -eq 'assets/NativeSymbolPanel.qml' }).Count -eq 0 -and
    @($BBIMEAssets | Where-Object { (Split-Path -Leaf $_) -eq 'NativeSymbolPanel.qml' }).Count -eq 0 -and
    (Test-Path -LiteralPath (Join-Path $root 'assets/NativeSymbolPanel.qml'))) 'Default product and module assets exclude Sym UI; legacy research source is retained only'
Assert-Contract ($backend.Contains('m_decoder.choose(index, !m_testing && learnSelections())')) 'Synthetic selection cannot learn'
Assert-Contract ($backend.Contains('!imeEnabled() && m_code.isEmpty()') -and
    $backend.Contains('!m_symbolsVisible && m_pressed.isEmpty()')) 'Settings changes require paused idle session'
Assert-Contract ($qml.Contains('page.sheetActive = true;') -and
    $qml.Contains('page.syncMainScope();') -and $qml.Contains('settingsSheet.open();') -and
    $qml -notmatch 'onClosed:\s*backend\.restoreFocus\(' -and
    $backend.Contains('m_mainFocusGate.pause()')) 'Settings scope and manual pause do not force editor focus on close'
Assert-Contract ($profile -notmatch 'dictionary/path|local/learnSelections|input/imeEnabled|input/composition') 'Shared schema excludes private state and file paths'
Assert-Contract ($settings.Contains('source.read(8193)') -and
    $settings.Contains('snapshot.write(bytes)') -and
    $settings.Contains('std::rename(') -and $settings.Contains('::fsync(')) 'Bounded parsing and atomic snapshot publication'
Assert-Contract ($descriptor.qnx.buildId -eq '15' -and
    @($descriptor.qnx.permission | Where-Object { $_.'#text' -eq 'access_shared' }).Count -eq 1) 'Version and explicit shared-file permission'
$hash = (Get-FileHash -LiteralPath (Join-Path $root 'assets/dict_pinyin.dat') -Algorithm SHA256).Hash
Assert-Contract ($hash -eq '179311C55AB9B912A07EF040E5A95AF97E704E2248B7B97735F17A3E05D13B67') 'Packaged ARM dictionary unchanged'
$core = Get-Content -Raw -LiteralPath (Join-Path $root 'src/inputmodule.cpp')
$adapter = Get-Content -Raw -LiteralPath (Join-Path $root 'src/nativeadapter.cpp')
$controller = Get-Content -Raw -LiteralPath (Join-Path $root 'src/nativecontroller.cpp')
$strip = Get-Content -Raw -LiteralPath (Join-Path $root 'assets/CandidateStrip.qml')
Assert-Contract ($core -notmatch 'draft\.txt|QSettings|Clipboard|saveDraft' -and
    $adapter -notmatch '->setText\(') 'Module never owns host draft or replaces whole document'
Assert-Contract ($core.Contains('pthread_equal(') -and $core.Contains('ticket.session == id_') -and
    $core.Contains('ticket.revision == revision_') -and
    $core.Contains('editor_.revision() == ticket.document')) 'Thread and session/document ticket checks'
Assert-Contract ($adapter.Contains('area->isEditable()') -and
    $adapter.Contains('TextFieldInputMode::Custom') -and
    $adapter.Contains('static_cast<TextFieldInputMode::Type>(originalMode_)') -and
    $adapter.Contains('emit retired()')) 'Read-only, input-mode restoration and control destruction'
Assert-Contract ($adapter -notmatch 'leased_ && mode == Text(Field|Area)InputMode::Custom' -and
    $adapter.Contains('mode == TextFieldInputMode::Custom') -and
    $adapter.Contains('mode == TextAreaInputMode::Custom')) 'Preconfigured Custom fields can register and remain Custom on release'
Assert-Contract ($controller.Contains('BindingCall call(*this, binding)') -and
    $controller.Contains('binding->retired = true') -and
    $controller.Contains('binding->adapter->deleteLater()')) 'In-flight host callbacks cannot delete their binding'
Assert-Contract ($strip.Contains('candidate.session, candidate.revision, candidate.document') -and
    $controller.Contains('binding->session->choose(ticket)') -and
    $core.Contains('if (!accepts(ticket) || ticket.index >= candidates_.size())')) 'Default candidate UI carries session/document tickets and the core rejects stale results'
Assert-Contract ($controller.Contains('event->isCtrlPressed() || (enter && event->isAltPressed())') -and
    $controller.Contains('submitRequested(control)') -and
    $backend.Contains('!m_nativeModule->enabled()') -and
    $backend.Contains('if (m_nativePageOpen || !m_active') -and
    $qml.Contains('backend.stopNativeModule();')) 'Host shortcut routing, idle settings and private-page capture guard'
Assert-Contract ($controller.Contains('identity == KEYCODE_LEFT_ALT') -and
    $controller.Contains('identity == KEYCODE_RIGHT_ALT') -and
    $controller.Contains('identity == Qt::Key_Meta') -and
    $controller.IndexOf('if (modifier)') -lt $controller.IndexOf('QString unicode = event->unicode()')) 'Bare modifiers classified before Unicode insertion'
$moduleSymbolCycle = Get-CppBody $controller 'void NativeController::cycleSymbols()'
$moduleSymbolChoose = Get-CppBody $controller 'bool NativeController::chooseSymbol(unsigned long session, unsigned long revision,'
$moduleSymbolGroup = Get-CppBody $controller 'void NativeController::setSymbolGroup(int group)'
$moduleSymbolPanel = Get-CppBody $controller 'void NativeController::setSymbolPanelActive(bool value)'
$moduleSymbolKey = Get-CppBody $controller 'bool NativeController::handleSymbolKey(QObject *event)'
$moduleKey = Get-CppBody $controller 'bool NativeController::handleKey(QObject *control, QObject *object)'
Assert-Contract ($moduleSymbolCycle -notmatch 'refreshSymbols\(|symbolsVisible_\s*=\s*true|insertLiteral\(' -and
    $moduleSymbolChoose.Contains('return false;') -and
    $moduleSymbolChoose -notmatch 'insertLiteral\(|session->choose\(' -and
    $moduleKey.Contains('KEYCODE_F1 + 21') -and $moduleKey.Contains('Qt::Key_F22') -and
    $moduleKey.Contains('event->accept();')) 'Default controller consumes Sym identity without opening UI or inserting symbols'
Assert-Contract ($moduleSymbolGroup -notmatch 'symbolGroup_\s*=|refreshSymbols\(|emit\s+symbolsChanged' -and
    $moduleSymbolPanel -notmatch 'symbolPanelActive_\s*=|emit\s+symbolsChanged' -and
    $moduleSymbolKey.Contains('return false;') -and
    $moduleSymbolKey -notmatch 'handleKey\(|insertLiteral\(') 'Default compatibility symbol callbacks cannot reactivate the removed panel'
$moduleSymBranch = Get-CppBody $moduleKey 'if (identity == KEYCODE_F1 + 21'
Assert-Contract ($moduleKey.IndexOf('identity == KEYCODE_F1 + 21') -lt $moduleKey.IndexOf('const bool leftShift') -and
    $moduleKey.IndexOf('identity == KEYCODE_F1 + 21') -lt $moduleKey.IndexOf('event->isCtrlPressed() || (enter') -and
    $moduleSymBranch.Contains('if (event->isPressed()) suppressStandaloneShifts();') -and
    $moduleSymBranch.Contains('event->accept();') -and $moduleSymBranch.Contains('return true;') -and
    $moduleSymBranch -notmatch 'event->isCtrlPressed\(|event->isAltPressed\(|insertLiteral\(|cycleSymbols\(') 'Sym is consumed before modifier filtering and its press cancels standalone Shift actions'
Assert-Contract ($nativePage.Contains('backend.handleNativeKey(title, event)') -and
    $nativePage.Contains('backend.handleNativeKey(body, event)') -and
    $backend.Contains('return m_nativeModule->handleKey(editor, object);') -and
    $backend.Contains('event->isPressed() && event->duration() == 0') -and
    $nativePage.Contains('inputMode: TextFieldInputMode.Custom') -and
    $nativePage.Contains('inputMode: TextAreaInputMode.Custom') -and
    $nativePage.Contains('input.flags: TextInputFlag.VirtualKeyboardOff')) 'Native sample owns language shortcut and stays Custom while paused'
$eligibility = Get-CppBody $backend 'static bool eligibleAppEditor(QObject *object)'
$mainKey = Get-CppBody $backend 'bool Backend::handleKey(QObject *object)'
$nativeKey = Get-CppBody $backend 'bool Backend::handleNativeKey(QObject *editor, QObject *object)'
$mainReconcile = Get-CppBody $backend 'void Backend::reconcileMainFocus()'
$nativeReconcile = Get-CppBody $backend 'void Backend::reconcileNativeFocus()'
$active = Get-CppBody $backend 'void Backend::active()'
$inactive = Get-CppBody $backend 'void Backend::inactive()'
$candidateAt = Get-CppBody $backend 'bool Backend::chooseCandidateAt(int index, unsigned long generation)'
$symbolCycle = Get-CppBody $backend 'void Backend::cycleSymbols()'
$symbolChoose = Get-CppBody $backend 'bool Backend::chooseSymbol(int index)'
$gate = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/focusstate.h')
$backendHeader = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'src/backend.h')
Assert-Contract ($eligibility.Contains('text->textFormat() != TextFormat::Plain') -and
    $eligibility.Contains('!visual->isVisible()') -and $eligibility.Contains('!control->isEnabled()') -and
    $eligibility.Contains('area->isEditable()') -and
    $backend.Contains('foreach (const QPointer<QObject> &editor, m_nativeEditors)') -and
    $nativePage.Contains('backend.registerNativeEditor(title)') -and
    $nativePage.Contains('backend.registerNativeEditor(body)') -and
    $nativePage -notmatch 'registerNativeEditor\(password\)') 'Application qualification checks plain editable visible enabled registered owners'
Assert-Contract ($mainReconcile.Contains('m_mainFocusGate.observe(owner)') -and
    $mainReconcile.Contains('m_mainFocusGate.permits(') -and
    $nativeReconcile.Contains('m_nativeFocusGate.observe(owner)') -and
    $nativeReconcile.Contains('m_nativeFocusGate.permits(') -and
    $gate.Contains('owner_ == actualFocusedEditor') -and $gate.Contains('!paused_') -and
    $gate.Contains('class EditorFocusGate') -and $backendHeader.Contains('#include "focusstate.h"') -and
    @($BBIMEPolicyHeaders | Where-Object {
        [IO.Path]::IsPathRooted($_) -and (Test-Path -LiteralPath $_ -PathType Leaf) -and
        [IO.Path]::GetFullPath($_) -eq [IO.Path]::GetFullPath((Join-Path $root 'src/focusstate.h'))
    }).Count -eq 1 -and
    @($BBIMEPolicyHeaders | Where-Object { (Split-Path -Leaf $_) -eq 'backend.h' }).Count -eq 0) 'Shared module exports only the pure focus policy; manual pause and actual owner checks remain intact'
Assert-Contract ($mainKey.IndexOf('reconcileMainFocus()') -lt $mainKey.IndexOf('if (!imeEnabled())') -and
    $nativeKey.IndexOf('reconcileNativeFocus()') -lt $nativeKey.IndexOf('!m_nativeModule->enabled()') -and
    $nativeKey.Contains('focusedNativeEditor() != editor') -and
    $backend.Contains('QTimer::singleShot(0, this, SLOT(reconcileMainFocus()))') -and
    $backend.Contains('QTimer::singleShot(0, this, SLOT(reconcileNativeFocus()))')) 'First keys reconcile actual focus; deferred blur coalesces field transfers'
Assert-Contract ($active.Contains('app->scene() && app->isFullscreen() && app->isAwake()') -and
    $inactive.Contains('m_active = false') -and $inactive.Contains('reconcileMainFocus()') -and
    $inactive.Contains('reconcileNativeFocus()')) 'Lifecycle requires actual installed foreground awake scene and revokes both routes'
Assert-Contract ($mainKey.Contains('if (symbolKey)') -and $mainKey -notmatch 'cycleSymbols\(' -and
    $nativeKey.Contains('if (isSymEvent(event))') -and
    $nativeKey.IndexOf('if (isSymEvent(event))') -lt $nativeKey.IndexOf('m_nativeModule->handleKey') -and
    $symbolCycle -notmatch 'refreshSymbols\(|m_symbolsVisible\s*=\s*true' -and
    $symbolChoose.Contains('return false;')) 'BBIME main/native Sym events and public symbol selection do not open or insert'
Assert-Contract ($candidateAt.Contains('generation != m_candidateGeneration') -and
    $candidateAt.IndexOf('generation != m_candidateGeneration') -lt $candidateAt.IndexOf('return chooseCandidate(index)') -and
    $qml.Contains('ListItem.view.pendingGeneration = ListItemData.generation') -and
    $qml.Contains('backend.chooseCandidateAt(touchedIndex, touchedGeneration)') -and
    $backend.Contains('row["generation"] = uint(m_candidateGeneration)')) 'Candidate touch captures original generation and C++ rejects stale clicks'
Assert-Contract ($qml.IndexOf('backend.recordLayout("footer"') -lt $qml.IndexOf('id: candidates') -and
    $qml.Contains('visible: backend.imeEnabled') -and
    $qml.Contains('verticalAlignment: VerticalAlignment.Bottom') -and
    $qml.Contains('bottomPadding: 0') -and $nativePage.Contains('bottomPadding: 0') -and
    $strip.Contains('visible: ime && ime.enabled') -and
    $strip.Contains('verticalAlignment: VerticalAlignment.Bottom') -and
    $strip.Contains('preferredHeight: 72') -and $strip.Contains('minHeight: 72') -and
    $strip.Contains('maxHeight: 72')) 'Candidate docks follow the footer, reserve a fixed active row and hide while paused'
$layoutSave = Get-CppBody $backend 'void Backend::saveLayout()'
Assert-Contract ($layoutSave.Contains('{"header", "modes", "editor", "composition", "footer", "candidates"}') -and
    $layoutSave.Contains('bool complete = available > 0') -and $layoutSave.Contains('bool fits = complete') -and
    $layoutSave.Contains('rect.width() > 0 && rect.height() > 0') -and
    $layoutSave.Contains('complete = complete && measured') -and
    $layoutSave.Contains('name != "candidates" || imeEnabled()') -and
    $layoutSave.Contains('!complete ? "PENDING_MEASUREMENTS" :') -and
    $layoutSave.Contains('fits && (!imeEnabled() || docked) ? "PASS" : "FAIL"') -and
    $layoutSave.Contains('layout.setValue("measurements_complete", complete)')) 'Layout evidence checks footer before candidates and cannot report PASS with incomplete participating measurements'
$q10Validation = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'tools/Test-Q10Module.ps1')
Assert-Contract ($q10Validation.Contains('[string]$ConnectionPath = ''''') -and
    $q10Validation.Contains('$env:Q10DEPLOY_CONFIG') -and
    $q10Validation.Contains('Join-Path $env:LOCALAPPDATA ''Q10Deploy\config.json''') -and
    $q10Validation.Contains('[switch]$LegacyAlgorithms') -and
    $q10Validation.Contains('if ($LegacyAlgorithms -or $config.LegacyAlgorithms)') -and
    $q10Validation.Contains('StrictHostKeyChecking=yes') -and
    $q10Validation.Contains('UserKnownHostsFile=') -and
    $q10Validation.Contains('$config.DeviceHost -ne ''192.168.1.61''') -and
    $q10Validation.Contains('$config.SshUser -ne ''root''') -and
    $q10Validation.Contains('native-module-arm-validation-0.1.0.15.json')) 'Q10 validation resolves configured paths and optional legacy algorithms while retaining device pin and strict host keys (source only; no device connection)'
Write-Output 'Source contracts only; SDK focus delivery, rendering and physical keys still require runtime tests. Legacy Sym research is excluded from default product capability evidence.'
