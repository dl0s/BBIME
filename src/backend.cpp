#include "backend.h"
#include "textpositions.h"
#include "symbols.h"
#include <bb/cascades/Application>
#include <bb/cascades/KeyEvent>
#include <bb/cascades/TextEditor>
#include <bb/cascades/TextAreaInputMode>
#include <bb/system/Clipboard>
#include <bb/system/Screenshot>
#include <bb/cascades/Window>
#include <QUrl>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QElapsedTimer>
#include <QTemporaryFile>
#include <sys/keycodes.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unistd.h>

using namespace bb::cascades;
static QString fromUtf8(const std::string &s) { return QString::fromUtf8(s.c_str()); }
// Q10 bb35 keymap: HID 0x71 (F22) maps to QNX symbol 0xf0d3.
static const int symKey = KEYCODE_F1 + 21;
using bbime::symbolKeys;
using bbime::symbolSlots;
using bbime::symbolCodes;

Backend::Backend(QObject *parent) : QObject(parent),
    m_decoder(m_inputService.legacyDecoder()),
    m_nativeModule(new bbime::NativeController(m_inputService, this)),
    m_nativePageOpen(false),
    m_moduleSettings(QDir::currentPath() + "/data/module-settings.ini"),
    m_candidateModel(new ArrayDataModel(this)), m_symbolModel(new ArrayDataModel(this)),
    m_mode("natural"),
    m_lastChineseMode("natural"),
    m_page(0), m_highlight(0), m_selectionAnchor(-1),
    m_selectionCursor(0), m_symbolGroup(0), m_symbolCycleStart(-1),
    m_imeEnabled(true), m_ready(false), m_active(true),
    m_loading(false), m_editing(false), m_testing(false), m_symbolsVisible(false),
    m_cursorCodePoints(true),
    m_sampleCount(0), m_leftShiftSeen(0), m_rightShiftSeen(0), m_shiftMoves(0),
    m_shiftCursorMoves(0), m_altSeen(0), m_symSeen(0), m_symbolCycles(0) {
    if (!m_moduleSettings.load() && m_moduleSettings.error() != "NOT_FOUND")
        m_settingsStatus = QString::fromUtf8("配置读取失败，保留默认值：") + m_moduleSettings.error();
    m_mode = defaultMode();
    m_lastChineseMode = QString::fromStdString(m_moduleSettings.profile().chineseMode);
    m_saveTimer.setSingleShot(true);
    connect(&m_saveTimer, SIGNAL(timeout()), this, SLOT(saveDraft()));
    m_metricsTimer.setSingleShot(true);
    m_metricsTimer.setInterval(750);
    connect(&m_metricsTimer, SIGNAL(timeout()), this, SLOT(writeMetrics()));
    m_layoutTimer.setSingleShot(true);
    connect(&m_layoutTimer, SIGNAL(timeout()), this, SLOT(saveLayout()));
    Application *app = Application::instance();
    connect(app, SIGNAL(thumbnail()), this, SLOT(inactive()));
    connect(app, SIGNAL(invisible()), this, SLOT(inactive()));
    connect(app, SIGNAL(asleep()), this, SLOT(inactive()));
    connect(app, SIGNAL(fullscreen()), this, SLOT(active()));
    connect(app, SIGNAL(awake()), this, SLOT(active()));
    m_status = QString::fromUtf8("正在载入词库");
    m_latency = QString::fromUtf8("解码 P95 --");
    refreshSymbols();
    QTimer::singleShot(0, this, SLOT(initialize()));
}
Backend::~Backend() {
    delete m_nativeModule;
    saveDraft();
}

void Backend::initialize() {
    QDir().mkpath("data");
    const bool settingsPass = bbime::ModuleSettings::selftest();
    std::fprintf(stderr, "BBIME: SETTINGS=%s\n", settingsPass ? "PASS" : "FAIL");
    QString dictionary = QDir::currentPath() + "/app/native/assets/dict_pinyin.dat";
    if (!QFile::exists(dictionary)) dictionary = "assets/dict_pinyin.dat";
    m_ready = m_inputService.open(dictionary.toLocal8Bit().constData(),
        (QDir::currentPath() + "/data/userdict.dat").toLocal8Bit().constData());
    if (!m_ready) {
        m_status = QString::fromUtf8("词库载入失败");
        std::fprintf(stderr, "BBIME: dictionary load failed\n");
        emit changed();
        return;
    }
    const bool moduleReady = m_nativeModule->initialize();
    const bool modulePass = moduleReady && m_nativeModule->selftest();
    std::fprintf(stderr, "BBIME: NATIVE_MODULE=%s\n", modulePass ? "PASS" : "FAIL");
    runDiagnostics();
    if (!m_ready) return;
    std::fprintf(stderr, "BBIME: READY mapped_syllables=%u\n",
                 unsigned(m_decoder.mappedSyllables()));
    emit changed();
    if (QFile::exists("data/layout-capture.once"))
        QTimer::singleShot(1800, this, SLOT(captureLayout()));
}

void Backend::captureLayout() {
    QFile::remove("data/layout-capture.once");
    if (m_nativePageOpen || !m_active || !m_editor ||
        !m_editor->text().isEmpty() || !m_code.isEmpty()) return;
    bb::system::Screenshot screenshot;
    const QString file = screenshot.captureWindow(
        QUrl::fromLocalFile(QDir::currentPath() + "/data/layout.png"),
        Application::instance()->mainWindow()->handle());
    std::fprintf(stderr, "BBIME: own_window_capture=%s error=%d\n",
        file.isEmpty() ? "FAIL" : "PASS", int(screenshot.error()));
}

void Backend::attachEditor(QObject *object) {
    m_editor = qobject_cast<TextArea *>(object);
    if (!m_editor) return;
    m_loading = true;
    QFile file("data/draft.txt");
    if (file.open(QIODevice::ReadOnly) && file.size() <= 131072)
        m_editor->setText(QString::fromUtf8(file.readAll()));
    m_loading = false;
    connect(m_editor, SIGNAL(textChanged(QString)), this, SLOT(documentChanged()));
    connect(m_editor->editor(), SIGNAL(cursorPositionChanged(int)),
            this, SLOT(editorPositionChanged()));
    connect(m_editor->editor(), SIGNAL(selectionStartChanged(int)),
            this, SLOT(editorPositionChanged()));
    connect(m_editor->editor(), SIGNAL(selectionEndChanged(int)),
            this, SLOT(editorPositionChanged()));
    m_editor->setInputMode(TextAreaInputMode::Custom);
}

