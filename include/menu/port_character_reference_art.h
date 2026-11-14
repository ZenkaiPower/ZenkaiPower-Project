#ifndef MENU_PORT_CHARACTER_REFERENCE_ART_H
#define MENU_PORT_CHARACTER_REFERENCE_ART_H

/* Scope 67: archive 9's list, details, introduction, voice and costume pages.
 * Anchor complete trees so nested captions and guide facial animation agree.
 * Edge pivots also keep the movie's native parked poses outside the page. */
static s32 Port_CharacterReferenceArtMarker(const char *name) {
    if (strcmp(name, "mc_guide_chichi") == 0 ||
        strcmp(name, "mc_single_chara_l") == 0 ||
        strncmp(name, "mc_ch_base_", 11) == 0 ||
        strcmp(name, "mc_yajirusi_left") == 0 ||
        strcmp(name, "mc_yajirusi_right") == 0) return 0x1A;
    if (strcmp(name, "mc_dammy_text") == 0 ||
        strcmp(name, "mc_icon_voice_play") == 0) return 0x19;
    if (strncmp(name, "mc_menu_plate", 13) == 0 ||
        strncmp(name, "mc_name_text_", 13) == 0 ||
        strncmp(name, "mc_form_text_", 13) == 0 ||
        strncmp(name, "mc_voice_text_", 14) == 0 ||
        strncmp(name, "mc_scroll_", 10) == 0 ||
        strcmp(name, "mc_yajirusi_up") == 0 ||
        strcmp(name, "mc_yajirusi_down") == 0) return 0x1B;
    return 0; /* library scenery, including mc_ch_gohan, stays wide */
}

static s32 Port_CharacterReferenceImage(s32 image) {
    if (image <= 2) return 0x1A; /* portrait and both frame layers */
    if (image >= 14 && image <= 23) return 0x1B; /* list and scrollbar */
    if (image >= 28 && image <= 31) return 0x1B; /* name/form/voice plates */
    /* Image 25 dims the right half of the page; image 26 is the pair of
     * mirrored introduction-frame halves. Both must fill their native screen
     * span, while the separate text callback keeps proportional glyphs. */
    if (image == 25 || image == 26) return 0;
    if (image == 24 || image == 27) return 0x19;
    if (image == 40) return 0x19; /* blue heading; image 39 is its wide strip */
    return 0;
}

/* The same guide tree enters from opposite edges in the list and commentary.
 * Follow its animated placement, including the commentary exit after its
 * state has already returned to the detail menu. */
static s32 Port_CharacterReferenceGuideMarker(f32 x) {
    return x >= 256.0f ? 0x1B : 0x1A;
}

#endif
