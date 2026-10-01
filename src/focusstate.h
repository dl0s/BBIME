#ifndef BBIME_FOCUSSTATE_H
#define BBIME_FOCUSSTATE_H

namespace bbime {
// Editor focus policy. A scope/lifecycle notification never grants
// focus or clears a user's pause; a genuinely different focus epoch does.
class EditorFocusGate {
public:
    EditorFocusGate() : owner_(0), paused_(false) {}
    void observe(const void *owner) {
        if (owner_ == owner) return;
        owner_ = owner;
        paused_ = false;
    }
    void pause() { paused_ = true; }
    void resume() { paused_ = false; }
    bool paused() const { return paused_; }
    const void *owner() const { return owner_; }
    bool permits(bool ready, bool foreground, bool scopeActive,
                 const void *actualFocusedEditor) const {
        return ready && foreground && scopeActive && !paused_ && owner_ &&
            owner_ == actualFocusedEditor;
    }
private:
    const void *owner_;
    bool paused_;
};
}
#endif
