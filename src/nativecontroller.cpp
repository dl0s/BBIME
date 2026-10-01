#include "nativecontroller.h"
#include "symbols.h"
#include <bb/cascades/KeyEvent>
#include <bb/cascades/TextArea>
#include <bb/cascades/TextField>
#include <bb/cascades/TextFormat>
#include <bb/cascades/TextFieldInputMode>
#include <bb/cascades/TextAreaInputMode>
#include <bb/cascades/Application>
#include <sys/keycodes.h>
#include <cstring>
#include <QTimer>

using namespace bb::cascades;
namespace bbime {
NativeController::NativeController(ImeService &service, QObject *parent) :
    QObject(parent), service_(service), active_(0), model_(new ArrayDataModel(this)),
    symbols_(new ArrayDataModel(this)),
    mode_("natural"), enabled_(false), positionsReady_(false),
    areaPoints_(true), fieldPoints_(true), learningEnabled_(false),
    symbolsVisible_(false), symbolPanelActive_(false), focusCheckQueued_(false),
    symbolGroup_(0), symbolStart_(-1), symbolRevision_(0) {
    Application *app = Application::instance();
    connect(app, SIGNAL(thumbnail()), this, SLOT(suspend()));
    connect(app, SIGNAL(invisible()), this, SLOT(suspend()));
    connect(app, SIGNAL(asleep()), this, SLOT(suspend()));
}
NativeController::~NativeController() {
    deactivate();
    while (!bindings_.isEmpty()) unregisterEditor(bindings_.begin().key());
}
bool NativeController::initialize() {
    deactivate();
    positionsReady_ = NativeEditorAdapter::probePositions(areaPoints_, fieldPoints_);
    foreach (Binding *binding, bindings_)
        binding->adapter->setPositionUnits(qobject_cast<TextArea *>(binding->control) ?
            areaPoints_ : fieldPoints_);
    emit changed();
    return ready();
}
bool NativeController::configure(const ModuleProfile &profile, bool learningEnabled) {
    if (enabled_ || active_) return false;
    ModuleProfile validated;
    if (!ModuleProfile::parse(profile.values(), validated)) return false;
    profile_ = validated;
    learningEnabled_ = learningEnabled;
    foreach (Binding *binding, bindings_)
        binding->session->setLearningEnabled(learningEnabled_);
    return true;
}
QString NativeController::composition() const {
    return active_ ? QString::fromStdString(active_->session->composition()) : QString();
}
int NativeController::selectedIndex() const {
    return active_ && !active_->session->candidates().empty() ?
        int(active_->session->selectedIndex()) : -1;
}
bool NativeController::registerEditor(QObject *control, const QString &kind, bool learn) {
    if (!control || bindings_.contains(control)) return false;
    FieldPolicy policy;
    if (kind == "text") policy.kind = FieldPolicy::PlainText;
    else if (kind == "search") policy.kind = FieldPolicy::Search;
    else if (kind == "path") policy.kind = FieldPolicy::Path;
    else return false;
    policy.learn = learn;
    policy.multiline = qobject_cast<TextArea *>(control) != 0;
    policy.chinesePunctuation = kind == "text";
    NativeEditorAdapter *adapter = new NativeEditorAdapter(control, policy,
        policy.multiline ? areaPoints_ : fieldPoints_, this);
    if (!adapter->supported() || !adapter->eligible()) { delete adapter; return false; }
    Binding *binding = new Binding;
    binding->control = control;
    binding->adapter = adapter;
    binding->session = new InputSession(service_, *adapter, policy);
    binding->session->setLearningEnabled(learningEnabled_);
    binding->calls = 0;
    binding->retired = false;
    bindings_.insert(control, binding);
    connect(adapter, SIGNAL(focusChanged(bool)), this, SLOT(focused(bool)));
    connect(adapter, SIGNAL(invalidated()), this, SLOT(invalidated()));
    connect(adapter, SIGNAL(retired()), this, SLOT(retired()));
    if (adapter->focused()) activate(binding);
    return true;
}
void NativeController::unregisterEditor(QObject *control) {
    Binding *binding = bindings_.take(control);
    if (!binding) return;
    disconnect(binding->adapter, 0, this, 0);
    if (active_ == binding) deactivate();
    binding->retired = true;
    if (!binding->calls) destroyBinding(binding);
}
void NativeController::destroyBinding(Binding *binding) {
    delete binding->session;
    binding->adapter->releaseInput();
    binding->adapter->deleteLater();
    delete binding;
}
void NativeController::activate(Binding *binding) {
    if (!ready() || !enabled_ || !binding->adapter->focused()) return;
    if (active_ == binding) return;
    BindingCall call(*this, binding);
    deactivate();
    if (binding->retired || !binding->adapter->focused() || !ready() || !enabled_) return;
    if (!binding->adapter->takeInput() || binding->retired ||
        !binding->adapter->focused() || !enabled_) {
        binding->adapter->releaseInput();
        return;
    }
    if (!binding->session->activate()) { binding->adapter->releaseInput(); return; }
    active_ = binding;
    active_->session->setMode(mode_.toStdString());
    refresh();
}
void NativeController::deactivate() {
    symbolsVisible_ = false;
    symbolStart_ = -1;
    symbols_->clear();
    pressed_.clear();
    shifts_.clear();
    if (active_) {
        Binding *binding = active_;
        BindingCall call(*this, binding);
        // Restoring input mode can run host slots synchronously. Detach first
        // so a newly focused binding cannot be retired by this older call.
        active_ = 0;
        binding->session->deactivate();
        binding->adapter->releaseInput();
    }
    refresh();
    emit symbolsChanged();
}
void NativeController::focused(bool value) {
    foreach (Binding *binding, bindings_) {
        if (binding->adapter != sender()) continue;
        if (value) activate(binding);
        else if (active_ == binding && !symbolsVisible_ && !symbolPanelActive_ && !focusCheckQueued_) {
            // Coalesce a field transfer; let the host finish a pending submit
            // before retiring its original editor on this event-loop turn.
            focusCheckQueued_ = true;
            QTimer::singleShot(0, this, SLOT(reconcileActiveFocus()));
        }
        return;
    }
}
void NativeController::reconcileActiveFocus() {
    focusCheckQueued_ = false;
    if (active_ && !active_->adapter->focused() && !symbolsVisible_ && !symbolPanelActive_)
        deactivate();
}
void NativeController::invalidated() {
    if (!active_ || active_->adapter != sender()) return;
    if (!active_->adapter->eligible()) deactivate();
    else {
        symbolsVisible_ = false;
        symbolStart_ = -1;
        active_->session->cancel();
        refresh();
        emit symbolsChanged();
    }
}
void NativeController::retired() {
    foreach (Binding *binding, bindings_) {
        if (binding->adapter == sender()) {
            // Keep an in-flight replacement alive until the host callback returns.
            unregisterEditor(binding->control);
            return;
        }
    }
}
void NativeController::refresh() {
    QVariantList rows;
    if (active_) {
        const std::vector<Candidate> &candidates = active_->session->candidates();
        for (size_t i = 0; i < candidates.size(); ++i) {
            const CandidateTicket ticket = active_->session->ticket(i);
            QVariantMap row;
            row["text"] = QString::fromUtf8(candidates[i].text.c_str());
            row["session"] = uint(ticket.session);
            row["revision"] = uint(ticket.revision);
            row["document"] = uint(ticket.document);
            row["index"] = int(i);
            rows << row;
        }
    }
    if (candidateRows_ != rows) {
        candidateRows_ = rows;
        model_->clear();
        model_->append(rows);
        emit candidatesChanged();
    }
    emit highlightChanged();
    emit changed();
}
void NativeController::setEnabled(bool value) {
    if (enabled_ == value) return;
    enabled_ = value;
    deactivate();
    if (enabled_)
        foreach (Binding *binding, bindings_)
            if (binding->adapter->focused()) { activate(binding); break; }
}
void NativeController::suspend() { setEnabled(false); }
void NativeController::setMode(const QString &mode) {
    if ((mode != "natural" && mode != "english") || mode_ == mode) return;
    mode_ = mode;
    symbolsVisible_ = false;
    symbolStart_ = -1;
    if (active_) active_->session->setMode(mode.toStdString());
    pressed_.clear();
    shifts_.clear();
    refresh();
    emit symbolsChanged();
}
void NativeController::cancel() {
    closeSymbols();
    if (active_) active_->session->cancel();
    refresh();
}
void NativeController::restoreFocus() {
    if (enabled_ && active_ && !symbolsVisible_ && !symbolPanelActive_ && active_->adapter->eligible())
        qobject_cast<AbstractTextControl *>(active_->adapter->control())->requestFocus();
}
void NativeController::refreshSymbols() {
    ++symbolRevision_;
    QVariantList rows;
    for (int slot = 0; slot < 30; ++slot) {
        const int index = symbolSlots[slot];
        QVariantMap row;
        row["text"] = index < 0 ? QString() : QString(QChar(symbolCodes[symbolGroup_][index]));
        row["key"] = index < 0 ? QString() : QString(QChar(symbolKeys[index]));
        row["index"] = index;
        row["group"] = symbolGroup_;
        row["session"] = uint(symbolTicket_.session);
        row["revision"] = uint(symbolTicket_.revision);
        row["document"] = uint(symbolTicket_.document);
        row["panel"] = uint(symbolRevision_);
        rows << row;
    }
    symbols_->clear();
    symbols_->append(rows);
    emit symbolsChanged();
}
void NativeController::cycleSymbols() {
    // Compatibility entry point: the baseline has no custom Sym panel.
}
void NativeController::setSymbolGroup(int group) {
    Q_UNUSED(group);
}
void NativeController::setSymbolPanelActive(bool value) {
    Q_UNUSED(value);
}
void NativeController::closeSymbols() {
    if (!symbolsVisible_) return;
    symbolsVisible_ = false;
    symbolStart_ = -1;
    shifts_.clear();
    emit symbolsChanged();
    // A displayed Dialog still owns focus until its close animation finishes.
    // Direct controller callers without a Dialog retain synchronous restoration.
    if (!symbolPanelActive_) restoreFocus();
}
bool NativeController::chooseSymbol(unsigned long session, unsigned long revision,
                                    unsigned long document, unsigned long panel, int group, int index) {
    Q_UNUSED(session);
    Q_UNUSED(revision);
    Q_UNUSED(document);
    Q_UNUSED(panel);
    Q_UNUSED(group);
    Q_UNUSED(index);
    return false;
}
bool NativeController::handleSymbolKey(QObject *event) {
    Q_UNUSED(event);
    return false;
}
void NativeController::suppressStandaloneShifts() {
    for (QHash<int, bool>::iterator it = shifts_.begin(); it != shifts_.end(); ++it)
        it.value() = false;
}
bool NativeController::chooseCandidate(unsigned long session, unsigned long revision,
                                       unsigned long document, int index) {
    if (!enabled_ || !active_ || !active_->adapter->focused() || index < 0) return false;
    Binding *binding = active_;
    BindingCall call(*this, binding);
    CandidateTicket ticket;
    ticket.session = session;
    ticket.revision = revision;
    ticket.document = document;
    ticket.index = size_t(index);
    const bool result = binding->session->choose(ticket);
    refresh();
    return result;
}
bool NativeController::prepareSubmit(QObject *control) {
    if (!active_ || (control && active_->control != control) || !active_->adapter->eligible() ||
        (!control && !active_->adapter->focused() && !symbolsVisible_ && !symbolPanelActive_))
        return false;
    Binding *binding = active_;
    BindingCall call(*this, binding);
    const bool result = binding->session->commitPending();
    refresh();
    return result;
}
bool NativeController::handleKey(QObject *control, QObject *object) {
    KeyEvent *event = qobject_cast<KeyEvent *>(object);
    Binding *binding = bindings_.value(control, 0);
    if (!event || !ready() || !enabled_ || !binding || active_ != binding ||
        (!binding->adapter->focused() && !symbolsVisible_ && !symbolPanelActive_) ||
        !binding->session->active())
        return false;
    BindingCall call(*this, binding);
    const int identity = event->keycap() ? event->keycap() : event->key();
    if (identity == KEYCODE_F1 + 21 || identity == Qt::Key_F22 ||
        event->key() == KEYCODE_F1 + 21 || event->key() == Qt::Key_F22) {
        if (event->isPressed()) suppressStandaloneShifts();
        event->accept();
        return true;
    }
    if (symbolPanelActive_ && !symbolsVisible_) {
        event->accept();
        return true;
    }
    const bool leftShift = identity == KEYCODE_LEFT_SHIFT || identity == Qt::Key_Shift;
    const bool rightShift = identity == KEYCODE_RIGHT_SHIFT;
    if (leftShift || rightShift) {
        if (event->isPressed()) {
            const bool alone = shifts_.isEmpty() && !event->isAltPressed() && !event->isCtrlPressed();
            if (!shifts_.isEmpty())
                for (QHash<int, bool>::iterator it = shifts_.begin(); it != shifts_.end(); ++it)
                    it.value() = false;
            if (!pressed_.contains(identity)) shifts_[identity] = alone;
            pressed_[identity] = event->duration();
        } else {
            const bool alone = shifts_.take(identity);
            pressed_.remove(identity);
            if (alone && !symbolsVisible_ && event->duration() <= 500 &&
                !event->isAltPressed() && !event->isCtrlPressed()) {
                if (binding->session->composition().empty() && profile_.shiftCursor)
                    binding->session->moveCursor(leftShift ? -1 : 1);
                else if (!binding->session->composition().empty() && profile_.shiftCandidates)
                    binding->session->moveHighlight(leftShift ? -1 : 1);
                emit highlightChanged();
            }
        }
        event->accept();
        return true;
    }
    const bool modifier = identity == KEYCODE_LEFT_ALT || identity == KEYCODE_RIGHT_ALT ||
        identity == KEYCODE_LEFT_CTRL || identity == KEYCODE_RIGHT_CTRL ||
        identity == KEYCODE_CAPS_LOCK || identity == Qt::Key_Alt ||
        identity == Qt::Key_AltGr || identity == Qt::Key_Control ||
        identity == Qt::Key_CapsLock || identity == Qt::Key_Meta;
    if (modifier) {
        for (QHash<int, bool>::iterator it = shifts_.begin(); it != shifts_.end(); ++it)
            it.value() = false;
        if (event->isPressed()) pressed_[identity] = event->duration();
        else pressed_.remove(identity);
        event->accept();
        return true;
    }
    if (!event->isPressed()) {
        if (!pressed_.contains(identity)) return false;
        pressed_.remove(identity);
        event->accept();
        return true;
    }
    for (QHash<int, bool>::iterator it = shifts_.begin(); it != shifts_.end(); ++it)
        it.value() = false;
    int key = identity;
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    const bool enter = key == KEYCODE_RETURN || key == 13 || key == Qt::Key_Return;
    const bool backspace = key == KEYCODE_BACKSPACE || key == 8 || key == Qt::Key_Backspace;
    // Host commands are not IME commands. In particular never switch language
    // on Alt+Enter or consume Ctrl/Alt+Backspace behind the host's router.
    if (event->isCtrlPressed() || (enter && event->isAltPressed()) ||
        (backspace && event->isAltPressed())) return false;
    const bool repeatable = !event->isAltPressed() &&
        (backspace || (key >= 'a' && key <= 'z'));
    if (pressed_.contains(identity) &&
        (!repeatable || event->duration() <= pressed_[identity])) {
        event->accept();
        return true;
    }
    InputSession &session = *binding->session;
    QString unicode = event->unicode();
    if (unicode.isEmpty() && event->key() >= 32 && event->key() < 127)
        unicode = QChar(event->key());
    bool recognized = true;
    const bool escape = key == KEYCODE_ESCAPE || key == Qt::Key_Escape || key == 27;
    if (escape) session.cancel();
    else if (backspace) session.backspace();
    else if (enter) {
        if (event->isShiftPressed() || !session.multiline()) {
            if (session.commitPending() && active_ == binding && !binding->retired &&
                binding->adapter->focused()) emit submitRequested(control);
        } else session.insertLiteral("\n");
    } else if (key == KEYCODE_LEFT || key == Qt::Key_Left) session.moveCursor(-1);
    else if (key == KEYCODE_RIGHT || key == Qt::Key_Right) session.moveCursor(1);
    else if (unicode == " ") {
        if (!session.composition().empty()) {
            if (!session.candidates().empty()) session.choose(session.ticket(session.selectedIndex()));
            else session.commitPending();
        } else session.insertLiteral(" ");
    } else if (event->isAltPressed() && !session.composition().empty() &&
               unicode.size() == 1 && unicode[0] >= QChar('1') && unicode[0] <= QChar('5')) {
        session.choose(session.ticket((session.selectedIndex() / 5) * 5 + unicode[0].unicode() - '1'));
    } else if (session.mode() != "english" && !event->isAltPressed() &&
               key >= 'a' && key <= 'z') session.append(char(key));
    else if (!unicode.isEmpty() && unicode[0].unicode() >= 32 &&
             unicode[0].category() != QChar::Other_PrivateUse &&
             unicode[0].category() != QChar::Other_Control &&
             unicode[0].category() != QChar::Other_Format &&
             unicode[0].category() != QChar::Other_NotAssigned) {
        if (session.mode() == "english" && !event->isAltPressed() && key >= 'a' && key <= 'z')
            unicode = QChar(event->isShiftPressed() ? key - ('a' - 'A') : key);
        if (session.mode() != "english" && !event->isAltPressed() &&
            profile_.chinesePunctuation && session.chinesePunctuation()) {
            if (unicode == ",") unicode = QChar(0xff0c);
            else if (unicode == ".") unicode = QChar(0x3002);
            else if (unicode == "?") unicode = QChar(0xff1f);
            else if (unicode == "!") unicode = QChar(0xff01);
            else if (unicode == ":") unicode = QChar(0xff1a);
            else if (unicode == ";") unicode = QChar(0xff1b);
        }
        session.insertLiteral(unicode.toUtf8().constData());
    } else recognized = false;
    if (!recognized) return false;
    if (active_ == binding && !binding->retired) pressed_[identity] = event->duration();
    event->accept();
    refresh();
    return true;
}

bool NativeController::selftest() {
    if (!ready() || enabled_ || active_) return false;
    deactivate();
    service_.suspend();
    FieldPolicy policy(FieldPolicy::PlainText);
    policy.multiline = true;
    TextArea area;
    TextField field;
    area.setTextFormat(TextFormat::Plain);
    field.setTextFormat(TextFormat::Plain);
    area.setInputMode(TextAreaInputMode::Custom);
    field.setInputMode(TextFieldInputMode::Custom);
    area.setMaximumLength(64);
    field.setMaximumLength(64);
    NativeEditorAdapter a(&area, policy, areaPoints_);
    InputSession first(service_, a, policy);
    policy.multiline = false;
    NativeEditorAdapter b(&field, policy, fieldPoints_);
    InputSession second(service_, b, policy);
    const QString originalMode = mode_;
    bool symbolsDisabled = true;
    for (int repeat = 0; repeat < 100; ++repeat) {
        cycleSymbols();
        setSymbolGroup(repeat % 3);
        setSymbolPanelActive(true);
        symbolsDisabled = !symbolsVisible() && !symbolPanelActive() &&
            symbols_->isEmpty() && !handleSymbolKey(0) &&
            !chooseSymbol(0, 0, 0, 0, repeat % 3, repeat % 26) && symbolsDisabled;
    }
    setMode("english");
    setMode("full");
    setMode("system");
    bool pass = mode_ == "english" && symbolsDisabled;
    setMode("natural");
    setMode("full");
    pass = mode_ == "natural" && pass;
    setMode(originalMode);
    pass = a.eligible() && b.eligible() && a.takeInput() && first.activate() &&
        first.setCode("nihk") && pass;
    const CandidateTicket old = first.ticket(0);
    pass = b.takeInput() && second.activate() && !first.choose(old) &&
        area.text().isEmpty() && first.composition().empty() && pass;
    pass = second.setCode("nihk") && pass;
    size_t greeting = second.candidates().size();
    for (size_t i = 0; i < second.candidates().size(); ++i)
        if (second.candidates()[i].text == "\xe4\xbd\xa0\xe5\xa5\xbd") { greeting = i; break; }
    pass = greeting < second.candidates().size() && second.choose(second.ticket(greeting)) &&
        field.text() == QString::fromUtf8("\xe4\xbd\xa0\xe5\xa5\xbd") && pass;
    second.cancel();
    field.setText("");
    field.setMaximumLength(1);
    pass = second.setCode("nihk") && !second.choose(second.ticket(0)) &&
        field.text().isEmpty() && second.composition() == "nihk" && pass;
    field.setInputMode(TextFieldInputMode::Password);
    pass = !second.choose(second.ticket(0)) && field.text().isEmpty() && pass;
    second.deactivate();
    b.releaseInput();
    pass = field.inputMode() == TextFieldInputMode::Password && pass;
    a.releaseInput();
    pass = area.inputMode() == TextAreaInputMode::Custom && pass;
    pass = a.takeInput() && pass;
    a.releaseInput();
    pass = area.inputMode() == TextAreaInputMode::Custom && pass;
    area.setEditable(false);
    pass = !a.takeInput() && !first.activate() && pass;
    if (!pass) positionsReady_ = false;
    emit changed();
    return pass;
}
}
