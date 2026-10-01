#include "inputmodule.h"

namespace bbime {
ImeService::ImeService() : active_(0), opened_(false), threadSet_(false), nextId_(0) {}
ImeService::~ImeService() { close(); }
bool ImeService::onInputThread() const {
    return threadSet_ && pthread_equal(thread_, pthread_self()) != 0;
}
bool ImeService::ready() const { return opened_ && onInputThread(); }
bool ImeService::open(const std::string &dictionary, const std::string &user) {
    if (threadSet_ && !onInputThread()) return false;
    if (!decoder_.open(dictionary, user)) return false;
    thread_ = pthread_self();
    threadSet_ = true;
    opened_ = true;
    return true;
}
void ImeService::suspend() {
    if (onInputThread() && active_) active_->deactivate();
}
bool ImeService::close() {
    if (threadSet_ && !onInputThread()) return false;
    suspend();
    decoder_.close();
    opened_ = false;
    return true;
}
bool ImeService::flush() {
    if (!ready()) return false;
    decoder_.flush();
    return true;
}

InputSession::InputSession(ImeService &service, EditorAdapter &editor,
                           const FieldPolicy &policy) :
    service_(service), editor_(editor), policy_(policy), id_(0), revision_(0),
    documentRevision_(0), highlight_(0), mode_("natural"), learningEnabled_(true) {
    if (service_.onInputThread()) id_ = ++service_.nextId_;
}
InputSession::~InputSession() { deactivate(); }
bool InputSession::active() const {
    return service_.ready() && service_.active_ == this && policy_.allowed() &&
        editor_.eligible();
}
bool InputSession::activate() {
    if (!service_.ready()) return false;
    if (!policy_.allowed() || !editor_.eligible()) {
        service_.suspend();
        return false;
    }
    if (!id_) id_ = ++service_.nextId_;
    if (service_.active_ != this) {
        service_.suspend();
        service_.active_ = this;
        cancel();
    } else if (documentRevision_ != editor_.revision()) cancel();
    documentRevision_ = editor_.revision();
    return true;
}
void InputSession::deactivate() {
    if (!service_.onInputThread()) return;
    cancel();
    if (service_.active_ == this) service_.active_ = 0;
}
void InputSession::cancel() {
    if (!service_.onInputThread()) return;
    code_.clear();
    pinyin_.clear();
    candidates_.clear();
    highlight_ = 0;
    ++revision_;
    if (service_.active_ == this && service_.ready())
        service_.decoder_.setCode("", true);
}
bool InputSession::setMode(const std::string &mode) {
    if (!active() || (mode != "natural" && mode != "english"))
        return false;
    cancel();
    mode_ = mode;
    return true;
}
bool InputSession::setCode(const std::string &code) {
    if (!active() || mode_ == "english") return false;
    if (!code_.empty() && editor_.revision() != documentRevision_) {
        cancel();
        return false;
    }
    if (!service_.decoder_.setCode(code, true)) return false;
    code_ = code;
    pinyin_ = service_.decoder_.pinyin();
    candidates_ = service_.decoder_.candidates();
    highlight_ = 0;
    documentRevision_ = editor_.revision();
    ++revision_;
    return true;
}
bool InputSession::append(char letter) { return setCode(code_ + letter); }
bool InputSession::backspace() {
    if (!active()) return false;
    if (!code_.empty()) return setCode(code_.substr(0, code_.size() - 1));
    return editor_.erasePrevious(editor_.revision());
}
bool InputSession::moveHighlight(int direction) {
    if (!active() || candidates_.empty()) return false;
    if (direction < 0) { if (highlight_) --highlight_; }
    else if (highlight_ + 1 < candidates_.size()) ++highlight_;
    return true;
}
CandidateTicket InputSession::ticket(size_t index) const {
    CandidateTicket result;
    result.session = id_;
    result.revision = revision_;
    result.document = code_.empty() ? editor_.revision() : documentRevision_;
    result.index = index;
    return result;
}
bool InputSession::accepts(const CandidateTicket &ticket) const {
    return active() && ticket.session == id_ && ticket.revision == revision_ &&
        ticket.document == (code_.empty() ? editor_.revision() : documentRevision_) &&
        editor_.revision() == ticket.document;
}
bool InputSession::choose(const CandidateTicket &ticket) {
    if (!accepts(ticket) || ticket.index >= candidates_.size()) return false;
    const Candidate chosen = candidates_[ticket.index];
    if (!chosen.consumed || chosen.consumed > code_.size()) return false;
    const std::string rest = code_.substr(chosen.consumed);
    if (!editor_.replaceSelection(chosen.text, documentRevision_)) return false;
    // Host text-change callbacks can switch fields or close the page during
    // replacement. The committed edit must not learn or decode in a new field.
    if (!active() || ticket.revision != revision_) return true;
    service_.decoder_.choose(ticket.index, policy_.mayLearn() && learningEnabled_);
    documentRevision_ = editor_.revision();
    return setCode(rest);
}
bool InputSession::commitPending() {
    if (!active()) return false;
    if (code_.empty()) return true;
    if (!candidates_.empty()) {
        // A prefix choice may leave codes. Finish all remaining composition
        // before the host performs a submit or navigation command.
        while (!code_.empty() && !candidates_.empty())
            if (!choose(ticket(highlight_))) return false;
    }
    if (!code_.empty()) {
        if (editor_.revision() != documentRevision_ ||
            !editor_.replaceSelection(code_, documentRevision_)) return false;
        cancel();
    }
    return active();
}
bool InputSession::insertLiteral(const std::string &text) {
    if (!active() || (!policy_.multiline && text.find_first_of("\r\n") != std::string::npos))
        return false;
    if (!commitPending()) return false;
    return editor_.replaceSelection(text, editor_.revision());
}
bool InputSession::moveCursor(int direction) {
    if (!active()) return false;
    cancel();
    return editor_.moveCursor(direction, editor_.revision());
}
}