void Backend::documentChanged() {
    if (!m_loading) {
        m_saveTimer.start(750);
    }
    if (!m_editing && !m_code.isEmpty()) cancel();
}
void Backend::editorPositionChanged() {
    if (!m_editing) {
        m_selectionAnchor = -1;
        if (!m_code.isEmpty()) cancel();
    }
}
void Backend::saveDraft() {
    if (m_testing || !m_editor) return;
    QDir().mkpath("data");
    QFile file("data/draft.txt.tmp");
    if (!file.open(QIODevice::WriteOnly)) return;
    const QByteArray text = m_editor->text().toUtf8();
    if (file.write(text) != text.size() || !file.flush() || ::fsync(file.handle()) != 0) {
        std::fprintf(stderr, "BBIME: draft_save=WRITE_FAIL\n");
        file.close();
        return;
    }
    file.close();
    // POSIX rename atomically replaces this private draft without a delete gap.
    if (std::rename("data/draft.txt.tmp", "data/draft.txt") != 0)
        std::fprintf(stderr, "BBIME: draft_save=RENAME_FAIL\n");
    writeMetrics();
}
void Backend::writeMetrics() {
    if (m_testing) return;
    QSettings metrics("data/typing-diagnostics.ini", QSettings::IniFormat);
    metrics.setValue("decoder_samples", m_sampleCount);
    metrics.setValue("decoder_p95_ms", percentile(95));
    metrics.setValue("window_samples", m_samples.size());
    metrics.setValue("physical_left_shift_presses", m_leftShiftSeen);
    metrics.setValue("physical_right_shift_presses", m_rightShiftSeen);
    metrics.setValue("standalone_shift_candidate_moves", m_shiftMoves);
    metrics.setValue("standalone_shift_cursor_moves", m_shiftCursorMoves);
    metrics.setValue("physical_alt_presses", m_altSeen);
    metrics.setValue("physical_sym_presses", m_symSeen);
    metrics.setValue("symbol_group_cycles", m_symbolCycles);
    metrics.setValue("decoder_ready", m_ready);
    metrics.setValue("cursor_index_unit", m_cursorCodePoints ? "code_points" : "utf16");
    metrics.setValue("ime_enabled", imeEnabled());
    metrics.setValue("input_mode", m_mode);
    metrics.remove("typing_active");
    metrics.sync();
}
void Backend::scheduleMetrics() {
    if (!m_testing) m_metricsTimer.start();
}
void Backend::recordLayout(const QString &name, double x, double y,
                           double width, double height) {
    if (m_testing || width <= 0 || height <= 0) return;
    m_layout[name] = QRectF(x, y, width, height);
    m_layoutTimer.start(150);
}
void Backend::saveLayout() {
    QSettings layout("data/ui-layout.ini", QSettings::IniFormat);
    const double available = m_layout.value("content").height();
    bool fits = available > 0;
    const char *names[] = {"header", "modes", "editor", "composition", "candidates", "footer"};
    layout.remove("first");
    layout.remove("second");
    layout.remove("typing_active");
    double previousBottom = 0;
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        const QString name = names[i];
        const QRectF rect = m_layout.value(name);
        const bool rowFits = rect.height() > 0 && rect.y() >= previousBottom - 1 &&
            rect.bottom() <= available + 1;
        fits = fits && rowFits;
        previousBottom = rect.bottom();
        layout.setValue(name + "/x", rect.x());
        layout.setValue(name + "/y", rect.y());
        layout.setValue(name + "/width", rect.width());
        layout.setValue(name + "/height", rect.height());
    }
    layout.setValue("available_height", available);
    layout.setValue("action_bar_policy", "manual_custom_ime_gate");
    layout.setValue("ime_enabled", imeEnabled());
    layout.setValue("action_bar_visible", !imeEnabled());
    layout.setValue("nonoverlapping_rows_fit", fits);
    layout.sync();
}
void Backend::inactive() {
    m_active = false;
    m_pressed.clear();
    m_shiftPending.clear();
    closeSymbols();
    cancel();
    if (!m_testing) m_decoder.flush();
    saveDraft();
}
void Backend::active() {
    if (!m_active) {
        m_pressed.clear();
        m_shiftPending.clear();
    }
    m_active = true;
}

QString Backend::pinyin() const { return fromUtf8(m_decoder.pinyin()); }
void Backend::refreshCandidates() {
    if (m_testing) return;
    QVariantList rows;
    for (size_t i = 0; i < m_decoder.candidates().size(); ++i) {
        QVariantMap row;
        row["text"] = fromUtf8(m_decoder.candidates()[i].text);
        rows << row;
    }
    m_candidateModel->clear();
    m_candidateModel->append(rows);
    emit candidatesChanged();
    emit highlightChanged();
}
void Backend::refreshSymbols() {
    if (m_testing) return;
    QVariantList rows;
    for (int cell = 0; cell < 30; ++cell) {
        const int index = symbolSlots[cell];
        QVariantMap row;
        row["symbolIndex"] = index;
        row["text"] = index < 0 ? QString() : QString(QChar(symbolCodes[m_symbolGroup][index]));
        row["key"] = index < 0 ? QString() : QString(QChar(symbolKeys[index]));
        rows << row;
    }
    m_symbolModel->clear();
    m_symbolModel->append(rows);
}
void Backend::setSymbolGroup(int group) {
    if (!imeEnabled() || group < 0 || group > 2 || group == m_symbolGroup) return;
    m_symbolGroup = group;
    refreshSymbols();
    if (!m_testing) emit symbolsChanged();
}
void Backend::cycleSymbols() {
    if (!m_active || !imeEnabled()) return;
    if (m_symbolsVisible) {
        const int next = (m_symbolGroup + 1) % 3;
        if (next == m_symbolCycleStart) {
            closeSymbols();
            return;
        }
        setSymbolGroup(next);
        if (!m_testing) {
            ++m_symbolCycles;
            scheduleMetrics();
        }
        return;
    }
    m_symbolCycleStart = m_mode == "english" ? 1 : 0;
    m_symbolsVisible = true;
    setSymbolGroup(m_symbolCycleStart);
    if (!m_testing) emit symbolsChanged();
}
void Backend::closeSymbols() {
    if (!m_symbolsVisible) return;
    m_symbolsVisible = false;
    m_symbolCycleStart = -1;
    if (!m_testing) emit symbolsChanged();
    restoreFocus();
}
bool Backend::chooseSymbol(int index) {
    if (!m_active || !imeEnabled() || index < 0 || index >= 26 || !m_symbolsVisible)
        return false;
    const QString text(QChar(symbolCodes[m_symbolGroup][index]));
    if (!m_code.isEmpty()) {
        if (!m_decoder.candidates().empty() && !chooseCandidate(m_highlight)) return false;
        if (!m_code.isEmpty() && !literalComposition()) return false;
    }
    if (!insert(text)) return false;
    closeSymbols();
    return true;
}
QString Backend::pageLabel() const {
    int count = int(m_decoder.candidates().size());
    return count ? QString("%1 / %2").arg(m_page + 1).arg((count + 4) / 5) : "";
}

