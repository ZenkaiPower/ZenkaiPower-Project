#include "../../src/gs/text_resolution.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(PortText_FollowsResolution(0)==!!ZENKAIPOWER_TEXT_FOLLOW_RES);
    assert(PortText_FollowsResolution(1)==!!ZENKAIPOWER_HUD_INFO_FOLLOW_RES);
    assert(PortText_UsesScreen()==(!ZENKAIPOWER_TEXT_FOLLOW_RES || !ZENKAIPOWER_HUD_INFO_FOLLOW_RES));
    puts("Compiled text resolution policy passed");
}
