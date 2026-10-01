#include "../src/inputmodule.h"
#include <cstdio>
#include <cstdlib>

static void check(bool condition, const char *name) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
static std::string bytes(const char *path) {
    std::FILE *file = std::fopen(path, "rb");
    check(file != 0, "synthetic dictionary readable");
    std::string result;
    char buffer[4096];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
        result.append(buffer, count);
    check(!std::ferror(file), "synthetic dictionary read complete");
    std::fclose(file);
    return result;
}

class Editor : public bbime::EditorAdapter {
public:
    Editor() : version(0), start(0), end(0), maximum(1024), writable(true),
        reject(false), switchTo(0) {}
    bool eligible() const { return writable; }
    unsigned long revision() const { return version; }
    bool replaceSelection(const std::string &text, unsigned long expected) {
        if (!writable || reject || expected != version ||
            document.size() - (end - start) + text.size() > maximum) return false;
        document.replace(start, end - start, text);
        start += text.size();
        end = start;
        ++version;
        if (switchTo) switchTo->activate();
        return true;
    }
    bool erasePrevious(unsigned long expected) {
        if (start == end && start) --start;
        return replaceSelection("", expected);
    }
    bool moveCursor(int direction, unsigned long expected) {
        if (!writable || expected != version) return false;
        if (direction < 0) { if (start) --start; }
        else if (end < document.size()) ++start;
        end = start;
        ++version;
        return true;
    }
    void reset(const std::string &value = "") {
        document = value;
        start = end = document.size();
        ++version;
    }
    std::string document;
    unsigned long version;
    size_t start, end, maximum;
    bool writable, reject;
    bbime::InputSession *switchTo;
};

struct ThreadProbe {
    bbime::ImeService *service;
    bbime::InputSession *session;
    bool rejected;
};
static void *wrongThread(void *argument) {
    ThreadProbe *probe = static_cast<ThreadProbe *>(argument);
    probe->rejected = !probe->service->ready() && !probe->service->flush() &&
        !probe->service->close() && !probe->session->activate() &&
        !probe->session->setCode("vsgo") &&
        !probe->session->choose(probe->session->ticket(0));
    probe->session->cancel();
    probe->service->suspend();
    return 0;
}

