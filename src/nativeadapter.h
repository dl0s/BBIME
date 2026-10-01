#ifndef BBIME_NATIVEADAPTER_H
#define BBIME_NATIVEADAPTER_H

#include "inputmodule.h"
#include <QObject>
#include <QPointer>
#include <bb/cascades/AbstractTextControl>
#include <bb/cascades/TextEditor>

namespace bbime {
class NativeEditorAdapter : public QObject, public EditorAdapter {
    Q_OBJECT
public:
    NativeEditorAdapter(QObject *control, const FieldPolicy &policy,
                        bool codePointPositions, QObject *parent = 0);
    ~NativeEditorAdapter();
    bool supported() const { return !control_.isNull(); }
    QObject *control() const { return control_.data(); }
    bool focused() const;
    bool takeInput();
    void releaseInput();
    void setPositionUnits(bool codePoints) { codePoints_ = codePoints; }
    bool eligible() const;
    unsigned long revision() const { return revision_; }
    bool replaceSelection(const std::string &text, unsigned long expected);
    bool erasePrevious(unsigned long expected);
    bool moveCursor(int direction, unsigned long expected);
    static bool probePositions(bool &areaCodePoints, bool &fieldCodePoints);
signals:
    void focusChanged(bool focused);
    void invalidated();
    void retired();
private slots:
    void changed();
    void destroyedControl();
private:
    bb::cascades::TextEditor *editor() const;
    int toUtf16(const QString &text, int position) const;
    int fromUtf16(const QString &text, int position) const;
    QPointer<bb::cascades::AbstractTextControl> control_;
    FieldPolicy policy_;
    int originalMode_;
    unsigned long revision_;
    bool leased_, editing_, codePoints_;
};
}
#endif
