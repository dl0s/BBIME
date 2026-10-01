#ifndef BBIME_INPUTMODULE_H
#define BBIME_INPUTMODULE_H

#include "decoder.h"
#include <pthread.h>

class Backend;
namespace bbime {

struct FieldPolicy {
    enum Kind { Denied, PlainText, Search, Path, Sensitive, Password, Numeric };
    explicit FieldPolicy(Kind value = Denied) :
        kind(value), learn(false), multiline(false), chinesePunctuation(false) {}
    bool allowed() const { return kind == PlainText || kind == Search || kind == Path; }
    bool mayLearn() const { return allowed() && kind == PlainText && learn; }
    Kind kind;
    bool learn, multiline, chinesePunctuation;
};

// The host owns its document, persistence and undo. Replacement must be atomic
// with respect to the supplied document/selection revision.
class EditorAdapter {
public:
    virtual ~EditorAdapter() {}
    virtual bool eligible() const = 0;
    virtual unsigned long revision() const = 0;
    virtual bool replaceSelection(const std::string &utf8, unsigned long expected) = 0;
    virtual bool erasePrevious(unsigned long expected) = 0;
    virtual bool moveCursor(int direction, unsigned long expected) = 0;
};

struct CandidateTicket {
    CandidateTicket() : session(0), revision(0), document(0), index(0) {}
    unsigned long session, revision, document;
    size_t index;
};

class InputSession;
class ImeService {
public:
    ImeService();
    ~ImeService();
    bool open(const std::string &dictionary, const std::string &privateUserDictionary);
    bool close();
    bool ready() const;
    bool onInputThread() const;
    bool flush();
    void suspend();
private:
    friend class InputSession;
    friend class ::Backend;
    Decoder &legacyDecoder() { return decoder_; }
    ImeService(const ImeService &);
    ImeService &operator=(const ImeService &);
    Decoder decoder_;
    InputSession *active_;
    pthread_t thread_;
    bool opened_, threadSet_;
    unsigned long nextId_;
};

class InputSession {
public:
    InputSession(ImeService &service, EditorAdapter &editor, const FieldPolicy &policy);
    ~InputSession();
    bool activate();
    void deactivate();
    bool active() const;
    bool setMode(const std::string &mode);
    const std::string &mode() const { return mode_; }
    const std::string &composition() const { return code_; }
    const std::string &pinyin() const { return pinyin_; }
    const std::vector<Candidate> &candidates() const { return candidates_; }
    unsigned long id() const { return id_; }
    unsigned long revision() const { return revision_; }
    size_t selectedIndex() const { return highlight_; }
    bool setCode(const std::string &code);
    bool append(char letter);
    bool backspace();
    void cancel();
    bool moveHighlight(int direction);
    CandidateTicket ticket(size_t index) const;
    bool choose(const CandidateTicket &ticket);
    bool accepts(const CandidateTicket &ticket) const;
    bool commitPending();
    bool insertLiteral(const std::string &text);
    bool moveCursor(int direction);
    bool multiline() const { return policy_.multiline; }
    bool chinesePunctuation() const { return policy_.chinesePunctuation; }
    void setLearningEnabled(bool enabled) { learningEnabled_ = enabled; }
private:
    InputSession(const InputSession &);
    InputSession &operator=(const InputSession &);
    ImeService &service_;
    EditorAdapter &editor_;
    FieldPolicy policy_;
    unsigned long id_, revision_, documentRevision_;
    size_t highlight_;
    std::string mode_, code_, pinyin_;
    std::vector<Candidate> candidates_;
    bool learningEnabled_;
};
}
#endif
