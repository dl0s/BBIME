#include "focusstate.h"
#include <cstdio>
#include <cstdlib>

static unsigned checks = 0;
static void check(bool condition, const char *name) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "FAIL: editor focus gate: %s\n", name);
        std::exit(1);
    }
}

int main() {
    int mainEditor = 1, nativeTitle = 2, nativeBody = 3, password = 4;
    bbime::EditorFocusGate gate;
    check(gate.owner() == 0 && !gate.permits(true, true, true, 0),
          "startup without a real editing owner is disabled");

    gate.observe(&mainEditor);
    check(gate.owner() == &mainEditor && gate.permits(true, true, true, &mainEditor),
          "eligible focused main editor may activate");
    check(!gate.permits(false, true, true, &mainEditor), "unready decoder blocks input");
    check(!gate.permits(true, false, true, &mainEditor), "background or asleep app blocks input");
    check(!gate.permits(true, true, false, &mainEditor), "inactive page or Sheet scope blocks input");
    check(!gate.permits(true, true, true, 0), "lost actual focus blocks input");
    check(!gate.permits(true, true, true, &password), "another focused field cannot borrow ownership");

    gate.pause();
    check(gate.paused() && !gate.permits(true, true, true, &mainEditor),
          "manual menu pause wins over an eligible focused editor");
    gate.observe(&mainEditor);
    check(gate.paused() && !gate.permits(true, true, true, &mainEditor),
          "duplicate focus notification cannot bounce out of manual pause");
    check(!gate.permits(true, true, false, &mainEditor) && gate.paused(),
          "Sheet opening preserves manual pause");
    check(!gate.permits(true, false, true, &mainEditor) && gate.paused(),
          "lifecycle eligibility evaluation preserves manual pause");
    check(!gate.permits(true, true, true, &mainEditor),
          "Sheet close and foreground return cannot implicitly resume unchanged ownership");
    gate.observe(&mainEditor);
    check(!gate.permits(false, true, true, &mainEditor) && gate.paused(),
          "temporary read-only hidden disabled or wrong input mode does not reset actual focus pause");
    gate.observe(&mainEditor);
    check(!gate.permits(true, true, true, &mainEditor) && gate.paused(),
          "restored eligibility without a real focus change remains paused");

    gate.resume();
    check(!gate.paused() && gate.permits(true, true, true, &mainEditor),
          "explicit input toggle can resume the same real editor");
    gate.pause();
    gate.observe(&nativeTitle);
    check(!gate.paused() && gate.owner() == &nativeTitle &&
          gate.permits(true, true, true, &nativeTitle),
          "new editor identity clears the previous field's pause");
    check(!gate.permits(true, true, true, &mainEditor),
          "previous editor cannot use the transferred lease");
    gate.observe(&nativeBody);
    check(gate.permits(true, true, true, &nativeBody) &&
          !gate.permits(true, true, true, &nativeTitle),
          "native title to body transfer follows actual owner");

    gate.pause();
    gate.observe(0);
    check(gate.owner() == 0 && !gate.permits(true, true, true, &nativeBody),
          "true blur revokes ownership even if a stale field is presented");
    gate.observe(0);
    gate.resume();
    check(!gate.permits(true, true, true, 0), "resume without editing focus cannot enable input");
    gate.observe(&nativeBody);
    check(!gate.paused() && gate.permits(true, true, true, &nativeBody),
          "fresh focus after true blur may activate again");

    std::printf("PASS: %u production EditorFocusGate checks (exported pure C++98 policy; SDK focus delivery not exercised)\n", checks);
    return 0;
}
