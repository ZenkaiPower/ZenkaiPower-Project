/* cc -std=c99 -Iinclude port/tests/ui/data_center_art.c -o /tmp/data-center-art */
#include <assert.h>
#include <string.h>
typedef int s32;
typedef float f32;
#include "menu/port_data_center_art.h"
#include "menu/port_menu_art_policy.h"

static float placed(float x, float pivot, int aspect) {
    return pivot + (x - pivot) * 1333.333f / aspect;
}

int main(void) {
    const int ratios[] = {1333, 1778, 2333, 3556};
    unsigned i;
    assert(Port_MenuArtModeCovered(53));
    assert(Port_MenuArtModeCovered(54));
    assert(Port_MenuArtModeCovered(55));
    assert(Port_MenuArtModeCovered(56));
    assert(Port_DataCenterArtMarker("mc_guide_blma") == 0x1A);
    assert(Port_DataCenterSubmenuMarker(62, "mc_input_code_1_01") == 0);
    assert(Port_DataCenterSubmenuMarker(62, "mc_text_keyboard") == 0);
    assert(Port_DataCenterSubmenuMarker(62, "mc_input_pass_1_17") == 0);
    assert(Port_DataCenterSubmenuMarker(62, "mc_guide_blma") == 0x1A);
    assert(Port_DataCenterSubmenuImage(62, 7) == 0);
    assert(Port_DataCenterSubmenuImage(62, 9) == 0);
    assert(Port_DataCenterSubmenuImage(62, 20) == 0);
    assert(Port_DataCenterSubmenuMarker(63, "mc_single_chara_r") == 0x1B);
    assert(Port_DataCenterSubmenuMarker(63, "mc_name_text_l") == 0x1A);
    assert(Port_DataCenterSubmenuMarker(63, "mc_name_text_r") == 0x1B);
    assert(Port_DataCenterSubmenuImage(63, 30) == 0);
    assert(Port_DataCenterSubmenuImage(63, 25) == 0x1B);
    assert(Port_DataCenterSubmenuImage(63, 26) == 0x1B);
    assert(Port_DataCenterNamePlateMarker(232) == Port_DataCenterSubmenuMarker(63, "mc_name_text_l"));
    assert(Port_DataCenterNamePlateMarker(263) == Port_DataCenterSubmenuMarker(63, "mc_form_text_l"));
    assert(Port_DataCenterNamePlateMarker(23) == Port_DataCenterSubmenuMarker(63, "mc_name_text_r"));
    assert(Port_DataCenterNamePlateMarker(54) == Port_DataCenterSubmenuMarker(63, "mc_form_text_r"));
    assert(Port_DataCenterSubmenuImage(63, 33) == 0x19);
    assert(Port_DataCenterSubmenuMarker(63, "mc_name_text_r") == Port_DataCenterSubmenuImage(63, 25));
    assert(Port_DataCenterSubmenuMarker(64, "menu_plate_1") == 0x1A);
    assert(Port_DataCenterSubmenuMarker(64, "mc_status_attribute") == 0x1A);
    assert(Port_DataCenterSubmenuMarker(64, "mc_menu_plate2_1") == 0x1B);
    assert(Port_DataCenterSubmenuImage(64, 9) == 0x1A);
    assert(Port_DataCenterSubmenuImage(64, 20) == 0x1B);
    assert(Port_DataCenterSubmenuMarker(65, "mc_list_plate_1") == 0x19);
    assert(Port_DataCenterSubmenuMarker(65, "mc_name_plate_l") == 0x1A);
    assert(Port_DataCenterSubmenuImage(65, 0) == 0);
    assert(Port_DataCenterSubmenuImage(65, 2) == 0);
    assert(Port_DataCenterSubmenuImage(65, 3) == 0x1B);
    assert(Port_DataCenterSubmenuImage(65, 7) == 0x19);
    assert(Port_DataCenterSubmenuMarker(65, "mc_icon_item_potara") == 0x19);
    assert(Port_DataCenterSubmenuMarker(65, "mc_ability_limit_big_num") == 0x1B);
    assert(Port_DataCenterSubmenuMarker(66, "mc_chara_000") == 0x19);
    assert(Port_DataCenterSubmenuMarker(66, "mc_yajirusi_up") == 0x19);
    assert(Port_DataCenterSubmenuImage(66, 7) == 0);
    assert(Port_DataCenterSubmenuImage(66, 31) == 0);
    assert(Port_DataCenterSubmenuImage(66, 19) == 0x19);
    assert(Port_DataCenterSubmenuMarker(66, "mc_compane_3") == 0);
    assert(Port_DataCenterArtMarker("mc_menu_plate_1") == 0x1B);
    assert(Port_DataCenterArtMarker("mc_menu_plate_2") == 0x1B);
    assert(Port_DataCenterArtMarker("mc_menu_plate_3") == 0x1B);
    assert(Port_DataCenterArtMarker("mc_compane_3") == 0);
    assert(Port_DataCenterArtMarker("mc_compane_1") == 0);
    /* Nested children inherit the parent anchor; no separate transform. */
    assert(Port_DataCenterArtMarker("mc_guide_blma_eye") == 0);
    assert(Port_DataCenterArtMarker("mc_guide_blma_mouth") == 0);
    assert(Port_DataCenterArtMarker("mc_menu_text_on") == 0);
    for (i = 0; i < sizeof(ratios) / sizeof(ratios[0]); i++) {
        int aspect = ratios[i], frame;
        assert(Port_DataCenterItemTravel(0, aspect) == 0);
        if (aspect > 1333) {
            float extra = Port_DataCenterItemTravel(520, aspect);
            float previous = -1;
            /* Native first-row origin 583.3; include its left-side icon. */
            assert(placed(583.3f - 32 + extra, 256, aspect) > 512);
            for (frame = 0; frame <= 20; frame++) {
                float x = 520.0f * frame / 20;
                float shift = Port_DataCenterItemTravel(x, aspect);
                assert(shift >= previous);
                previous = shift;
            }
        }
        float previousGuide = -10000, previousPlate = 10000;
        /* Actual movie: guide -129 -> 11, rows 521 -> 171 in 14 steps.
         * Bulma's texture alpha ends at x=105; row art starts at x=-3. */
        assert(placed(-129 + 105, 0, aspect) < 0);
        assert(placed(521 - 3, 512, aspect) > 512);
        for (frame = 0; frame <= 14; frame++) {
            float guide = placed(-129 + frame * 10, 0, aspect);
            float plate = placed(521 - frame * 25, 512, aspect);
            assert(guide > previousGuide);
            assert(plate < previousPlate);
            previousGuide = guide;
            previousPlate = plate;
        }
        assert(placed(11 + 105, 0, aspect) < placed(171 - 3, 512, aspect));
    }
    return 0;
}
