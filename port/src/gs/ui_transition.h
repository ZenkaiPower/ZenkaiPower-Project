#ifndef PORT_UI_TRANSITION_H
#define PORT_UI_TRANSITION_H

/* Presentation only: keep the overlay and its input ownership alive through the closing animation. */
struct UiDialogTransition {
    float progress = 0.0f;
    bool wanted = false;

    void set(bool open) { wanted = open; }
    bool active() const { return wanted || progress > 0.0f; }
    bool closing() const { return !wanted && progress > 0.0f; }
    void reset() { wanted = false; progress = 0.0f; }
    void advance(float seconds) {
        if (seconds < 0.0f) seconds = 0.0f;
        if (seconds > 0.05f) seconds = 0.05f;
        progress += wanted ? seconds / 0.18f : -seconds / 0.14f;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
    }
    float amount() const { return progress * progress * (3.0f - 2.0f * progress); }
};

#endif
