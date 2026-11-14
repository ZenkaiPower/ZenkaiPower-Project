/* cc -std=c99 -Iinclude port/tests/ui/character_reference_art.c -o /tmp/character-reference-art */
#include <assert.h>
#include <string.h>
typedef int s32;
typedef float f32;
#include "menu/port_character_reference_art.h"
#include "menu/port_menu_art_policy.h"

static float place(float x, float pivot, int aspect) {
    return pivot + (x - pivot) * 1333.333333f / aspect;
}

int main(void) {
    int ratios[] = {1333, 1778, 2333, 3556};
    unsigned i;
    assert(Port_MenuArtModeCovered(60));
    assert(Port_MenuArtModeCovered(70)); /* Shenron wide scene */
    assert(Port_MenuArtModeCovered(71)); /* shared save dialogs */
    assert(Port_CharacterReferenceArtMarker("mc_guide_chichi") == 0x1A);
    assert(Port_CharacterReferenceArtMarker("mc_single_chara_l") == 0x1A);
    assert(Port_CharacterReferenceArtMarker("mc_ch_base_1") == 0x1A);
    assert(Port_CharacterReferenceArtMarker("mc_menu_plate2_1") == 0x1B);
    assert(Port_CharacterReferenceArtMarker("mc_menu_plate_5") == 0x1B);
    assert(Port_CharacterReferenceArtMarker("mc_name_text_1") == 0x1B);
    assert(Port_CharacterReferenceArtMarker("mc_form_text_1") == 0x1B);
    assert(Port_CharacterReferenceArtMarker("mc_voice_text_1") == 0x1B);
    assert(Port_CharacterReferenceArtMarker("mc_scroll_bar_point") == 0x1B);
    assert(Port_CharacterReferenceArtMarker("mc_dammy_text") == 0x19);
    assert(Port_CharacterReferenceArtMarker("mc_ch_gohan") == 0);
    assert(Port_CharacterReferenceImage(0) == 0x1A);
    assert(Port_CharacterReferenceGuideMarker(-39) == 0x1A);
    assert(Port_CharacterReferenceGuideMarker(-219) == 0x1A);
    assert(Port_CharacterReferenceGuideMarker(311) == 0x1B);
    assert(Port_CharacterReferenceGuideMarker(471) == 0x1B);
    assert(Port_CharacterReferenceImage(14) == 0x1B); /* top decoration shares list backing */
    assert(Port_CharacterReferenceImage(15) == 0x1B);
    assert(Port_CharacterReferenceImage(28) == 0x1B);
    assert(Port_CharacterReferenceImage(25) == 0); /* wide right-side dimmer */
    assert(Port_CharacterReferenceImage(26) == 0); /* full-width text frame */
    assert(Port_CharacterReferenceImage(27) == 0x19);
    assert(Port_CharacterReferenceImage(39) == 0);
    assert(Port_CharacterReferenceImage(40) == 0x19);
    for (i = 0; i < sizeof(ratios) / sizeof(*ratios); ++i) {
        /* Native parked portrait ends left of the page; action/list rows
         * begin beyond its right edge, even on ultrawide displays. */
        assert(place(-270 + 256, 0, ratios[i]) < 0);
        assert(place(531, 512, ratios[i]) > 512);
        assert(place(520, 512, ratios[i]) > 512);
        assert(place(256, 256, ratios[i]) == 256);
        /* Name text retains its native inset inside the backing. */
        assert(place(277.35f, 512, ratios[i]) > place(256.95f, 512, ratios[i]));
    }
    return 0;
}
