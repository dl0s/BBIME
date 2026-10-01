#ifndef BBIME_NATIVECONTROLLER_H
#define BBIME_NATIVECONTROLLER_H

#include "nativeadapter.h"
#include "moduleprofile.h"
#include <QHash>
#include <QString>
#include <bb/cascades/ArrayDataModel>

namespace bbime {
class NativeController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool composing READ composing NOTIFY changed)
    Q_PROPERTY(QString composition READ composition NOTIFY changed)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY highlightChanged)
    Q_PROPERTY(bb::cascades::ArrayDataModel* candidateModel READ candidateModel CONSTANT)
    Q_PROPERTY(bool symbolsVisible READ symbolsVisible NOTIFY symbolsChanged)
    Q_PROPERTY(int symbolGroup READ symbolGroup WRITE setSymbolGroup NOTIFY symbolsChanged)
    Q_PROPERTY(bb::cascades::ArrayDataModel* symbolModel READ symbolModel CONSTANT)
public:
    explicit NativeController(ImeService &service, QObject *parent = 0);
    ~NativeController();
    bool initialize();
    bool selftest();
    bool configure(const ModuleProfile &profile, bool learningEnabled);
    bool ready() const { return positionsReady_ && service_.ready(); }
    bool enabled() const { return enabled_; }
    bool composing() const { return active_ && !active_->session->composition().empty(); }
    QString composition() const;
    QString mode() const { return mode_; }
    int selectedIndex() const;
    bb::cascades::ArrayDataModel *candidateModel() const { return model_; }
    bb::cascades::ArrayDataModel *symbolModel() const { return symbols_; }
    bool symbolsVisible() const { return symbolsVisible_; }
    int symbolGroup() const { return symbolGroup_; }
    Q_INVOKABLE bool registerEditor(QObject *editor, const QString &policy, bool learn = false);
    Q_INVOKABLE void unregisterEditor(QObject *editor);
    Q_INVOKABLE bool handleKey(QObject *editor, QObject *event);
    Q_INVOKABLE bool chooseCandidate(unsigned long session, unsigned long revision,
                                    unsigned long document, int index);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool prepareSubmit(QObject *editor = 0);
    Q_INVOKABLE void cycleSymbols();
    Q_INVOKABLE void closeSymbols();
    Q_INVOKABLE void restoreFocus();
    Q_INVOKABLE bool handleSymbolKey(QObject *event);
    Q_INVOKABLE bool chooseSymbol(unsigned long session, unsigned long revision,
                                 unsigned long document, unsigned long panel, int group, int index);
public slots:
    void suspend();
    void setEnabled(bool enabled);
    void setMode(const QString &mode);
    void setSymbolGroup(int group);
private slots:
    void focused(bool value);
    void invalidated();
    void retired();
signals:
    void changed();
    void highlightChanged();
    void candidatesChanged();
    void submitRequested(QObject *editor);
    void symbolsChanged();
private:
    struct Binding {
        QObject *control;
        NativeEditorAdapter *adapter;
        InputSession *session;
        int calls;
        bool retired;
    };
    class BindingCall {
    public:
        BindingCall(NativeController &owner, Binding *binding) :
            owner_(owner), binding_(binding) { ++binding_->calls; }
        ~BindingCall() {
            if (--binding_->calls == 0 && binding_->retired) owner_.destroyBinding(binding_);
        }
    private:
        NativeController &owner_;
        Binding *binding_;
    };
    void destroyBinding(Binding *binding);
    void activate(Binding *binding);
    void deactivate();
    void refresh();
    void refreshSymbols();
    ImeService &service_;
    QHash<QObject *, Binding *> bindings_;
    Binding *active_;
    bb::cascades::ArrayDataModel *model_;
    bb::cascades::ArrayDataModel *symbols_;
    QString mode_;
    ModuleProfile profile_;
    QHash<int, int> pressed_;
    QHash<int, bool> shifts_;
    bool enabled_, positionsReady_, areaPoints_, fieldPoints_, learningEnabled_;
    bool symbolsVisible_;
    int symbolGroup_, symbolStart_;
    CandidateTicket symbolTicket_;
    unsigned long symbolRevision_;
};
}
#endif