void Backend::setMode(const QString &mode) {
    if (mode != "natural" && mode != "full" && mode != "english")
        return;
    if (mode == m_mode) return;
    if (!imeEnabled()) return;
    if (!literalComposition()) return;
    closeSymbols();
    m_shiftPending.clear();
    m_mode = mode;
    if (mode == "natural" || mode == "full") m_lastChineseMode = mode;
    if (!m_testing) emit changed();
    restoreFocus();
}
void Backend::setImeEnabled(bool enabled) {
    if (enabled) m_nativeModule->suspend();
    if (enabled == imeEnabled()) return;
    m_imeEnabled = enabled;
    m_pressed.clear();
    m_shiftPending.clear();
    m_selectionAnchor = -1;
    m_selectionCursor = 0;
    m_symbolsVisible = false;
    m_symbolCycleStart = -1;
    // Discard preedit on both transitions; never insert unfinished letters.
    cancel();
    if (!m_testing) {
        emit symbolsChanged();
        scheduleMetrics();
        m_layoutTimer.start(150);
    }
    restoreFocus();
}
void Backend::toggleIme() {
    setImeEnabled(!imeEnabled());
}
void Backend::disableIme() {
    setImeEnabled(false);
}
bool Backend::startNativeModule() {
    disableIme();
    m_nativeModule->suspend();
    if (!m_ready || !m_nativeModule->ready() ||
        !m_nativeModule->configure(m_moduleSettings.profile(), learnSelections())) return false;
    m_nativeModule->setMode(m_mode);
    m_nativeModule->setEnabled(true);
    m_nativePageOpen = true;
    return true;
}
void Backend::stopNativeModule() {
    m_nativeModule->suspend();
    m_nativePageOpen = false;
    restoreFocus();
}
void Backend::toggleLanguage() {
    if (!imeEnabled()) return;
    setMode(m_mode == "english" ? m_lastChineseMode : "english");
}
bool Backend::settingsWritable() const {
    return m_active && !m_testing && !imeEnabled() && m_code.isEmpty() &&
        !m_symbolsVisible && m_pressed.isEmpty() && !m_nativeModule->enabled();
}
void Backend::saveModuleProfile(const bbime::ModuleProfile &profile, bool learn) {
    if (!settingsWritable()) return;
    m_settingsStatus = m_moduleSettings.save(profile, learn) ?
        QString::fromUtf8("设置已保存") :
        QString::fromUtf8("保存失败，设置未改变：") + m_moduleSettings.error();
    emit settingsChanged();
}
void Backend::setDefaultMode(const QString &mode) {
    bbime::ModuleProfile profile = m_moduleSettings.profile();
    profile.startMode = mode.toStdString();
    if (mode == "natural" || mode == "full") profile.chineseMode = profile.startMode;
    saveModuleProfile(profile, learnSelections());
}
void Backend::setShiftCandidates(bool enabled) {
    bbime::ModuleProfile profile = m_moduleSettings.profile();
    profile.shiftCandidates = enabled;
    saveModuleProfile(profile, learnSelections());
}
void Backend::setShiftCursor(bool enabled) {
    bbime::ModuleProfile profile = m_moduleSettings.profile();
    profile.shiftCursor = enabled;
    saveModuleProfile(profile, learnSelections());
}
void Backend::setChinesePunctuation(bool enabled) {
    bbime::ModuleProfile profile = m_moduleSettings.profile();
    profile.chinesePunctuation = enabled;
    saveModuleProfile(profile, learnSelections());
}
void Backend::setLearnSelections(bool enabled) {
    saveModuleProfile(m_moduleSettings.profile(), enabled);
}
void Backend::publishSettings() {
    if (!settingsWritable()) return;
    const QString path = bbime::ModuleSettings::sharedPath(true);
    if (path.isEmpty()) m_settingsStatus = QString::fromUtf8("共享目录不可用，请检查共享文件权限");
    else m_settingsStatus = m_moduleSettings.publish(path) ?
        QString::fromUtf8("共享设置已发布") :
        QString::fromUtf8("发布失败：") + m_moduleSettings.error();
    emit settingsChanged();
}
void Backend::importSettings() {
    if (!settingsWritable()) return;
    const QString path = bbime::ModuleSettings::sharedPath(false);
    if (path.isEmpty()) m_settingsStatus = QString::fromUtf8("共享目录不可用或尚未发布");
    else m_settingsStatus = m_moduleSettings.importProfile(path) ?
        QString::fromUtf8("共享设置已读取") :
        QString::fromUtf8("读取失败，本地设置未改变：") + m_moduleSettings.error();
    emit settingsChanged();
}
void Backend::restoreFocus() {
    if (m_editor && m_active && !m_testing && !m_symbolsVisible) m_editor->requestFocus();
}

void Backend::remember() {
    if (!m_editor) return;
    Snapshot state;
    state.text = m_editor->text();
    state.cursor = m_editor->editor()->cursorPosition();
    state.start = m_editor->editor()->selectionStart();
    state.end = m_editor->editor()->selectionEnd();
    m_undo.append(state);
    while (m_undo.size() > 24) m_undo.removeFirst();
}
void Backend::undo() {
    cancel();
    if (!m_editor || m_undo.isEmpty()) return;
    Snapshot state = m_undo.takeLast();
    m_editor->setText(state.text);
    m_editor->editor()->setSelection(state.start, state.end);
    if (state.start == state.end) m_editor->editor()->setCursorPosition(state.cursor);
}
bool Backend::insert(const QString &text) {
    if (!m_editor) return false;
    if (text.isEmpty()) return true;
    TextEditor *editor = m_editor->editor();
    const QString document = m_editor->text();
    const int replaced = utf16Offset(document, editor->selectionEnd()) -
        utf16Offset(document, editor->selectionStart());
    if (document.size() - replaced + text.size() > 16384) {
        m_status = QString::fromUtf8("文本已达测试长度上限");
        if (!m_testing) emit changed();
        return false;
    }
    remember();
    m_editing = true;
    editor->insertPlainText(text);
    m_editing = false;
    m_selectionAnchor = -1;
    return true;
}
void Backend::deleteBackward() {
    if (!m_editor) return;
    TextEditor *editor = m_editor->editor();
    const QString text = m_editor->text();
    int start = utf16Offset(text, editor->selectionStart());
    int end = utf16Offset(text, editor->selectionEnd());
    if (start == end) {
        end = utf16Offset(text, editor->cursorPosition());
        if (end <= 0) return;
        start = end - 1;
        if (text[start].isLowSurrogate() && start > 0 &&
            text[start - 1].isHighSurrogate()) --start;
    }
    remember();
    editor->setSelection(editorOffset(text, start), editorOffset(text, end));
    editor->insertPlainText("");
}
void Backend::moveCursor(int delta, bool selection) {
    if (!m_editor) return;
    if (!m_code.isEmpty()) cancel();
    TextEditor *editor = m_editor->editor();
    const QString text = m_editor->text();
    int current = utf16Offset(text, editor->cursorPosition());
    if (selection) {
        if (m_selectionAnchor < 0) {
            m_selectionAnchor = utf16Offset(text, editor->selectionStart());
            m_selectionCursor = current;
        }
        current = m_selectionCursor;
    }
    int target = qBound(0, current + delta, text.size());
    if (target > 0 && target < text.size() && text[target].isLowSurrogate())
        target = qBound(0, target + (delta < 0 ? -1 : 1), text.size());
    if (selection) {
        m_selectionCursor = target;
        m_editing = true;
        editor->setSelection(editorOffset(text, qMin(m_selectionAnchor, target)),
                             editorOffset(text, qMax(m_selectionAnchor, target)));
        m_editing = false;
    } else {
        int position = editorOffset(text, target);
        if (editor->selectionStart() != editor->selectionEnd())
            position = delta < 0 ? editor->selectionStart() : editor->selectionEnd();
        editor->setSelection(position, position);
    }
}
int Backend::utf16Offset(const QString &text, int position) const {
    return bbime::utf16Offset(text.utf16(), text.size(), position, m_cursorCodePoints);
}
int Backend::editorOffset(const QString &text, int position) const {
    return bbime::editorOffset(text.utf16(), text.size(), position, m_cursorCodePoints);
}

void Backend::measure(double ms) {
    m_samples.append(ms);
    if (m_samples.size() > 512) m_samples.remove(0);
    ++m_sampleCount;
    if (m_sampleCount <= 5 || m_sampleCount % 16 == 0) {
        m_latency = QString::fromUtf8("解码 P95 %1 ms").arg(percentile(95), 0, 'f', 2);
        if (!m_testing) emit metricsChanged();
    }
}
double Backend::percentile(int percent) const {
    if (m_samples.isEmpty()) return 0;
    QVector<double> sorted = m_samples;
    std::sort(sorted.begin(), sorted.end());
    int index = (sorted.size() * percent + 99) / 100 - 1;
    return sorted[qBound(0, index, sorted.size() - 1)];
}
void Backend::updateCode(const QString &code) {
    if (!m_ready || !imeEnabled()) return;
    QElapsedTimer timer;
    timer.start();
    bool accepted = m_decoder.setCode(code.toLatin1().constData(), m_mode != "full");
    const double ms = timer.nsecsElapsed() / 1000000.0;
    if (accepted) {
        m_code = code;
        m_page = 0;
        m_highlight = 0;
        m_status = !code.isEmpty() && m_decoder.candidates().empty() ?
            QString::fromUtf8("无匹配") : QString();
        if (!code.isEmpty()) measure(ms);
        refreshCandidates();
        scheduleMetrics();
    } else m_status = QString::fromUtf8("编码已达长度上限");
    if (!m_testing) emit changed();
}
void Backend::cancel() {
    m_code.clear();
    m_page = 0;
    m_highlight = 0;
    if (m_ready) m_decoder.setCode("", m_mode != "full");
    m_status.clear();
    refreshCandidates();
    if (!m_testing) emit changed();
}
bool Backend::literalComposition() {
    if (!m_code.isEmpty() && !insert(m_code)) return false;
    cancel();
    return true;
}
bool Backend::choose(int slot) {
    if (slot < 0 || slot >= 5) return false;
    return chooseCandidate(m_page * 5 + slot);
}
bool Backend::chooseCandidate(int index) {
    if (!m_active || !imeEnabled() || index < 0 ||
        index >= int(m_decoder.candidates().size())) return false;
    const bbime::Candidate chosen = m_decoder.candidates()[index];
    if (!insert(fromUtf8(chosen.text))) return false;
    m_decoder.choose(index, !m_testing && learnSelections());
    QString rest = m_code.mid(int(chosen.consumed));
    updateCode(rest);
    restoreFocus();
    return true;
}
void Backend::page(int direction) {
    if (!m_active || !imeEnabled()) return;
    const int pages = (int(m_decoder.candidates().size()) + 4) / 5;
    if (pages > 0) m_page = qBound(0, m_page + direction, pages - 1);
    m_highlight = m_page * 5;
    if (!m_testing) emit highlightChanged();
    if (!m_testing) emit changed();
    restoreFocus();
}
void Backend::moveCandidate(int direction) {
    if (!m_active || !imeEnabled()) return;
    const int count = int(m_decoder.candidates().size());
    if (!count) return;
    m_highlight = qBound(0, m_highlight + direction, count - 1);
    m_page = m_highlight / 5;
    if (!m_testing) emit highlightChanged();
    if (!m_testing) emit changed();
    restoreFocus();
}
void Backend::copy() {
    if (!m_editor) return;
    TextEditor *editor = m_editor->editor();
    const QString document = m_editor->text();
    const int start = utf16Offset(document, editor->selectionStart());
    const int end = utf16Offset(document, editor->selectionEnd());
    QString text = end > start ? document.mid(start, end - start) : document;
    bb::system::Clipboard clipboard;
    clipboard.insert("text/plain", text.toUtf8());
    restoreFocus();
}
void Backend::clear() {
    cancel();
    if (m_editor) {
        remember();
        m_editor->setText("");
    }
    restoreFocus();
}

