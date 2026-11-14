#ifndef MENU_PORT_TRAINING_ART_H
#define MENU_PORT_TRAINING_ART_H

/* Ultimate Training movie scope 60. Anchor complete trees, including guide
 * facial animation and list callbacks, rather than their individual images. */
static s32 Port_TrainingArtMarker(const char *name) {
    if (strcmp(name, "mc_bg_cloud") == 0) return 0x13;
    if (strcmp(name, "mc_guide_saiyaman") == 0 ||
        strcmp(name, "mc_guide_bidel") == 0 ||
        strcmp(name, "mc_title_class") == 0) return 0x1A;
    return strncmp(name, "mc_", 3) == 0 ? 0x1B : 0;
}

/* Preserve native flight velocity over the extended offscreen-to-offscreen path. */
static f32 Port_TrainingFlightRate(s32 aspect) {
    if (aspect <= 1333) return 1.0f;
    return 632.5f / (632.5f + 138.0f +
        512.0f * ((f32)aspect / 1333.333f - 1.0f));
}

static f32 Port_TrainingOptionsShift(const char *name, f32 x, s32 aspect) {
    f32 travel = 1.0f;
    if (aspect <= 1333 || aspect > 1778) return 0.0f;
    if (strncmp(name, "mc_menu_plate_", 14) == 0) {
        travel = strcmp(name, "mc_menu_plate_2_4") == 0 ?
            (513.0f - x) / 340.0f : (531.65f - x) / 320.0f;
    } else if (strcmp(name, "mc_yajirusi_up") != 0 &&
               strcmp(name, "mc_yajirusi_down") != 0 &&
               strcmp(name, "mc_yajirusi_right") != 0 &&
               strcmp(name, "mc_icon_training_clear_in") != 0) return 0.0f;
    /* The arrow icon inherits this group shift. Matching its mc_yajirusi_icon_*
     * child as well would displace the chevron twice from the list centre. */
    if (travel < 0.0f) travel = 0.0f;
    if (travel > 1.0f) travel = 1.0f;
    return -512.0f * ((f32)aspect / 1333.333f - 1.0f) * travel;
}

/* Keep the mirrored backing connected to the emblem at rest, extending its
 * right-entry travel so the parked shape remains outside the physical page. */
static f32 Port_TrainingRightBackingShift(f32 x, s32 aspect) {
    f32 travel;
    if (aspect <= 1333 || aspect > 1778) return 0.0f;
    travel = (898.0f - x) / 200.0f;
    if (travel < 0.0f) travel = 0.0f;
    if (travel > 1.0f) travel = 1.0f;
    return -512.0f * ((f32)aspect / 1333.333f - 1.0f) * travel;
}

static s32 Port_TrainingCloudFraction(f32 offset) {
    /* GS UV coordinates retain four fractional bits. Do not round up across
     * the integer boundary: FlashAnim_Scroll already supplies the integer part. */
    return (s32)((offset - (s32)offset) * 16.0f);
}

static f32 Port_TrainingGuideX(const char *name, f32 x, s32 aspect) {
    if (aspect > 1333 && strcmp(name, "mc_guide_bidel") == 0) {
        /* Native Videl parks at -241 with a 256-wide portrait. Move that last
         * 15-unit strip outside too, tapering to zero at her resting x=-21.
         * The same position-based correction works for entry and reverse exit. */
        f32 travel = (-21.0f - x) / 220.0f;
        if (travel < 0.0f) travel = 0.0f;
        if (travel > 1.0f) travel = 1.0f;
        x -= 16.0f * travel;
    }
    return x;
}

/* Images 1 and 3 are looping flying Goten copies whose timelines park at
 * x=320.1 / about -312 inside the parent. Image 2 is the tree/cliff scenery
 * cutout and must stay aligned with the full-width background. */
static f32 Port_TrainingFlightShift(s32 image, f32 x, s32 aspect) {
    f32 progress, leftShift;
    if (aspect <= 1333 || (image != 1 && image != 3)) return 0.0f;
    /* Map the complete flight between its two offscreen endpoints. A single
     * affine mapping preserves the authored easing everywhere; adding travel
     * only near the edges made him abruptly accelerate on exit. Do not clamp:
     * frames beyond these endpoints must retain the same continuous speed. */
    progress = (x + 312.4f) / (320.1f + 312.4f);
    leftShift = -(77.0f + 512.0f * ((f32)aspect / 1333.333f - 1.0f));
    return leftShift + (61.0f - leftShift) * progress;
}

static f32 Port_TrainingPreviewFrameShift(f32 x, s32 mirrored, s32 aspect) {
    f32 rest = mirrored ? 542.0f : 30.0f;
    f32 travel;
    if (aspect <= 1333) return 0.0f;
    travel = (rest - x) / 479.0f;
    if (travel < 0.0f) travel = 0.0f;
    if (travel > 1.0f) travel = 1.0f;
    /* Both halves move together from the left, including the mirrored half
     * whose translation (63 -> 542) superficially resembles a right entry. */
    return -(64.0f + 512.0f * ((f32)aspect / 1333.333f - 1.0f)) * travel;
}

/* The 256-wide image sits 128 units inside the complete 512-wide frame.
 * Share the frame's correction rather than independently adjusting the photo. */
static f32 Port_TrainingPreviewX(f32 frameX, s32 aspect) {
    return frameX + 128.0f + Port_TrainingPreviewFrameShift(frameX, 0, aspect);
}

#endif