int main(int argc, char **argv) {
    check(argc == 3, "dictionary arguments");
    bbime::ImeService service;
    Editor a, b;
    bbime::FieldPolicy text(bbime::FieldPolicy::PlainText);
    text.multiline = true;
    bbime::InputSession first(service, a, text);
    check(!first.activate(), "unopened service rejects activation");
    check(service.open(argv[1], argv[2]), "single service load");
    {
        bbime::ImeService intruder;
        check(!intruder.open(argv[1], argv[2]), "second service rejected");
    }
    text.multiline = false;
    bbime::InputSession second(service, b, text);
    check(first.activate() && first.setCode("nihk"), "first composition");
    bbime::CandidateTicket old = first.ticket(0);
    check(second.activate() && !first.active() && first.composition().empty() &&
        a.document.empty() && !first.choose(old) && !second.choose(old),
        "switch cancels preedit and rejects cross-field ticket");
    check(second.setCode("nihk") && second.choose(second.ticket(0)) &&
        b.document == "\xe4\xbd\xa0\xe5\xa5\xbd", "second field commit isolated");
    check(second.setCode("ni"), "ticket revision preparation");
    old = second.ticket(0);
    check(second.append('h') && !second.choose(old), "old composition ticket rejected");
    old = second.ticket(0);
    b.reset();
    check(!second.choose(old) && b.document.empty(), "external document change rejects ticket");
    check(!second.append('k') && second.composition().empty(),
        "external document change cancels obsolete preedit");

    check(second.setCode("nihk"), "failed edit preparation");
    old = second.ticket(0);
    b.reject = true;
    service.flush();
    const std::string noLearn = bytes(argv[2]);
    check(!second.choose(old) && second.composition() == "nihk" && b.document.empty(),
        "failed edit preserves composition and document");
    service.flush();
    check(bytes(argv[2]) == noLearn, "failed edit does not learn");
    b.reject = false;
    check(second.choose(old), "failed edit can retry same ticket");
    b.reset();
    second.cancel();
    b.maximum = 1;
    check(second.setCode("nihk") && !second.choose(second.ticket(0)) &&
        second.composition() == "nihk" && b.document.empty(), "length refusal recoverable");
    b.maximum = 1024;
    check(second.choose(second.ticket(0)), "length refusal retry");
    b.reset("abc");
    second.cancel();
    b.start = 1;
    b.end = 2;
    ++b.version;
    check(second.setCode("nihk") && second.choose(second.ticket(0)) &&
        b.document == "a\xe4\xbd\xa0\xe5\xa5\xbd" "c", "selection replacement");

    b.reset();
    second.cancel();
    check(!second.insertLiteral("a\nb") && b.document.empty(), "single-line newline refused");
    check(first.activate() && first.insertLiteral("a\nb") && a.document == "a\nb",
        "multiline literal accepted");
    check(first.setCode("nihk"), "prefix preparation");
    size_t prefix = first.candidates().size();
    for (size_t i = 0; i < first.candidates().size(); ++i)
        if (first.candidates()[i].consumed == 2) { prefix = i; break; }
    check(prefix < first.candidates().size() && first.choose(first.ticket(prefix)) &&
        first.composition() == "hk", "prefix choice preserves suffix");
    check(first.commitPending() && first.composition().empty(), "submit completes suffix");
    check(first.setMode("full") && first.setCode("ni'hao") &&
        first.commitPending(), "full pinyin session");
    check(first.setMode("english") && !first.append('n') && first.insertLiteral("A"),
        "English bypasses decoder");
    check(!first.setMode("system") && first.mode() == "english", "invalid mode unchanged");

    const bbime::FieldPolicy::Kind denied[] = { bbime::FieldPolicy::Denied,
        bbime::FieldPolicy::Sensitive, bbime::FieldPolicy::Password, bbime::FieldPolicy::Numeric };
    for (size_t i = 0; i < sizeof(denied) / sizeof(denied[0]); ++i) {
        bbime::FieldPolicy policy(denied[i]);
        policy.learn = true;
        Editor blocked;
        bbime::InputSession session(service, blocked, policy);
        check(!session.activate() && !session.append('n') && !session.insertLiteral("secret") &&
            !policy.mayLearn() && blocked.document.empty(), "denied field never receives input");
    }
    b.reset();
    check(second.activate() && second.setCode("nihk"), "read-only preparation");
    old = second.ticket(0);
    b.writable = false;
    check(!second.choose(old) && !second.insertLiteral("x"), "read-only rejects commit");
    second.deactivate();
    b.writable = true;
    check(second.activate() && second.composition().empty(), "read-only cleanup");

    check(second.setCode("nihk"), "wrong-thread preparation");
    old = second.ticket(0);
    ThreadProbe probe = { &service, &second, false };
    pthread_t thread;
    check(pthread_create(&thread, 0, wrongThread, &probe) == 0, "thread probe create");
    check(pthread_join(thread, 0) == 0 && probe.rejected && second.composition() == "nihk" &&
        second.choose(old), "wrong-thread calls leave active owner usable");
    second.cancel();
    b.reset();
    check(second.setCode("nihk"), "suspend preparation");
    old = second.ticket(0);
    service.suspend();
    check(!second.active() && second.composition().empty() && !second.choose(old),
        "background cancels session and tickets");
    check(second.activate() && !second.choose(old), "reactivation cannot revive old ticket");

    check(second.setCode("nihk"), "reentrant field switch preparation");
    b.switchTo = &first;
    check(second.choose(second.ticket(0)) && first.active() && !second.active() &&
        first.composition().empty() && second.composition().empty(),
        "host callback switch cannot leak suffix or learning");
    b.switchTo = 0;
    service.flush();
    check(bytes(argv[2]) == noLearn, "no-learning sessions preserve dictionary bytes");
    {
        Editor transient;
        bbime::InputSession session(service, transient, text);
        check(session.activate() && session.setCode("nihk"), "temporary field active");
    }
    check(first.activate(), "destroyed session releases service");
    check(service.close() && !first.active() && first.composition().empty() &&
        service.open(argv[1], argv[2]) && first.activate(), "close and reopen resets sessions");
    std::printf("PASS: input sessions, field isolation, stale tickets, selection replacement, "
        "failed commit recovery, read-only and denied fields, prefix submit, English/full pinyin, "
        "wrong thread, lifecycle, reentrant host callbacks and no-learning.\n");
    return 0;
}