bool Backend::handleKey(QObject *object) {
    KeyEvent *event = qobject_cast<KeyEvent *>(object);
    if (!event || !m_active) return false;
    if (!imeEnabled()) return false;
    const int identity = event->keycap() ? event->keycap() : event->key();
    const bool leftShift = identity == KEYCODE_LEFT_SHIFT || identity == Qt::Key_Shift;
    const bool rightShift = identity == KEYCODE_RIGHT_SHIFT;
    if (leftShift || rightShift) {
        if (event->isPressed()) {
            if (!m_pressed.contains(identity)) {
                const bool alone = m_shiftPending.isEmpty();
                if (!alone)
                    for (QHash<int, ShiftAction>::iterator it = m_shiftPending.begin();
                         it != m_shiftPending.end(); ++it) it.value() = NoShiftAction;
                ShiftAction action = NoShiftAction;
                if (alone && !m_symbolsVisible && !event->isAltPressed() && !event->isCtrlPressed()) {
                    if (m_code.isEmpty() && shiftCursor()) action = CursorShiftAction;
                    else if (!m_code.isEmpty() && shiftCandidates() &&
                        (m_mode == "natural" || m_mode == "full")) action = CandidateShiftAction;
                }
                m_shiftPending[identity] = action;
                if (!m_testing) {
                    if (leftShift) ++m_leftShiftSeen;
                    else ++m_rightShiftSeen;
                    scheduleMetrics();
                }
            }
            m_pressed[identity] = event->duration();
        } else {
            const ShiftAction action = m_shiftPending.take(identity);
            m_pressed.remove(identity);
            const bool shortAlone = event->duration() <= 500 && !m_symbolsVisible &&
                !event->isAltPressed() && !event->isCtrlPressed();
            if (shortAlone && action == CandidateShiftAction && !m_code.isEmpty() &&
                (m_mode == "natural" || m_mode == "full")) {
                moveCandidate(leftShift ? -1 : 1);
                if (!m_testing) {
                    ++m_shiftMoves;
                    scheduleMetrics();
                }
            } else if (shortAlone && action == CursorShiftAction && m_code.isEmpty() && m_editor) {
                const int previousCursor = m_editor->editor()->cursorPosition();
                moveCursor(leftShift ? -1 : 1, false);
                restoreFocus();
                if (!m_testing && previousCursor != m_editor->editor()->cursorPosition()) {
                    ++m_shiftCursorMoves;
                    scheduleMetrics();
                }
            }
        }
        event->accept();
        return true;
    }
    if (event->isPressed()) {
        for (QHash<int, ShiftAction>::iterator it = m_shiftPending.begin();
             it != m_shiftPending.end(); ++it) it.value() = NoShiftAction;
    }
    if (!event->isPressed()) {
        m_pressed.remove(identity);
        event->accept();
        return true;
    }
    int physical = event->keycap() ? event->keycap() : event->key();
    const int logical = event->key();
    const bool enter = physical == KEYCODE_RETURN || physical == KEYCODE_KP_ENTER ||
        physical == 13 || physical == 10 || physical == Qt::Key_Return;
    const bool backspace = physical == KEYCODE_BACKSPACE || physical == 8 ||
        physical == Qt::Key_Backspace;
    const bool escape = physical == KEYCODE_ESCAPE || physical == 27 ||
        physical == Qt::Key_Escape;
    QString unicode = event->unicode();
    if (unicode.isEmpty() && logical >= 32 && logical < 127) unicode = QChar(logical);
    if (physical >= 'A' && physical <= 'Z') physical += 'a' - 'A';
    const bool repeatable = !event->isCtrlPressed() && !event->isAltPressed() &&
        ((backspace && !event->isShiftPressed()) ||
         (physical >= 'a' && physical <= 'z'));
    if (m_pressed.contains(identity) &&
        (!repeatable || event->duration() <= m_pressed[identity])) {
        event->accept();
        return true;
    }
    m_pressed[identity] = event->duration();
    event->accept();

    const bool altKey = identity == KEYCODE_LEFT_ALT || identity == KEYCODE_RIGHT_ALT ||
        identity == Qt::Key_Alt || identity == Qt::Key_AltGr;
    const bool symbolKey = identity == symKey || identity == Qt::Key_F22 ||
        logical == symKey || logical == Qt::Key_F22;
    if (altKey) {
        if (!m_testing) { ++m_altSeen; scheduleMetrics(); }
        return true;
    }
    if (symbolKey) {
        if (!m_testing) { ++m_symSeen; scheduleMetrics(); }
        cycleSymbols();
        return true;
    }
    if (identity == KEYCODE_LEFT_CTRL || identity == KEYCODE_RIGHT_CTRL ||
        identity == KEYCODE_CAPS_LOCK || identity == Qt::Key_Control ||
        identity == Qt::Key_CapsLock) return true;
    if (m_symbolsVisible) {
        if (escape || backspace) closeSymbols();
        else if (!event->isAltPressed() && !event->isCtrlPressed()) {
            const char *key = physical >= 'a' && physical <= 'z' ?
                std::strchr(symbolKeys, physical) : 0;
            if (key) chooseSymbol(int(key - symbolKeys));
        }
        return true;
    }
    if (enter && event->isAltPressed()) {
        toggleLanguage();
        return true;
    }
    if (event->isCtrlPressed()) {
        if (physical == 'a' && m_editor) {
            cancel();
            m_editor->editor()->setSelection(0,
                editorOffset(m_editor->text(), m_editor->text().size()));
        }
        else if (physical == 'c') copy();
        else if (physical == 'v') {
            if (!literalComposition()) return true;
            bb::system::Clipboard clipboard;
            insert(QString::fromUtf8(clipboard.value("text/plain")));
        } else if (physical == 'z') undo();
        return true;
    }
    if (escape || (backspace && (event->isAltPressed() || event->isShiftPressed()))) {
        cancel();
        return true;
    }
    if (backspace) {
        if (!m_code.isEmpty()) updateCode(m_code.left(m_code.size() - 1));
        else deleteBackward();
        return true;
    }
    if (enter) {
        if (event->isShiftPressed()) {
            if (!m_code.isEmpty()) {
                if (!m_decoder.candidates().empty()) {
                    if (!choose(selectedSlot())) return true;
                } else if (!literalComposition()) return true;
                if (!literalComposition()) return true;
            }
            insert("\n");
        } else if (!m_code.isEmpty()) literalComposition();
        else insert("\n");
        return true;
    }
    if (physical == KEYCODE_LEFT || physical == Qt::Key_Left) {
        moveCursor(-1, event->isShiftPressed());
        return true;
    }
    if (physical == KEYCODE_RIGHT || physical == Qt::Key_Right) {
        moveCursor(1, event->isShiftPressed());
        return true;
    }
    if (!m_ready && m_mode != "english") return true;
    if (!m_code.isEmpty() && unicode.size() == 1 &&
        unicode[0] >= QChar('1') && unicode[0] <= QChar('5')) {
        choose(unicode[0].unicode() - '1');
        return true;
    }
    if (physical == 32 || unicode == " ") {
        if (!m_code.isEmpty()) {
            if (event->isShiftPressed()) page(hasNext() ? 1 : -m_page);
            else if (!m_decoder.candidates().empty()) choose(selectedSlot());
            else literalComposition();
        } else insert(" ");
        return true;
    }
    if (!m_code.isEmpty() && (unicode == "[" || unicode == "]")) {
        page(unicode == "[" ? -1 : 1);
        return true;
    }
    if (m_mode != "english" && !event->isAltPressed() &&
        physical >= 'a' && physical <= 'z') {
        updateCode(m_code + QChar(physical));
        return true;
    }
    if (m_mode == "full" && unicode == "'") {
        updateCode(m_code + "'");
        return true;
    }
    if (unicode.isEmpty() || unicode[0].unicode() < 32 ||
        unicode[0].category() == QChar::Other_PrivateUse) return true;
    if (!m_code.isEmpty()) {
        if (!m_decoder.candidates().empty() && !choose(selectedSlot())) return true;
        if (!m_code.isEmpty() && !literalComposition()) return true;
    }
    if (m_mode == "english" && !event->isAltPressed() &&
        physical >= 'a' && physical <= 'z')
        unicode = QChar(event->isShiftPressed() ? physical - ('a' - 'A') : physical);
    if (m_mode != "english" && !event->isAltPressed() && chinesePunctuation()) {
        if (unicode == ",") unicode = QChar(0xff0c);
        if (unicode == ".") unicode = QChar(0x3002);
        if (unicode == "?") unicode = QChar(0xff1f);
        if (unicode == "!") unicode = QChar(0xff01);
        if (unicode == ":") unicode = QChar(0xff1a);
        if (unicode == ";") unicode = QChar(0xff1b);
    }
    insert(unicode);
    return true;
}

