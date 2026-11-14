// Run from a release folder containing fonts.pak, with no loose fonts/ directory.
#include "../../src/gs/ui_font.h"
#include <cassert>
int main() {
    ImGui::CreateContext();
    assert(ui_font_load("fonts/anton/Anton-Regular.ttf", 48));
    assert(ui_font_load("fonts/dejavu/DejaVuSansCondensed.ttf", 18));
    assert(ui_font_load("fonts/dejavu/DejaVuSansCondensed-Bold.ttf", 48));
    assert(!ui_font_load("fonts/missing-font.ttf", 18));
    ImGui::DestroyContext();
    puts("PASS: packaged Anton and DejaVu fonts load; missing font returns safely");
}
