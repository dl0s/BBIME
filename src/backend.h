#ifndef BBIME_BACKEND_H
#define BBIME_BACKEND_H
#include "decoder.h"
#include "inputmodule.h"
#include "nativecontroller.h"
#include "modulesettings.h"
#include "focusstate.h"
#include <QObject>
#include <QStringList>
#include <QHash>
#include <QTimer>
#include <QPointer>
#include <QRectF>
#include <QMap>
#include <bb/cascades/ArrayDataModel>
#include <bb/cascades/TextArea>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QObject* nativeModule READ nativeModule CONSTANT)
    Q_PROPERTY(bool imeEnabled READ imeEnabled NOTIFY changed)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)
    Q_PROPERTY(QString composition READ composition NOTIFY changed)
    Q_PROPERTY(QString pinyin READ pinyin NOTIFY changed)
    Q_PROPERTY(bb::cascades::ArrayDataModel* candidateModel READ candidateModel CONSTANT)
    Q_PROPERTY(bb::cascades::ArrayDataModel* symbolModel READ symbolModel CONSTANT)
    Q_PROPERTY(QString pageLabel READ pageLabel NOTIFY changed)
    Q_PROPERTY(QString latency READ latency NOTIFY metricsChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool composing READ composing NOTIFY changed)
    Q_PROPERTY(bool hasPrevious READ hasPrevious NOTIFY changed)
    Q_PROPERTY(bool hasNext READ hasNext NOTIFY changed)
    Q_PROPERTY(int selectedSlot READ selectedSlot NOTIFY changed)
    Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY highlightChanged)
    Q_PROPERTY(int candidatePage READ candidatePage NOTIFY highlightChanged)
    Q_PROPERTY(bool symbolsVisible READ symbolsVisible NOTIFY symbolsChanged)
    Q_PROPERTY(int symbolGroup READ symbolGroup WRITE setSymbolGroup NOTIFY symbolsChanged)
    Q_PROPERTY(QString defaultMode READ defaultMode WRITE setDefaultMode NOTIFY settingsChanged)
    Q_PROPERTY(bool shiftCandidates READ shiftCandidates WRITE setShiftCandidates NOTIFY settingsChanged)
    Q_PROPERTY(bool shiftCursor READ shiftCursor WRITE setShiftCursor NOTIFY settingsChanged)
    Q_PROPERTY(bool chinesePunctuation READ chinesePunctuation WRITE setChinesePunctuation NOTIFY settingsChanged)
    Q_PROPERTY(bool learnSelections READ learnSelections WRITE setLearnSelections NOTIFY settingsChanged)
    Q_PROPERTY(QString settingsStatus READ settingsStatus NOTIFY settingsChanged)
public:
    explicit Backend(QObject *parent = 0);
    ~Backend();
    bool ready() const { return m_ready; }
    QObject *nativeModule() const { return m_nativeModule; }
    bool imeEnabled() const { return m_imeEnabled; }
    QString mode() const { return m_mode; }
    QString composition() const { return m_code; }
    QString pinyin() const;
    bb::cascades::ArrayDataModel *candidateModel() const { return m_candidateModel; }
    bb::cascades::ArrayDataModel *symbolModel() const { return m_symbolModel; }
    QString pageLabel() const;
    QString latency() const { return m_latency; }
    QString status() const { return m_status; }
    bool composing() const { return !m_code.isEmpty(); }
    bool hasPrevious() const { return m_page > 0; }
    bool hasNext() const { return (m_page + 1) * 5 < int(m_decoder.candidates().size()); }
    int selectedSlot() const { return m_decoder.candidates().empty() ? -1 : m_highlight % 5; }
    int selectedIndex() const { return m_decoder.candidates().empty() ? -1 : m_highlight; }
    int candidatePage() const { return m_page; }
    bool symbolsVisible() const { return m_symbolsVisible; }
    int symbolGroup() const { return m_symbolGroup; }
    QString defaultMode() const { return QString::fromStdString(m_moduleSettings.profile().startMode); }
    bool shiftCandidates() const { return m_moduleSettings.profile().shiftCandidates; }
    bool shiftCursor() const { return m_moduleSettings.profile().shiftCursor; }
    bool chinesePunctuation() const { return m_moduleSettings.profile().chinesePunctuation; }
    bool learnSelections() const { return m_moduleSettings.learnSelections(); }
    QString settingsStatus() const { return m_settingsStatus; }
    Q_INVOKABLE void attachEditor(QObject *editor);
    Q_INVOKABLE void editorFocusChanged(bool focused);
    Q_INVOKABLE void setMainScopeActive(bool active);
    Q_INVOKABLE bool registerNativeEditor(QObject *editor);
    Q_INVOKABLE void nativeEditorFocusChanged(QObject *editor, bool focused);
    Q_INVOKABLE void setNativeScopeActive(bool active);
    Q_INVOKABLE void toggleNativeIme();
    Q_INVOKABLE bool handleKey(QObject *event);
    Q_INVOKABLE bool handleNativeKey(QObject *editor, QObject *event);
    Q_INVOKABLE bool choose(int slot);
    Q_INVOKABLE bool chooseCandidate(int index);
    Q_INVOKABLE bool chooseCandidateAt(int index, unsigned long generation);
    Q_INVOKABLE void cycleSymbols();
    Q_INVOKABLE void closeSymbols();
    Q_INVOKABLE bool chooseSymbol(int index);
    Q_INVOKABLE void page(int direction);
    Q_INVOKABLE void moveCandidate(int direction);
    Q_INVOKABLE void toggleLanguage();
    Q_INVOKABLE void toggleIme();
    Q_INVOKABLE void disableIme();
    Q_INVOKABLE bool startNativeModule();
    Q_INVOKABLE void stopNativeModule();
    Q_INVOKABLE void recordLayout(const QString &name, double x, double y,
                                 double width, double height);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void runDiagnostics();
    Q_INVOKABLE void restoreFocus();
    Q_INVOKABLE void publishSettings();
    Q_INVOKABLE void importSettings();
