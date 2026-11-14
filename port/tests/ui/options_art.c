/* cc -std=c99 -Iinclude port/tests/ui/options_art.c -o /tmp/options-art */
#include <assert.h>
#include <string.h>
typedef int s32;
typedef float f32;
#include "menu/port_options_art.h"
#include "menu/port_menu_art_policy.h"
static float center(float x, int aspect) { return 256 + (x - 256) * 1333.333333f / aspect; }
static float right(float x, int aspect) { return 512 + (x - 512) * 1333.333333f / aspect; }
int main(void) {
    int ratios[] = {1333, 1778, 2333, 3556};
    unsigned i;
    assert(Port_OptionsMouseControl("mc_select_plate_1"));
    assert(Port_OptionsMouseControl("mc_select_plate_4"));
    assert(!Port_OptionsMouseControl("mc_select_plate_1_text"));
    assert(!Port_OptionsMouseControl("mc_select_plate_5"));
    assert(Port_OptionsMouseControl("mc_yajirusi_up"));
    assert(Port_OptionsMouseControl("mc_yajirusi_down"));
    assert(Port_OptionsMouseControl("mc_yajirusi_left"));
    assert(Port_OptionsMouseControl("mc_yajirusi_right"));
    assert(Port_OptionsMouseControl("mc_bgm_scroll_bar"));
    assert(!Port_OptionsMouseControl("mc_yajirusi_icon_up"));
    assert(!Port_OptionsMouseControl("mc_yajirusi_icon_right"));
    assert(Port_MenuArtModeCovered(62));
    assert(!Port_MenuArtOptionsStateCovered(9));
    assert(!Port_MenuArtOptionsStateCovered(11));
    assert(Port_MenuArtOptionsStateCovered(2));
    assert(Port_MenuArtOptionsStateCovered(7));
    assert(Port_OptionsCameraArrow(180.0f));
    assert(!Port_OptionsCameraArrow(113.0f));
    assert(!Port_OptionsCameraArrow(225.0f));
    assert(Port_OptionsArtMarker("mc_guide_dende") == 0x1A);
    assert(Port_OptionsArtMarker("mc_pikkoro") == 0x1B);
    assert(Port_OptionsArtMarker("mc_menu_plate_1") == 0x1B);
    assert(Port_OptionsArtMarker("mc_bottom_plate_1") == 0x1A);
    assert(Port_OptionsArtMarker("mc_select_plate_1") == 0x19);
    assert(Port_OptionsArtMarker("mc_select_plate_4") == 0x19);
    assert(Port_OptionsArtMarker("mc_vol_window") == 0x19);
    assert(Port_OptionsArtMarker("mc_vol_plate_bgm_9") == 0x19);
    assert(Port_OptionsArtMarker("mc_bgm_scroll_bar") == 0x13);
    assert(Port_OptionsArtMarker("mc_yajirusi_up") == 0x1A);
    assert(Port_OptionsArtMarker("mc_button_text_7") == 0x1B);
    assert(Port_OptionsArtMarker("mc_type_image_1") == 0x19);
    assert(Port_OptionsArtMarker("mc_layout_bg") == 0x19);
    assert(Port_OptionsArtMarker("mc_target") == 0x19);
    assert(Port_OptionsImage(34) == 0x19);
    assert(Port_OptionsImage(44) == 0x19); /* camera preview frame */
    assert(Port_OptionsImage(33) == 0x13);
    assert(Port_OptionsImage(35) == 0x13); /* dimmer always fills the page */
    assert(Port_OptionsImage(28) == 0x1A);
    assert(Port_OptionsImage(21) == 0x19);
    assert(Port_OptionsImage(31) == 0x13);
    assert(Port_OptionsImage(32) == 0x13);
    assert(Port_OptionsImage(40) == 0x19);
    assert(Port_OptionsImage(47) == 0x1B);
    for (i = 0; i < sizeof(ratios)/sizeof(*ratios); ++i) {
        int aspect = ratios[i];
        /* Original volume arrows x=105/475 and preview arrows x=127/442
         * share the centered panel transform; their spacing scales uniformly. */
        float scale = 1333.333333f / aspect;
        float gap = center(148.65f, aspect) - center(105, aspect);
        assert(gap > 43.64f * scale && gap < 43.66f * scale);
        assert(center(127, aspect) < center(174, aspect));
        assert(center(442, aspect) > center(430, aspect));
        float volumeShift = Port_OptionsVolumeTravel(522, aspect);
        assert(center(522 + volumeShift, aspect) > 512);
        assert(Port_OptionsVolumeTravel(148.65f, aspect) == 0);
        assert(Port_OptionsVolumeTravel(335.325f, aspect) <= volumeShift);
        assert(Port_OptionsVolumeTravel(335.325f, aspect) >= 0);
        float shift = Port_OptionsChoiceTravel("mc_select_plate_1", -172, aspect);
        assert(center(-172 + shift + 170, aspect) < 0); /* complete parked left choice */
        assert(Port_OptionsChoiceTravel("mc_select_plate_1", 138, aspect) == 0);
        assert(Port_OptionsChoiceTravel("mc_select_plate_2", 318, aspect) == 0);
        assert(center(518 + Port_OptionsChoiceTravel("mc_select_plate_2", 518, aspect), aspect) > 512);
        assert(right(521, aspect) > 512); /* parked main rows */
    }
    assert(Port_OptionsChoiceTravel("mc_select_plate_1", -172, 1333) == 0);
    return 0;
}
