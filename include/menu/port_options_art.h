#ifndef MENU_PORT_OPTIONS_ART_H
#define MENU_PORT_OPTIONS_ART_H

/* The four two-choice button roots share centered, animated artwork. */
static s32 Port_OptionsMouseSelector(const char *name) {
    return strncmp(name,"mc_select_plate_",16)==0 &&
           name[16]>='1' && name[16]<='4' && name[17]==0;
}

/* Only control roots own hit bounds; nested icon clips must retain them. */
static s32 Port_OptionsMouseControl(const char *name) {
    return Port_OptionsMouseSelector(name) || strcmp(name,"mc_yajirusi_up")==0 || strcmp(name,"mc_yajirusi_down")==0 ||
           strcmp(name,"mc_yajirusi_left")==0 || strcmp(name,"mc_yajirusi_right")==0 ||
           strcmp(name,"mc_bgm_scroll_bar")==0;
}

/* Scope 68: the shared Options movie (main, screen, sound and controller). */
static s32 Port_OptionsArtMarker(const char *name) {
    if (strcmp(name, "mc_bgm_scroll_bar") == 0) return 0x13;
    if (strcmp(name, "mc_guide_dende") == 0 ||
        strncmp(name, "mc_bottom_plate_", 16) == 0 ||
        strncmp(name, "mc_bgm_", 7) == 0 ||
        strcmp(name, "mc_yajirusi_up") == 0 || strcmp(name, "mc_yajirusi_down") == 0) return 0x1A;
    if (strncmp(name, "mc_vol_", 7) == 0) return 0x19;
    if (strncmp(name, "mc_select_plate_", 16) == 0) return 0x19;
    /* Preserve the original 4:3 composition of the screen-position preview. */
    if (strcmp(name, "mc_layout_bg") == 0 || strcmp(name, "mc_target") == 0) return 0x19;
    if (strncmp(name, "mc_type_", 8) == 0 ||
        strcmp(name, "mc_layout_window") == 0) return 0x19;
    return strncmp(name, "mc_", 3) == 0 ? 0x1B : 0;
}

static s32 Port_OptionsImage(s32 image) {
    if (image == 35) return 0x13; /* page dimmer, including inside a menu tree */
    if ((image >= 21 && image <= 27) || (image >= 36 && image <= 38)) return 0x19; /* volume controls */
    if (image == 31 || image == 32) return 0x13; /* full-width BGM Test backing */
    if (image >= 28 && image <= 30) return 0x1A; /* proportional music list and scrollbar */
    if (image == 33) return 0x13; /* cyan indicator shares the stretched backing */
    if (image == 34 || image == 42 || image == 44) return 0x19; /* previews */
    if (image >= 39 && image <= 41) return 0x19; /* screen-adjustment preview */
    return 0x1B; /* other loose control backings share the right-side controls */
}

/* Extend the common 522 -> 148.65 volume-panel path by half the extra
 * virtual page width. All rows use this same shift so they stay together. */
static f32 Port_OptionsVolumeTravel(f32 windowX, s32 aspect) {
    f32 travel;
    if (aspect <= 1333) return 0.0f;
    travel = (windowX - 148.65f) / (522.0f - 148.65f);
    if (travel < 0.0f) travel = 0.0f;
    if (travel > 1.0f) travel = 1.0f;
    return 256.0f * ((f32)aspect / 1333.333333f - 1.0f) * travel;
}

/* Camera-type arrows share y=180 throughout entry, cycling and exit.
 * Their reused movie slots also serve volume/controller pages. */
static s32 Port_OptionsCameraArrow(f32 y) {
    return y > 179.0f && y < 181.0f;
}

/* Centered choices retain their native resting positions; extend both ends
 * of their horizontal entrance/exit paths beyond the wider visible page. */
static f32 Port_OptionsChoiceTravel(const char *name, f32 x, s32 aspect) {
    f32 travel;
    s32 left = strcmp(name, "mc_select_plate_1") == 0 || strcmp(name, "mc_select_plate_3") == 0;
    s32 right = strcmp(name, "mc_select_plate_2") == 0 || strcmp(name, "mc_select_plate_4") == 0;
    if (aspect <= 1333 || (!left && !right)) return 0.0f;
    travel = left ? (138.0f - x) / 310.0f : (x - 318.0f) / 200.0f;
    if (travel < 0.0f) travel = 0.0f;
    if (travel > 1.0f) travel = 1.0f;
    return (left ? -256.0f : 256.0f) * ((f32)aspect / 1333.333333f - 1.0f) * travel;
}

#endif