public slots:
    void setMode(const QString &mode);
    void setSymbolGroup(int group);
    void setDefaultMode(const QString &mode);
    void setShiftCandidates(bool enabled);
    void setShiftCursor(bool enabled);
    void setChinesePunctuation(bool enabled);
    void setLearnSelections(bool enabled);
    void initialize();
    void saveDraft();
    void documentChanged();
    void editorPositionChanged();
    void inactive();
    void active();
    void captureLayout();
    void saveLayout();
    void writeMetrics();
    void queueMainFocusCheck();
    void queueNativeFocusCheck();
    void reconcileMainFocus();
    void reconcileNativeFocus();
signals:
    void changed();
    void metricsChanged();
    void candidatesChanged();
    void highlightChanged();
    void symbolsChanged();
    void settingsChanged();
private:
    bool settingsWritable() const;
    void watchEditorState(QObject *editor, bool native);
    QObject *focusedNativeEditor() const;
    void saveModuleProfile(const bbime::ModuleProfile &profile, bool learn);
    void setImeEnabled(bool enabled);
    void refreshCandidates();
    void refreshSymbols();
    void scheduleMetrics();
    void updateCode(const QString &code);
    bool insert(const QString &text);
    void remember();
    void undo();
    void deleteBackward();
    void moveCursor(int delta, bool selection);
    bool literalComposition();
    void measure(double ms);
    double percentile(int percent) const;
    bool inputSelftest();
    int utf16Offset(const QString &text, int position) const;
    int editorOffset(const QString &text, int position) const;
    bbime::ImeService m_inputService;
    bbime::Decoder &m_decoder;
    bbime::NativeController *m_nativeModule;
    bool m_nativePageOpen;
    bool m_mainScopeActive, m_nativeScopeActive;
    bool m_mainFocusQueued, m_nativeFocusQueued;
    bbime::EditorFocusGate m_mainFocusGate, m_nativeFocusGate;
    QList<QPointer<QObject> > m_nativeEditors;
    bbime::ModuleSettings m_moduleSettings;
    bb::cascades::ArrayDataModel *m_candidateModel, *m_symbolModel;
    QPointer<bb::cascades::TextArea> m_editor;
    struct Snapshot { QString text; int cursor, start, end; };
    QList<Snapshot> m_undo;
    QHash<int, int> m_pressed;
    enum ShiftAction { NoShiftAction, CandidateShiftAction, CursorShiftAction };
    QHash<int, ShiftAction> m_shiftPending;
    QTimer m_saveTimer, m_metricsTimer, m_layoutTimer;
    QMap<QString, QRectF> m_layout;
    QString m_mode, m_code, m_status, m_latency, m_settingsStatus;
    int m_page, m_highlight, m_selectionAnchor, m_selectionCursor, m_symbolGroup, m_symbolCycleStart;
    bool m_imeEnabled, m_ready, m_active, m_loading, m_editing, m_testing, m_symbolsVisible;
    bool m_cursorCodePoints;
    unsigned long m_candidateGeneration;
    QVector<double> m_samples;
    unsigned m_sampleCount;
    unsigned m_leftShiftSeen, m_rightShiftSeen, m_shiftMoves;
    unsigned m_shiftCursorMoves;
    unsigned m_altSeen, m_symSeen, m_symbolCycles;
};
#endif
