#ifndef MENU_PORT_DATA_CENTER_ART_H
#define MENU_PORT_DATA_CENTER_ART_H

/* Data Center top menu, movie scope 61. Child eyes/mouths, captions and
 * selection icons inherit their complete guide/option group's anchor. */
static s32 Port_DataCenterArtMarker(const char *name) {
    if (strcmp(name, "mc_guide_blma") == 0) return 0x1A;
    if (strncmp(name, "mc_menu_plate_", 14) == 0) return 0x1B;
    return 0; /* mc_compane_* and standalone background shape 0 stay wide. */
}

/* 62 keyboard, 63 password results, 64 character list, 65 item panel,
 * 66 replay list. Keyboard controls retain Dragon Net Battle's wide layout. */
static s32 Port_DataCenterSubmenuMarker(s32 scope, const char *name) {
    if (strncmp(name, "mc_compane_", 11) == 0) return 0;
    if (strcmp(name, "mc_guide_blma") == 0) return 0x1A;
    if (scope == 62) return 0; /* complete keyboard/passcode rows stay wide */
    if (scope == 66) return strncmp(name, "mc_", 3) == 0 ? 0x19 : 0;
    if (strncmp(name, "mc_name_", 8) == 0 || strncmp(name, "mc_form_", 8) == 0) {
        s32 length = strlen(name);
        return length > 1 && name[length - 1] == 'l' ? 0x1A : 0x1B;
    }
    if (scope == 65) {
        if (strncmp(name, "mc_ability_limit_", 17) == 0) return 0x1B;
        return strncmp(name, "mc_", 3) == 0 ? 0x19 : 0;
    }
    if (strncmp(name, "mc_status_", 10) == 0 || strncmp(name, "mc_text_ability_", 16) == 0 ||
        strncmp(name, "menu_plate_", 11) == 0) return 0x1A;
    return strncmp(name, "mc_", 3) == 0 ? 0x1B : 0;
}

static s32 Port_DataCenterSubmenuImage(s32 scope, s32 image) {
    if (scope == 62) return 0; /* keyboard, input backing and page effects stay wide */
    if (scope == 65) {
        /* Same composition as Evolution Z scope 53, with different image IDs:
         * wide dimmer/backing, right capacity, centered item rows and icons. */
        if (image <= 2) return 0;
        if (image <= 6) return 0x1B;
        return 0x19;
    }
    if (image <= 6) return 0; /* background panel and mc_compane scenery */
    // TODO: Keep replay image 7's rounded black panel proportional while stretching its purple background; revisit the split to avoid visual artifacts.
    if (scope == 66) return image == 7 || image == 31 ? 0 : 0x19; /* wide outer frame */
    if (scope == 63 && image == 33) return 0x19; /* A New Warrior Appears! */
    if (scope == 63 && image >= 30 && image <= 32) return 0; /* page border/effects */
    if (image == 9 || (scope == 64 && (image == 14 || image == 26)) ||
        (scope == 63 && (image == 14 || image == 21))) return 0x1A;
    return 0x1B;
}

/* The same two backing shapes move between the lower-left reveal name and
 * upper-right registration name. Follow the active pair, not one fixed pivot. */
static s32 Port_DataCenterNamePlateMarker(f32 y) {
    return y > 150.0f ? 0x1A : 0x1B;
}

/* Item rows share the main backing's 520 -> 0 entrance. Center anchoring
 * otherwise pulls their parked poses into a wide page before the panel opens. */
static f32 Port_DataCenterItemTravel(f32 backingX, s32 aspect) {
    f32 travel;
    if (aspect <= 1333) return 0.0f;
    travel = backingX / 520.0f;
    if (travel < 0.0f) travel = 0.0f;
    if (travel > 1.0f) travel = 1.0f;
    return 256.0f * ((f32)aspect / 1333.333f - 1.0f) * travel;
}

#endif
