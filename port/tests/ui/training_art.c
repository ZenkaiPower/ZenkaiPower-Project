/* cc -std=c99 -Iinclude port/tests/ui/training_art.c -lm -o /tmp/training-art && /tmp/training-art */
#include <assert.h>
#include <math.h>
#include <string.h>
typedef int s32;
typedef float f32;
#include "menu/port_training_art.h"
#include "menu/port_menu_art_policy.h"

static float placed(float x, float pivot, int aspect) {
    return pivot + (x - pivot) * (1333.333f / aspect);
}
int main(void) {
    const int ratios[] = {1333, 1778, 2333, 3556};
    unsigned i;
    assert(Port_MenuArtModeCovered(44));
    assert(Port_MenuArtModeCovered(45));
    assert(!Port_MenuArtModeCovered(46));
    assert(Port_TrainingArtMarker("mc_bg_cloud") == 0x13);
    assert(Port_TrainingArtMarker("mc_guide_saiyaman") == 0x1A);
    assert(Port_TrainingArtMarker("mc_guide_bidel") == 0x1A);
    assert(Port_TrainingArtMarker("mc_ch_bidel") == 0x1B);
    assert(Port_TrainingArtMarker("mc_ch_goten") == 0x1B);
    assert(Port_TrainingArtMarker("mc_menu_plate_2_1") == 0x1B);
    assert(Port_TrainingArtMarker("mc_yajirusi_down") == 0x1B);
    assert(Port_TrainingFlightRate(1333) == 1.0f);
    {
        const char *groups[] = {"mc_yajirusi_up", "mc_yajirusi_down", "mc_yajirusi_right"};
        const char *icons[] = {"mc_yajirusi_icon_up", "mc_yajirusi_icon_down", "mc_yajirusi_icon_right"};
        unsigned arrow, ratio;
        for (ratio = 0; ratio < sizeof(ratios)/sizeof(ratios[0]); ratio++) {
            int aspect = ratios[ratio];
            float listShift = Port_TrainingOptionsShift("mc_menu_plate_2_4", 173, aspect);
            for (arrow = 0; arrow < 3; arrow++) {
                assert(Port_TrainingOptionsShift(icons[arrow], 0, aspect) == 0);
                assert(fabsf(Port_TrainingOptionsShift(groups[arrow], 317, aspect) - listShift) < 0.001f);
            }
        }
    }
    assert(Port_TrainingRightBackingShift(898, 1778) == 0);
    assert(fabsf(Port_TrainingRightBackingShift(698, 1778) -
                 Port_TrainingOptionsShift("mc_menu_plate_1", 211.65f, 1778)) < 0.001f);
    assert(Port_TrainingRightBackingShift(698, 1333) == 0);
    assert(Port_TrainingRightBackingShift(698, 2333) == 0);
    {
        int tick;
        float last = -1;
        for (tick = 0; tick < 300; tick++) {
            float phase = 511 + tick * 0.047407407f * 0.75f;
            float rendered = (int)phase + Port_TrainingCloudFraction(phase) / 16.0f;
            assert(rendered >= last);
            assert(phase - rendered >= 0 && phase - rendered < 0.0626f);
            if (tick) assert(rendered - last <= 0.0626f);
            last = rendered;
        }
    }
    assert(Port_TrainingOptionsShift("mc_menu_plate_1", 531.65f, 1778) == 0);
    assert(Port_TrainingOptionsShift("mc_menu_plate_1", 211.65f, 1778) < 0);
    assert(Port_TrainingOptionsShift("mc_menu_plate_1", 211.65f, 2333) == 0);
    assert(Port_TrainingGuideX("mc_guide_bidel", -241, 1333) == -241);
    assert(Port_TrainingFlightShift(1, 320.1f, 1333) == 0);
    assert(Port_TrainingPreviewFrameShift(63, 1, 1333) == 0);
    for (i = 0; i < sizeof(ratios)/sizeof(ratios[0]); i++) {
        int aspect = ratios[i];
        if (aspect > 1333) {
            float stretch = 1 + (138 + 512 * ((float)aspect / 1333.333f - 1)) / 632.5f;
            assert(fabsf(stretch * Port_TrainingFlightRate(aspect) - 1) < 0.00001f);
        }
        float rest = Port_TrainingGuideX("mc_guide_bidel", -21, aspect);
        float mid = Port_TrainingGuideX("mc_guide_bidel", -131, aspect);
        float parked = Port_TrainingGuideX("mc_guide_bidel", -241, aspect);
        assert(rest == -21); /* settled layout stays native */
        assert(parked < mid && mid < rest); /* continuous entrance and reverse exit */
        assert(placed(-259 + 256, 0, aspect) < 0); /* complete Saiyaman quad */
        assert(placed(516.3f, 512, aspect) > 512); /* other Videl illustration */
        assert(-130.65f + 128 < 0); /* Goten parks above the page, Y stays unchanged */
        assert(placed(531.65f, 512, aspect) > 512); /* right-side menu rows */
        assert(Port_TrainingFlightShift(2, 320.1f, aspect) == 0); /* tree/cliff cutout has no flight shift */
        /* Equal native steps keep equal rendered steps across the whole flight,
         * including the former +/-200 acceleration boundaries and parked ends. */
        {
            int step;
            float last = -400 + Port_TrainingFlightShift(1, -400, aspect);
            float rate = ( -390 + Port_TrainingFlightShift(1, -390, aspect)) - last;
            for (step = -390; step <= 400; step += 10) {
                float next = step + Port_TrainingFlightShift(1, (float)step, aspect);
                assert(fabsf((next - last) - rate) < 0.001f);
                assert(next > last);
                last = next;
            }
        }
        if (aspect > 1333) {
            float right = Port_TrainingFlightShift(1, 320.1f, aspect);
            float left = Port_TrainingFlightShift(3, -312.4f, aspect);
            assert(placed(132.3f + 320.1f + right, 512, aspect) > 512);
            assert(placed(132.3f - 312.4f + 256 + left, 512, aspect) < 0);
            assert(placed(63 + Port_TrainingPreviewFrameShift(63, 1, aspect), 512, aspect) < 0);
            assert(placed(-449 + 256 + Port_TrainingPreviewFrameShift(-449, 0, aspect), 512, aspect) < 0);
            assert(Port_TrainingPreviewFrameShift(542, 1, aspect) == 0);
            assert(Port_TrainingPreviewFrameShift(30, 0, aspect) == 0);
            assert(placed(parked + 256, 0, aspect) < 0); /* complete Videl guide quad */
            float preview = Port_TrainingPreviewX(-449, aspect);
            int step;
            assert(placed(preview + 256, 512, aspect) < 0);
            assert(Port_TrainingPreviewX(30, aspect) == 158);
            for (step = 0; step <= 20; step++) {
                float frame = -449 + 479.0f * step / 20;
                float shift = Port_TrainingPreviewFrameShift(frame, 0, aspect);
                float image = Port_TrainingPreviewX(frame, aspect);
                /* Compare rendered centers, not merely native resting positions. */
                assert(fabsf(placed(image + 128, 512, aspect) -
                             placed(frame + shift + 256, 512, aspect)) < 0.0001f);
                assert(fabsf((image - frame - shift) - 128) < 0.0001f);
            }
        }
    }
    return 0;
}
