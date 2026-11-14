/* cc -std=c99 port/tests/ui/auto_video.c -o /tmp/auto-video-test && /tmp/auto-video-test */
#include "../../src/gs/auto_video.h"
#include <assert.h>
#include <limits.h>
int main(void) {
    assert(AutoVideo_Aspect(1920, 1080) == 1778);
    assert(AutoVideo_Aspect(2560, 1440) == 1778);
    assert(AutoVideo_Aspect(3440, 1440) == 2389);
    assert(AutoVideo_Aspect(1920, 1200) == 1600);
    assert(AutoVideo_Aspect(1080, 1920) == 1333);
    assert(AutoVideo_Aspect(7680, 1080) == 4000);
    assert(AutoVideo_Aspect(0, 0) == 1333);
    assert(AutoVideo_Scale(1920, 1080, 1778) == 3);
    assert(AutoVideo_Scale(1593, 896, 1778) == 2); /* Default window on a 1080p monitor. */
    assert(AutoVideo_Scale(1920, 1080, 2389) == 2); /* Ultrawide picture letterboxed on 1080p. */
    assert(AutoVideo_Scale(3840, 2160, 1778) == 5);
    assert(AutoVideo_Scale(1280, 720, 1778) == 2);
    assert(AutoVideo_Scale(3440, 1440, 2389) == 4);
    assert(AutoVideo_Scale(800, 448, 1778) == 1);
    assert(AutoVideo_Scale(600, 1200, 1778) == 1); /* Narrow window: letterboxed picture. */
    assert(AutoVideo_Scale(15360, 8640, 1778) == 8);
    assert(AutoVideo_Scale(0, 0, 1778) == 1);
    assert(AutoVideo_Scale(INT_MAX, INT_MAX, 1333) == 8);
    return 0;
}
