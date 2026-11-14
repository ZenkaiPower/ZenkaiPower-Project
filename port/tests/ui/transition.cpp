/* g++ -std=c++17 -Wall -Wextra -Werror port/tests/ui/transition.cpp -o /tmp/ui_transition_test
 * /tmp/ui_transition_test */
#include "../../src/gs/ui_transition.h"
#include <cassert>
#include <initializer_list>

int main() {
    for (float frame : {1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 144.0f}) {
        UiDialogTransition transition;
        assert(!transition.active());
        transition.set(true);
        assert(transition.active());
        for (int i = 0; i < 30; ++i) transition.advance(frame);
        assert(transition.progress == 1.0f && transition.amount() == 1.0f);
        transition.set(false);
        assert(transition.active() && transition.closing());
        transition.advance(frame);
        float closing = transition.progress;
        assert(closing < 1.0f && closing > 0.0f);
        transition.set(true); // F1 during closing reverses without a jump.
        assert(transition.progress == closing);
        transition.advance(frame);
        assert(transition.progress > closing && !transition.closing());
        transition.set(false);
        for (int i = 0; i < 30; ++i) transition.advance(frame);
        assert(!transition.active() && transition.progress == 0.0f && transition.amount() == 0.0f);
    }
    UiDialogTransition transition;
    transition.set(true);
    transition.set(false); // Open and close before the first rendered frame.
    assert(!transition.active());
    transition.set(true);
    transition.advance(10.0f); // A stalled frame must not skip the entire animation.
    assert(transition.progress > 0.0f && transition.progress < 1.0f);
    float progress = transition.progress;
    transition.advance(-1.0f);
    assert(transition.progress == progress);
    transition.reset();
    assert(!transition.active());
}
