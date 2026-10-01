#include "nativeadapter.h"
#include "textpositions.h"
#include <bb/cascades/TextArea>
#include <bb/cascades/TextField>
#include <bb/cascades/TextAreaInputMode>
#include <bb/cascades/TextFieldInputMode>
#include <bb/cascades/TextFormat>

using namespace bb::cascades;
namespace bbime {
NativeEditorAdapter::NativeEditorAdapter(QObject *object, const FieldPolicy &policy,
                                         bool points, QObject *parent) :
    QObject(parent), policy_(policy), originalMode_(0), revision_(0),
    leased_(false), editing_(false), codePoints_(points) {
    TextArea *area = qobject_cast<TextArea *>(object);
    TextField *field = qobject_cast<TextField *>(object);
    if (!area && !field) return;
    control_ = area ? static_cast<AbstractTextControl *>(area) : field;
    connect(control_, SIGNAL(textChanged(QString)), this, SLOT(changed()));
    connect(control_, SIGNAL(focusedChanged(bool)), this, SIGNAL(focusChanged(bool)));
    connect(control_, SIGNAL(enabledChanged(bool)), this, SLOT(changed()));
    connect(control_, SIGNAL(textFormatChanged(bb::cascades::TextFormat::Type)),
            this, SLOT(changed()));
    connect(control_, SIGNAL(destroyed(QObject*)), this, SLOT(destroyedControl()));
    connect(editor(), SIGNAL(cursorPositionChanged(int)), this, SLOT(changed()));
    connect(editor(), SIGNAL(selectionStartChanged(int)), this, SLOT(changed()));
    connect(editor(), SIGNAL(selectionEndChanged(int)), this, SLOT(changed()));
    if (field)
        connect(field, SIGNAL(inputModeChanged(bb::cascades::TextFieldInputMode::Type)),
                this, SLOT(changed()));
    else {
        connect(area, SIGNAL(inputModeChanged(bb::cascades::TextAreaInputMode::Type)),
                this, SLOT(changed()));
        connect(area, SIGNAL(editableChanged(bool)), this, SLOT(changed()));
    }
}
NativeEditorAdapter::~NativeEditorAdapter() { releaseInput(); }
TextEditor *NativeEditorAdapter::editor() const {
    if (TextArea *area = qobject_cast<TextArea *>(control_.data())) return area->editor();
    if (TextField *field = qobject_cast<TextField *>(control_.data())) return field->editor();
    return 0;
}
bool NativeEditorAdapter::focused() const { return control_ && control_->isFocused(); }
bool NativeEditorAdapter::eligible() const {
    if (!policy_.allowed() || !control_ || !control_->isEnabled()) return false;
    if (control_->textFormat() != TextFormat::Plain) return false;
    if (TextField *field = qobject_cast<TextField *>(control_.data())) {
        const TextFieldInputMode::Type mode = field->inputMode();
        return mode == TextFieldInputMode::Default || mode == TextFieldInputMode::Text ||
            mode == TextFieldInputMode::Chat || (leased_ && mode == TextFieldInputMode::Custom);
    }
    TextArea *area = qobject_cast<TextArea *>(control_.data());
    if (!area->isEditable()) return false;
    const TextAreaInputMode::Type mode = area->inputMode();
    return mode == TextAreaInputMode::Default || mode == TextAreaInputMode::Text ||
        (leased_ && mode == TextAreaInputMode::Custom);
}
bool NativeEditorAdapter::takeInput() {
    if (!eligible()) return false;
    if (leased_) return true;
    editing_ = true;
    if (TextField *field = qobject_cast<TextField *>(control_.data())) {
        originalMode_ = int(field->inputMode());
        leased_ = true;
        field->setInputMode(TextFieldInputMode::Custom);
    } else {
        TextArea *area = qobject_cast<TextArea *>(control_.data());
        originalMode_ = int(area->inputMode());
        leased_ = true;
        area->setInputMode(TextAreaInputMode::Custom);
    }
    editing_ = false;
    return leased_ && eligible();
}
void NativeEditorAdapter::releaseInput() {
    if (!leased_) return;
    leased_ = false;
    editing_ = true;
    if (TextField *field = qobject_cast<TextField *>(control_.data())) {
        if (field->inputMode() == TextFieldInputMode::Custom)
            field->setInputMode(static_cast<TextFieldInputMode::Type>(originalMode_));
    } else if (TextArea *area = qobject_cast<TextArea *>(control_.data())) {
        if (area->inputMode() == TextAreaInputMode::Custom)
            area->setInputMode(static_cast<TextAreaInputMode::Type>(originalMode_));
    }
    editing_ = false;
}
void NativeEditorAdapter::changed() {
    ++revision_;
    if (!editing_) emit invalidated();
}
void NativeEditorAdapter::destroyedControl() {
    control_ = 0;
    leased_ = false;
    ++revision_;
    emit retired();
}
int NativeEditorAdapter::toUtf16(const QString &text, int position) const {
    return utf16Offset(text.utf16(), text.size(), position, codePoints_);
}
int NativeEditorAdapter::fromUtf16(const QString &text, int position) const {
    return editorOffset(text.utf16(), text.size(), position, codePoints_);
}
bool NativeEditorAdapter::replaceSelection(const std::string &utf8, unsigned long expected) {
    if (!leased_ || !eligible() || revision_ != expected) return false;
    const QByteArray bytes(utf8.data(), int(utf8.size()));
    const QString replacement = QString::fromUtf8(bytes.constData(), bytes.size());
    if (replacement.toUtf8() != bytes ||
        (!policy_.multiline && (replacement.contains('\n') || replacement.contains('\r'))))
        return false;
    TextEditor *edit = editor();
    const QString document = control_->text();
    const int start = toUtf16(document, edit->selectionStart());
    const int end = toUtf16(document, edit->selectionEnd());
    const TextField *field = qobject_cast<TextField *>(control_.data());
    const int maximum = field ? field->maximumLength() :
        qobject_cast<TextArea *>(control_.data())->maximumLength();
    if (maximum >= 0 && document.size() - (end - start) + replacement.size() > maximum)
        return false;
    const QString expectedText = document.left(start) + replacement + document.mid(end);
    editing_ = true;
    edit->insertPlainText(replacement);
    editing_ = false;
    // No setText fallback: that could overwrite a host update or its undo.
    return control_ && control_->text() == expectedText;
}
bool NativeEditorAdapter::erasePrevious(unsigned long expected) {
    if (!leased_ || !eligible() || revision_ != expected) return false;
    QPointer<TextEditor> edit = editor();
    const QString document = control_->text();
    int start = toUtf16(document, edit->selectionStart());
    int end = toUtf16(document, edit->selectionEnd());
    if (start == end) {
        end = toUtf16(document, edit->cursorPosition());
        if (!end) return true;
        start = end - 1;
        if (document[start].isLowSurrogate() && start > 0 &&
            document[start - 1].isHighSurrogate()) --start;
    }
    editing_ = true;
    edit->setSelection(fromUtf16(document, start), fromUtf16(document, end));
    if (!control_ || !edit || !leased_ || !eligible() || control_->text() != document ||
        toUtf16(document, edit->selectionStart()) != start ||
        toUtf16(document, edit->selectionEnd()) != end) {
        editing_ = false;
        return false;
    }
    edit->insertPlainText("");
    editing_ = false;
    return control_ && control_->text() == document.left(start) + document.mid(end);
}
bool NativeEditorAdapter::moveCursor(int direction, unsigned long expected) {
    if (!leased_ || !eligible() || revision_ != expected) return false;
    TextEditor *edit = editor();
    const QString document = control_->text();
    int target = toUtf16(document, edit->cursorPosition());
    if (edit->selectionStart() != edit->selectionEnd())
        target = toUtf16(document, direction < 0 ? edit->selectionStart() : edit->selectionEnd());
    else {
        target = qBound(0, target + (direction < 0 ? -1 : 1), document.size());
        if (target > 0 && target < document.size() && document[target].isLowSurrogate())
            target += direction < 0 ? -1 : 1;
    }
    editing_ = true;
    edit->setSelection(fromUtf16(document, target), fromUtf16(document, target));
    editing_ = false;
    return true;
}
bool NativeEditorAdapter::probePositions(bool &areaPoints, bool &fieldPoints) {
    const QString probe = QString::fromUtf8("A\xf0\x9f\x98\x80" "B");
    TextArea area;
    TextField field;
    area.setText(probe);
    field.setText(probe);
    area.editor()->setCursorPosition(probe.size());
    field.editor()->setCursorPosition(probe.size());
    const int areaEnd = area.editor()->cursorPosition();
    const int fieldEnd = field.editor()->cursorPosition();
    if ((areaEnd != 3 && areaEnd != 4) || (fieldEnd != 3 && fieldEnd != 4)) return false;
    areaPoints = areaEnd == 3;
    fieldPoints = fieldEnd == 3;
    return true;
}
}