static void testKey(Backend *backend, int cap, int key, bool alt = false,
                    bool shift = false, bool ctrl = false, bool release = true,
                    int duration = 0) {
    KeyEvent down(cap, key, true, alt, shift, ctrl, duration);
    backend->handleKey(&down);
    if (release) {
        KeyEvent up(cap, key, false, alt, shift, ctrl, duration);
        backend->handleKey(&up);
    }
}
static void testLetters(Backend *backend, const char *code) {
    for (const char *p = code; *p; ++p) testKey(backend, *p, *p);
}
static bool testIgnoredKey(Backend *backend, int cap, int key, bool alt = false,
                           bool shift = false, bool ctrl = false) {
    KeyEvent down(cap, key, true, alt, shift, ctrl, 0);
    KeyEvent up(cap, key, false, alt, shift, ctrl, 150);
    down.ignore();
    up.ignore();
    const bool handledDown = backend->handleKey(&down);
    const bool handledUp = backend->handleKey(&up);
    return !handledDown && !handledUp && !down.isAccepted() && !up.isAccepted();
}
static bool testCursorCheckpoint(TextArea *area, const char *name, int expected,
                                 bool collapsed = false) {
    TextEditor *editor = area->editor();
    const bool pass = editor->cursorPosition() == expected &&
        (!collapsed || editor->selectionStart() == editor->selectionEnd());
    if (!pass)
        std::fprintf(stderr, "BBIME: CURSOR_CHECK %s expected=%d actual=%d start=%d end=%d\n",
            name, expected, editor->cursorPosition(), editor->selectionStart(),
            editor->selectionEnd());
    return pass;
}
bool Backend::inputSelftest() {
    const bbime::ModuleSettings originalSettings = m_moduleSettings;
    QTemporaryFile settingsFixture(QDir::currentPath() + "/data/.bbime-key-settings-XXXXXX");
    if (!settingsFixture.open()) return false;
    settingsFixture.close();
    m_moduleSettings = bbime::ModuleSettings(settingsFixture.fileName());
    QPointer<TextArea> originalEditor = m_editor;
    const QString originalMode = m_mode, originalChinese = m_lastChineseMode;
    const QString originalLatency = m_latency;
    const QHash<int, int> originalPressed = m_pressed;
    const QHash<int, ShiftAction> originalShifts = m_shiftPending;
    const QList<Snapshot> originalUndo = m_undo;
    const QVector<double> originalSamples = m_samples;
    const unsigned originalCount = m_sampleCount;
    const bool originalActive = m_active;
    const bool originalImeEnabled = m_imeEnabled;
    const bool originalSymbols = m_symbolsVisible;
    const int originalSymbolGroup = m_symbolGroup;
    const int originalCycleStart = m_symbolCycleStart;
    const int originalAnchor = m_selectionAnchor, originalCursor = m_selectionCursor;
    TextArea *scratch = new TextArea();
    scratch->setInputMode(TextAreaInputMode::Custom);
    scratch->setMaximumLength(16384);
    m_testing = true;
    const QString probe = QString::fromUtf8("A\xf0\x9f\x98\x80" "B");
    scratch->setText(probe);
    scratch->editor()->setCursorPosition(probe.size());
    const int probeEnd = scratch->editor()->cursorPosition();
    const bool cursorUnitsPass = probeEnd == 3 || probeEnd == 4;
    m_cursorCodePoints = probeEnd == 3;
    std::fprintf(stderr, "BBIME: CURSOR_UNITS=%s probe_end=%d\n",
        cursorUnitsPass ? (m_cursorCodePoints ? "CODE_POINTS" : "UTF16") : "UNKNOWN", probeEnd);
    scratch->setText("");
    m_active = true;
    m_mode = "natural";
    m_imeEnabled = true;
    m_lastChineseMode = "natural";
    m_editor = scratch;
    m_symbolsVisible = false;
    m_symbolCycleStart = -1;
    m_pressed.clear();
    m_shiftPending.clear();
    m_undo.clear();
    cancel();
    testLetters(this, "nihk");
    testKey(this, ' ', ' ', false, false, false, false);
    bool pass = scratch->text() == QString::fromUtf8("\xe4\xbd\xa0\xe5\xa5\xbd");
    testKey(this, ' ', ' ', false, false, false, false, 300);
    pass = pass && scratch->text().size() == 2;
    KeyEvent spaceUp(' ', ' ', false, false, false, false, 300);
    handleKey(&spaceUp);
    testLetters(this, "ni");
    testKey(this, KEYCODE_BACKSPACE, 8);
    pass = pass && m_code == "n";
    testKey(this, KEYCODE_BACKSPACE, 8);
    pass = pass && m_code.isEmpty() && scratch->text().size() == 2;
    testKey(this, KEYCODE_RETURN, 13, true, false, false, false);
    testKey(this, KEYCODE_RETURN, 13, true, false, false, false, 300);
    pass = pass && m_mode == "english";
    KeyEvent enterUp(KEYCODE_RETURN, 13, false, true, false, false, 300);
    handleKey(&enterUp);
    testKey(this, 'a', 'A', false, true);
    pass = pass && scratch->text().endsWith("A");
    testKey(this, 'a', 'a', false, false, true);
    testKey(this, 'x', 'x');
    pass = pass && scratch->text() == "x";
    testKey(this, KEYCODE_RETURN, 13, true);
    testLetters(this, "vsgo");
    testKey(this, 'w', '1', true);
    pass = pass && scratch->text() == QString::fromUtf8("x\xe4\xb8\xad\xe5\x9b\xbd");
    scratch->setText(QString(16384, 'x'));
    scratch->editor()->setCursorPosition(16384);
    testLetters(this, "ni");
    pass = pass && !choose(0) && m_code == "ni" && scratch->text().size() == 16384;
    scratch->editor()->setSelection(0, 16384);
    pass = pass && choose(0) && m_code.isEmpty() && scratch->text().size() == 1;
    testLetters(this, "nihk");
    testKey(this, KEYCODE_RETURN, 13);
    pass = pass && scratch->text().endsWith("nihk") && m_code.isEmpty();
    testLetters(this, "ni");
    pass = pass && selectedSlot() == 0 && m_decoder.candidates().size() > 5;
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = pass && selectedSlot() == 1 && m_page == 0;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT);
    pass = pass && selectedSlot() == 0;
    for (int i = 0; i < 5; ++i)
        testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = pass && m_page == 1 && selectedSlot() == 0;
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT, false, false, false, false);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT, false, false, false, false, 300);
    KeyEvent longShiftUp(KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT,
                         false, false, false, false, 600);
    handleKey(&longShiftUp);
    pass = pass && m_page == 1 && selectedSlot() == 0;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    testKey(this, ' ', ' ', false, true);
    KeyEvent shiftUp(KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT,
                     false, false, false, false, 150);
    handleKey(&shiftUp);
    pass = pass && m_page == 2 && selectedSlot() == 0;
    const QString expected = fromUtf8(m_decoder.candidates()[m_highlight].text);
    const QString before = scratch->text();
    testKey(this, ' ', ' ');
    pass = pass && scratch->text() == before + expected && m_code.isEmpty();
    testLetters(this, "ni");
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    testKey(this, 'h', 'H', false, true);
    handleKey(&shiftUp);
    pass = pass && m_code == "nih" && selectedSlot() == 0;
    cancel();
    const bool legacyPass = pass;
    pass = true;
    // Global candidate selection must not depend on the five-slot keyboard page.
    testLetters(this, "ni");
    const int tapIndex = 7;
    const QString tapped = fromUtf8(m_decoder.candidates()[tapIndex].text);
    const QString beforeTap = scratch->text();
    pass = pass && !chooseCandidate(-1) && !chooseCandidate(1000);
    const bool tapCommitted = chooseCandidate(tapIndex);
    pass = pass && tapCommitted && scratch->text() == beforeTap + tapped;
    testLetters(this, "ni");
    moveCandidate(7);
    pass = pass && selectedIndex() == 7 && candidatePage() == 1;
    testKey(this, KEYCODE_BACKSPACE, 8);
    pass = pass && selectedIndex() == 0 && candidatePage() == 0 && m_code == "n";
    const bool stripPass = pass;
    pass = true;
    const QString beforeAlt = scratch->text(), beforeAltCode = m_code;
    testKey(this, KEYCODE_LEFT_ALT, KEYCODE_LEFT_ALT, true);
    testKey(this, KEYCODE_RIGHT_ALT, KEYCODE_RIGHT_ALT, true);
    testKey(this, Qt::Key_Alt, Qt::Key_Alt, true);
    testKey(this, KEYCODE_MENU, KEYCODE_MENU);
    pass = pass && scratch->text() == beforeAlt && m_code == beforeAltCode;
    const bool altPass = pass;
    pass = true;
    cancel();
    testKey(this, symKey, symKey, false, false, false, false);
    testKey(this, symKey, symKey, false, false, false, false, 300);
    pass = pass && m_symbolsVisible && m_symbolGroup == 0 && m_code.isEmpty();
    KeyEvent symUp(symKey, symKey, false, false, false, false, 300);
    handleKey(&symUp);
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 1;
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 2;
    testKey(this, symKey, symKey, false, false, false, false);
    pass = pass && !m_symbolsVisible && m_symbolCycleStart == -1 &&
        scratch->text() == beforeAlt && m_code.isEmpty() && m_mode == "natural";
    testKey(this, symKey, symKey, false, false, false, false, 300);
    pass = pass && !m_symbolsVisible;
    handleKey(&symUp);
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 0;
    testKey(this, KEYCODE_BACKSPACE, 8);
    pass = pass && !m_symbolsVisible && scratch->text() == beforeAlt;
    testLetters(this, "nihk");
    const QString beforeSymbol = scratch->text();
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_code == "nihk" &&
        !chooseSymbol(-1) && !chooseSymbol(26);
    const int beforeSymbolCursor = scratch->editor()->cursorPosition();
    for (int group = 1; group <= 2; ++group) {
        testKey(this, symKey, symKey);
        pass = pass && m_symbolsVisible && m_symbolGroup == group &&
            m_code == "nihk" && scratch->text() == beforeSymbol &&
            scratch->editor()->cursorPosition() == beforeSymbolCursor && m_mode == "natural";
    }
    testKey(this, symKey, symKey);
    pass = pass && !m_symbolsVisible && m_symbolCycleStart == -1 &&
        m_code == "nihk" && scratch->text() == beforeSymbol &&
        scratch->editor()->cursorPosition() == beforeSymbolCursor && m_mode == "natural";
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 0;
    testKey(this, 'q', 'q');
    pass = pass && !m_symbolsVisible && m_code.isEmpty() &&
        scratch->text() == beforeSymbol + QString::fromUtf8("\xe4\xbd\xa0\xe5\xa5\xbd") + QChar(0xff0c);
    setMode("english");
    const QString beforeEnglishSymbols = scratch->text();
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 1;
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 2;
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 0;
    testKey(this, symKey, symKey);
    pass = pass && !m_symbolsVisible && m_symbolCycleStart == -1 &&
        m_mode == "english" && scratch->text() == beforeEnglishSymbols;
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 1;
    pass = pass && chooseSymbol(symbolSlots[25]) && scratch->text().endsWith("\\");
    testKey(this, symKey, symKey);
    testKey(this, symKey, symKey);
    pass = pass && m_symbolsVisible && m_symbolGroup == 2;
    pass = pass && chooseSymbol(1) && scratch->text().endsWith(QChar(0x00d7));
    testKey(this, symKey, symKey);
    testKey(this, KEYCODE_ESCAPE, 27);
    pass = pass && !m_symbolsVisible;
    scratch->setText(QString(16384, 'x'));
    scratch->editor()->setCursorPosition(16384);
    testKey(this, symKey, symKey);
    pass = pass && !chooseSymbol(0) && m_symbolsVisible && scratch->text().size() == 16384;
    closeSymbols();
    const bool symbolsPass = pass;
    pass = true;
    QString keyboardRows[3];
    bool seenSymbols[26] = {false};
    int symbolCount = 0;
    for (int cell = 0; cell < 30; ++cell) {
        const int index = symbolSlots[cell];
        if (index < 0) keyboardRows[cell / 10] += " ";
        else {
            const bool valid = index < 26 && !seenSymbols[index];
            pass = pass && valid;
            if (valid) {
                seenSymbols[index] = true;
                ++symbolCount;
                keyboardRows[cell / 10] += QChar(symbolKeys[index]);
            }
        }
    }
    const bool gridPass = pass && symbolCount == 26 &&
        keyboardRows[0] == "qwertyuiop" && keyboardRows[1] == "asdfghjkl " &&
        keyboardRows[2] == " zxcvbnm  ";
    pass = true;
    setMode("natural");
    const QString cursorText = QString::fromUtf8("A\xe4\xb8\xad\xf0\x9f\x98\x80" "B");
    scratch->setText(cursorText);
    scratch->editor()->setCursorPosition(0);
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT);
    pass = testCursorCheckpoint(scratch, "left_bound", 0) && pass;
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "right_ascii", 1) && pass;
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "right_chinese", 2) && pass;
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "right_surrogate", editorOffset(cursorText, 4)) && pass;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT);
    pass = testCursorCheckpoint(scratch, "left_surrogate", 2) && pass;
    const int cursorEnd = editorOffset(cursorText, cursorText.size());
    scratch->editor()->setCursorPosition(cursorEnd);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "right_bound", cursorEnd) && pass;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT);
    pass = testCursorCheckpoint(scratch, "left_from_bound", editorOffset(cursorText, 4)) && pass;
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "back_to_bound", cursorEnd) && pass;
    scratch->editor()->setSelection(1, editorOffset(cursorText, 4));
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT);
    pass = testCursorCheckpoint(scratch, "collapse_left", 1, true) && pass;
    scratch->editor()->setSelection(1, editorOffset(cursorText, 4));
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "collapse_right", editorOffset(cursorText, 4), true) && pass;
    scratch->editor()->setCursorPosition(2);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT, false, false, false, false);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT, false, false, false, false, 300);
    handleKey(&longShiftUp);
    pass = testCursorCheckpoint(scratch, "long_hold", 2) && pass;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT, false, false, false, false);
    handleKey(&shiftUp);
    KeyEvent rightShiftUp(KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT, false, false, false, false, 150);
    handleKey(&rightShiftUp);
    pass = testCursorCheckpoint(scratch, "both_shifts", 2) && pass;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, true);
    pass = testCursorCheckpoint(scratch, "alt_shift", 2) && pass &&
        scratch->text() == cursorText;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    testLetters(this, "ni");
    handleKey(&shiftUp);
    pass = testCursorCheckpoint(scratch, "composition_started", 2) && pass &&
        m_code == "ni" && selectedIndex() == 0;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    cancel();
    handleKey(&shiftUp);
    pass = testCursorCheckpoint(scratch, "composition_cancelled", 2) && pass;
    setMode("english");
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "english_shift", editorOffset(cursorText, 4)) && pass;
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    testKey(this, 'a', 'A', false, true);
    const int afterCapital = scratch->editor()->cursorPosition();
    handleKey(&shiftUp);
    pass = testCursorCheckpoint(scratch, "capital_combination", afterCapital) && pass;
    const QString beforePopupCursor = scratch->text();
    testKey(this, symKey, symKey);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "symbol_popup", afterCapital) && pass && m_symbolsVisible &&
        scratch->text() == beforePopupCursor;
    closeSymbols();
    const bool cursorPass = pass;
    pass = true;
    scratch->setText(cursorText);
    scratch->editor()->setCursorPosition(editorOffset(cursorText, 4));
    testKey(this, KEYCODE_BACKSPACE, 8);
    pass = pass && scratch->text() == QString::fromUtf8("A\xe4\xb8\xad" "B") &&
        scratch->editor()->cursorPosition() == 2;
    testKey(this, 'a', 'a', false, false, true);
    pass = pass && scratch->editor()->selectionStart() == 0 &&
        scratch->editor()->selectionEnd() == editorOffset(scratch->text(), scratch->text().size());
    const QString longText = QString(16380, 'x') +
        QString::fromUtf8("\xf0\x9f\x98\x80" "YZ");
    scratch->setText(longText);
    scratch->editor()->setSelection(editorOffset(longText, 16380),
                                    editorOffset(longText, 16382));
    const bool replacedEmoji = insert(QString::fromUtf8("\xe5\xa5\xbd\xe5\x95\x8a"));
    pass = pass && replacedEmoji && scratch->text().size() == 16384 &&
        scratch->text().endsWith(QString::fromUtf8("\xe5\xa5\xbd\xe5\x95\x8a" "YZ"));
    const bool positionsPass = pass;
    scratch->setText(beforePopupCursor);
    scratch->editor()->setSelection(afterCapital, afterCapital);
    pass = true;
    setImeEnabled(false);
    pass = testIgnoredKey(this, 'a', 'a') && pass;
    pass = testIgnoredKey(this, symKey, symKey) && pass;
    pass = testIgnoredKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT) && pass &&
        !m_symbolsVisible && scratch->editor()->cursorPosition() == afterCapital &&
        scratch->inputMode() == TextAreaInputMode::Custom;
    const bool pausedPass = pass;
    pass = true;
    setImeEnabled(true);
    setMode("natural");
    scratch->setText(cursorText);
    scratch->editor()->setSelection(1, editorOffset(cursorText, 4));
    testLetters(this, "ni");
    testKey(this, KEYCODE_LEFT_SHIFT, KEYCODE_LEFT_SHIFT, false, false, false, false);
    cycleSymbols();
    m_selectionAnchor = 1;
    const int gateCursor = scratch->editor()->cursorPosition();
    const int gateStart = scratch->editor()->selectionStart();
    const int gateEnd = scratch->editor()->selectionEnd();
    toggleIme();
    pass = pass && !imeEnabled() && m_mode == "natural" &&
        scratch->inputMode() == TextAreaInputMode::Custom && scratch->text() == cursorText &&
        scratch->editor()->cursorPosition() == gateCursor &&
        scratch->editor()->selectionStart() == gateStart &&
        scratch->editor()->selectionEnd() == gateEnd &&
        m_code.isEmpty() && m_decoder.candidates().empty() && !m_symbolsVisible &&
        m_pressed.isEmpty() && m_shiftPending.isEmpty() && m_selectionAnchor == -1;
    const int pausedCaps[] = {'a', ' ', KEYCODE_BACKSPACE, KEYCODE_RETURN, KEYCODE_ESCAPE,
        KEYCODE_LEFT, KEYCODE_RIGHT, KEYCODE_LEFT_SHIFT, KEYCODE_RIGHT_SHIFT,
        KEYCODE_LEFT_ALT, KEYCODE_RIGHT_ALT, symKey};
    for (unsigned i = 0; i < sizeof(pausedCaps) / sizeof(pausedCaps[0]); ++i) {
        const bool untouched = testIgnoredKey(this, pausedCaps[i], pausedCaps[i]);
        pass = untouched && pass;
    }
    pass = testIgnoredKey(this, KEYCODE_RETURN, 13, true) && pass;
    pass = testIgnoredKey(this, 'w', '1', true) && pass;
    pass = testIgnoredKey(this, 'a', 'A', false, true) && pass;
    const char *ctrlKeys = "acvz";
    for (const char *p = ctrlKeys; *p; ++p)
        pass = testIgnoredKey(this, *p, *p, false, false, true) && pass;
    handleKey(&shiftUp);
    toggleLanguage();
    setMode("english");
    setMode("full");
    setMode("natural");
    setMode("system");
    cycleSymbols();
    setSymbolGroup(2);
    pass = !choose(0) && !chooseCandidate(0) && !chooseSymbol(0) && pass;
    page(1);
    moveCandidate(1);
    pass = pass && !imeEnabled() && m_mode == "natural" &&
        scratch->text() == cursorText && scratch->editor()->cursorPosition() == gateCursor &&
        scratch->editor()->selectionStart() == gateStart &&
        scratch->editor()->selectionEnd() == gateEnd &&
        m_code.isEmpty() && !m_symbolsVisible && m_pressed.isEmpty() &&
        m_shiftPending.isEmpty() && m_page == 0 && m_highlight == 0;
    // Simulate delayed candidate/symbol callbacks while the gate is closed.
    m_decoder.setCode("ni", true);
    m_symbolsVisible = true;
    m_page = 1;
    m_highlight = 6;
    pass = !chooseCandidate(0) && !chooseSymbol(0) && pass;
    page(-1);
    moveCandidate(-1);
    pass = pass && m_page == 1 && m_highlight == 6 && scratch->text() == cursorText;
    m_symbolsVisible = false;
    cancel();
    scratch->editor()->setSelection(cursorEnd, cursorEnd);
    const QString pausedText = scratch->text();
    const int pausedCursor = scratch->editor()->cursorPosition();
    testKey(this, 'n', 'n');
    pass = pass && scratch->text() == pausedText &&
        scratch->editor()->cursorPosition() == pausedCursor && m_code.isEmpty();
    toggleIme();
    handleKey(&shiftUp);
    handleKey(&rightShiftUp);
    pass = pass && imeEnabled() && m_mode == "natural" &&
        scratch->inputMode() == TextAreaInputMode::Custom &&
        scratch->text() == pausedText && scratch->editor()->cursorPosition() == pausedCursor &&
        m_code.isEmpty() && !m_symbolsVisible && m_shiftPending.isEmpty();
    testLetters(this, "ni");
    const QString gateCandidate = m_decoder.candidates().empty() ? QString() :
        fromUtf8(m_decoder.candidates()[0].text);
    testKey(this, ' ', ' ');
    pass = pass && !gateCandidate.isEmpty() && scratch->text() == pausedText + gateCandidate &&
        m_code.isEmpty();
    setMode("full");
    testLetters(this, "ni");
    disableIme();
    pass = pass && !imeEnabled() && m_code.isEmpty() && m_mode == "full";
    toggleIme();
    pass = pass && imeEnabled() && m_mode == "full";
    setMode("system");
    pass = pass && imeEnabled() && m_mode == "full" &&
        scratch->inputMode() == TextAreaInputMode::Custom;
    setMode("english");
    toggleIme();
    toggleIme();
    pass = pass && imeEnabled() && m_mode == "english";
    const QString beforeRestoredEnglish = scratch->text();
    testKey(this, 'b', 'b');
    pass = pass && scratch->text() == beforeRestoredEnglish + "b";
    setImeEnabled(false);
    inactive();
    active();
    pass = pass && !imeEnabled() && m_mode == "english" &&
        scratch->inputMode() == TextAreaInputMode::Custom;
    const bool gatePass = pass;
    pass = true;
    bbime::ModuleProfile testProfile;
    testProfile.shiftCursor = false;
    testProfile.shiftCandidates = false;
    testProfile.chinesePunctuation = false;
    pass = m_moduleSettings.save(testProfile, false) && pass;
    setImeEnabled(true);
    setMode("natural");
    scratch->setText(cursorText);
    scratch->editor()->setSelection(1, 1);
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = testCursorCheckpoint(scratch, "disabled_shift_cursor", 1, true) && pass;
    testLetters(this, "ni");
    testKey(this, KEYCODE_RIGHT_SHIFT, KEYCODE_RIGHT_SHIFT);
    pass = pass && selectedIndex() == 0 && m_code == "ni";
    cancel();
    testKey(this, ',', ',');
    pass = pass && scratch->text().contains(',') &&
        !scratch->text().contains(QChar(0xff0c)) && !learnSelections();
    setMode("english");
    QString expectedCapital = scratch->text();
    expectedCapital.insert(utf16Offset(expectedCapital, scratch->editor()->cursorPosition()), 'A');
    testKey(this, 'a', 'A', false, true);
    pass = pass && scratch->text() == expectedCapital;
    const bool preferencesPass = pass;
    std::fprintf(stderr, "BBIME: INPUT_EXTENSIONS STRIP=%s ALT=%s SYMBOLS=%s GRID=%s CURSOR=%s POSITIONS=%s PAUSED=%s GATE=%s PREFERENCES=%s\n",
        stripPass ? "PASS" : "FAIL", altPass ? "PASS" : "FAIL",
        symbolsPass ? "PASS" : "FAIL", gridPass ? "PASS" : "FAIL",
        cursorPass ? "PASS" : "FAIL", positionsPass ? "PASS" : "FAIL", pausedPass ? "PASS" : "FAIL",
        gatePass ? "PASS" : "FAIL", preferencesPass ? "PASS" : "FAIL");
    pass = cursorUnitsPass && legacyPass && stripPass && altPass && symbolsPass && gridPass &&
        cursorPass && positionsPass && pausedPass && gatePass && preferencesPass;
    m_editor = originalEditor;
    m_mode = originalMode;
    m_lastChineseMode = originalChinese;
    m_imeEnabled = originalImeEnabled;
    m_pressed = originalPressed;
    m_shiftPending = originalShifts;
    m_undo = originalUndo;
    m_samples = originalSamples;
    m_sampleCount = originalCount;
    m_active = originalActive;
    m_symbolsVisible = originalSymbols;
    m_symbolGroup = originalSymbolGroup;
    m_symbolCycleStart = originalCycleStart;
    m_selectionAnchor = originalAnchor;
    m_selectionCursor = originalCursor;
    m_moduleSettings = originalSettings;
    m_testing = false;
    m_latency = originalLatency;
    emit metricsChanged();
    delete scratch;
    return pass;
}

void Backend::runDiagnostics() {
    if (!m_ready) return;
    m_nativeModule->suspend();
    const QString previous = m_code;
    const int previousPage = m_page;
    const int previousHighlight = m_highlight;
    bool pass = true;
    const char *codes[] = {"nihk", "vsgo", "uijx", "nihkvsgo"};
    const char *expect[] = {"\xe4\xbd\xa0\xe5\xa5\xbd", "\xe4\xb8\xad\xe5\x9b\xbd"};
    for (int c = 0; c < 2; ++c) {
        m_decoder.setCode(codes[c], true);
        bool found = false;
        for (size_t i = 0; i < m_decoder.candidates().size(); ++i)
            if (m_decoder.candidates()[i].text == expect[c]) found = true;
        pass = pass && found;
    }
    QVector<double> values;
    for (int i = 0; i < 40; ++i) {
        QElapsedTimer timer;
        timer.start();
        m_decoder.setCode(codes[i % 4], true);
        values.append(timer.nsecsElapsed() / 1000000.0);
        pass = pass && !m_decoder.candidates().empty();
    }
    std::sort(values.begin(), values.end());
    const bool inputPass = inputSelftest();
    QSettings diagnostic("data/startup-diagnostics.ini", QSettings::IniFormat);
    diagnostic.setValue("version", "0.1.0.11");
    diagnostic.setValue("editor_input_mode", "Custom");
    diagnostic.setValue("system_ime", "NEVER_ENABLED");
    diagnostic.setValue("ime_enabled", imeEnabled());
    diagnostic.setValue("input_mode", m_mode);
    diagnostic.setValue("cursor_index_unit", m_cursorCodePoints ? "code_points" : "utf16");
    diagnostic.setValue("decoder_ready", pass);
    diagnostic.setValue("selftest", pass ? "PASS" : "FAIL");
    diagnostic.setValue("synthetic_key_selftest", inputPass ? "PASS" : "FAIL");
    diagnostic.setValue("synthetic_samples", values.size());
    diagnostic.setValue("synthetic_decoder_p50_ms", values[19]);
    diagnostic.setValue("synthetic_decoder_p95_ms", values[37]);
    diagnostic.setValue("synthetic_decoder_max_ms", values.last());
    diagnostic.setValue("typing_samples", m_sampleCount);
    diagnostic.setValue("typing_decoder_p95_ms", percentile(95));
    diagnostic.setValue("mapped_syllables", unsigned(m_decoder.mappedSyllables()));
    diagnostic.setValue("pointer_bytes", int(sizeof(void *)));
    diagnostic.sync();
    m_decoder.setCode(previous.toLatin1().constData(), m_mode != "full");
    m_code = previous;
    m_page = previousPage;
    m_highlight = previousHighlight;
    refreshCandidates();
    m_status.clear();
    std::fprintf(stderr, "BBIME: SELFTEST=%s KEYS=%s synthetic_decoder_p95_ms=%.3f\n",
                 pass ? "PASS" : "FAIL", inputPass ? "PASS" : "FAIL", values[37]);
    if (!pass) {
        m_ready = false;
        m_status = QString::fromUtf8("词库自检失败");
    } else if (!inputPass) {
        m_status = QString::fromUtf8("交互自检异常，详见诊断");
        std::fprintf(stderr, "BBIME: decoder remains ready; interaction selftest failed\n");
    }
    emit changed();
}
