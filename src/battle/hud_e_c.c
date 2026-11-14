#include "common.h"
#include "battle/hud_e.h"
#include "battle/battle.h"
#include "sys/heap.h"

/*
 * Battle HUD, timer part, 0x22EC08-0x22F998: the clock at the top of the screen (or mode 3's counter).
 * Two nodes over six sprites of sheet 0. Hud_PreUpdate hands over the value every frame (HudTimer_SetTime /
 * HudTimer_SetCount); the node update lays the digits out. The pulse and the slide are frozen by the pause flag.
 */

#define HUDT_PAUSED() (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)

/* Sprite / node library at 0x224B50.. (neighbouring ranges). */
extern void HudSprite_Show(HudESprite *spr, s32 show);
extern void HudSprite_SetRect(HudESprite *spr, s32 x0, s32 x1, s32 y0, s32 y1); /* screen rectangle */
extern void HudSprite_SetUv(HudESprite *spr, s32 u0, s32 u1, s32 v0, s32 v1); /* texel rectangle */
extern void HudSprite_SetTex(HudESprite *spr, s32 tex, s32 sub);               /* texture entry and sub entry */
extern void HudSprite_SetMirror(HudESprite *spr, s32 mirror);
extern void HudSprite_Move(HudESprite *spr, s32 dx, s32 dy);                 /* moves the rectangle */
extern void HudSprite_Scale(HudESprite *spr, f32 sx, f32 sy);                 /* scales the rectangle */
extern void HudSprite_Center(HudESprite *spr);                                 /* centres the rectangle */
extern void HudSprite_InitTex(HudESprite *spr, HudERes *res, s32 tex, s32 sub); /* sprite of a sheet's texture */
extern void HudSprite_Draw(HudESprite *spr, HudERes *res, s32 additive);     /* HudSprite_Draw */
extern void HudNode_SetPos(HudENode *node, s32 x, s32 y);                    /* HudNode_SetPos */
extern void *memset(void *dst, s32 c, u32 n);
#ifdef PORT
extern int Port_NetStats(int *out);
extern int Port_NetJitterMs(void);
#endif

#define HUDT_SPR_COUNT 7
#define HUDT_ONLINE_FRAME 6

/* Online connection panel tuning. */
#define HUDT_ONLINE_SCALE    0.55f /* panel size vs. the original */
/*
 * Position is relative to the timer frame:
 * - place the connection panel ABOVE the timer with a tiny (12.5%) overlap
 *   at their touching edges, to avoid clipping against the screen top;
 * - place the connection percentage toward the top of that background.
 */
#define HUDT_ONLINE_TOP_OVERLAP 0.125f
#define HUDT_ONLINE_Y_OFFSET    2  /* slightly lower than the original layout */
#define HUDT_TEXT_VPOS          0.40f
#define HUDT_TEXT_SCALE      0.40f
#define HUDT_TEXT_HALF_H     4.35f
#define HUDT_TEXT_ALPHA      0x59  /* 70% opacity: HUD font full alpha is 0x80 */

HudTimer *gHudTimer = NULL;
static s32 sHudTimerConnectionPercent = -1;
static void HudTimer_UpdateConnection(void);
static void HudTimer_LayoutConnectionFrame(void);

extern s32 Font_GetCmdCount(void);
extern void Font_Flush(s32 first);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetScale(f32 scale);
extern void Font_SetAlign(s32 align);
extern void Font_SetShadowMode(s32 mode);
extern void Font_SetShadowColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_PrintAt(s32 x, s32 y, u16 *str);

/* Root node update: steps the slide and moves the part up by 224 * slide. */
void HudTimer_UpdateRoot(HudENode *node) {
    if (!HUDT_PAUSED()) {
        Ramp_Step(&gHudTimer->slide);
    }
    HudNode_SetPos(node, gHudTimer->slide.value * 0.0f, gHudTimer->slide.value * -224.0f);
}

/* Texel rectangle of digit d: 32x32 cells, four per row. */
#define HUDT_DIGIT_UV(spr, d) HudSprite_SetUv(spr, ((d) % 4) * 32, ((d) % 4) * 32 + 32, ((d) / 4) * 32, ((d) / 4) * 32 + 32)

/* Clock node update: lays out up to three digits 11 pixels apart, centred (a negative value shows the two halves
   of the "infinite" picture). A clock is drawn with sub texture 1 below 10 seconds and 2 below 4, where each
   change of the value also starts a 0.5 s pulse of the digits' size (1 + pulse, about the point 16 below the
   baseline). */
void HudTimer_UpdateDigits(void) {
    s32 x = 0;
    Ramp *pulse = &gHudTimer->pulse;
    s32 value = gHudTimer->value;
    HudESprite *spr;
    s32 d;
    s32 w;

    if (value >= 1000) {
        value = 999;
    }
    if (value < 0) {
        spr = &gHudTimer->spr[1];
        HudSprite_Show(spr, 1);
        HudSprite_SetRect(spr, 0, 32, -32, 0);
        HudSprite_SetUv(spr, 96, 128, 64, 96);
        HudSprite_Move(spr, 1, 0);
        spr = &gHudTimer->spr[2];
        HudSprite_Show(spr, 1);
        HudSprite_SetRect(spr, -32, 0, -32, 0);
        HudSprite_SetUv(spr, 64, 96, 64, 96);
        HudSprite_Move(spr, 1, 0);
        HudSprite_Show(&gHudTimer->spr[3], 0);
    } else {
        spr = &gHudTimer->spr[3];
        {
            s32 hundred = 100;

            d = value / hundred;
        }
        if (d == 0) {
            HudSprite_Show(spr, 0);
        } else {
            HudSprite_Show(spr, 1);
            HudSprite_SetRect(spr, 0, 32, -32, 0);
            HUDT_DIGIT_UV(spr, d);
            x = 11;
        }
        spr = &gHudTimer->spr[2];
        {
            s32 hundred = 100;
            s32 ten = 10;

            d = value % hundred / ten;
        }
        if (x == 0 && d == 0) {
            HudSprite_Show(spr, 0);
        } else {
            HudSprite_Show(spr, 1);
            HudSprite_SetRect(spr, x, x + 32, -32, 0);
            HUDT_DIGIT_UV(spr, d);
            x += 11;
        }
        spr = &gHudTimer->spr[1];
        {
            s32 ten = 10;

            d = value % ten;
        }
        HudSprite_Show(spr, 1);
        HudSprite_SetRect(spr, x, x + 32, -32, 0);
        HUDT_DIGIT_UV(spr, d);
        HudSprite_Move(&gHudTimer->spr[1], w = -(x + 32) / 2, 0);
        HudSprite_Move(&gHudTimer->spr[2], w, 0);
        HudSprite_Move(&gHudTimer->spr[3], w, 0);
    }
    if (gHudTimer->isClock) {
        if ((u32)value < 4) {
            s32 ofs[2] = { 0, 16 };
            f32 scale;

            if (value != gHudTimer->prev) {
                Ramp_Start(pulse, 0.5f, 1.0f, 0.0f);
            }
            scale = pulse->value + 1.0f;
            spr = &gHudTimer->spr[1];
            HudSprite_SetTex(spr, 0, 2);
            HudSprite_Move(spr, ofs[0], ofs[1]);
            HudSprite_Scale(spr, scale, scale);
            HudSprite_Move(spr, -ofs[0], -ofs[1]);
            spr = &gHudTimer->spr[2];
            HudSprite_SetTex(spr, 0, 2);
            HudSprite_Move(spr, ofs[0], ofs[1]);
            HudSprite_Scale(spr, scale, scale);
            HudSprite_Move(spr, -ofs[0], -ofs[1]);
            spr = &gHudTimer->spr[3];
            HudSprite_SetTex(spr, 0, 2);
            HudSprite_Move(spr, ofs[0], ofs[1]);
            HudSprite_Scale(spr, scale, scale);
            HudSprite_Move(spr, -ofs[0], -ofs[1]);
            if (!HUDT_PAUSED()) {
                Ramp_Step(pulse);
            }
        } else if ((u32)value < 10) {
            HudSprite_SetTex(&gHudTimer->spr[1], 0, 1);
            HudSprite_SetTex(&gHudTimer->spr[2], 0, 1);
            HudSprite_SetTex(&gHudTimer->spr[3], 0, 1);
        } else {
            HudSprite_SetTex(&gHudTimer->spr[1], 0, 0);
            HudSprite_SetTex(&gHudTimer->spr[2], 0, 0);
            HudSprite_SetTex(&gHudTimer->spr[3], 0, 0);
        }
    } else {
        HudSprite_SetTex(&gHudTimer->spr[1], 0, 0);
        HudSprite_SetTex(&gHudTimer->spr[2], 0, 0);
        HudSprite_SetTex(&gHudTimer->spr[3], 0, 0);
    }
    HudSprite_Move(&gHudTimer->spr[1], -2, 4);
    HudSprite_Move(&gHudTimer->spr[2], -2, 4);
    HudSprite_Move(&gHudTimer->spr[3], -2, 4);
    HudSprite_Show(&gHudTimer->spr[5], gHudTimer->mark);
    HudTimer_UpdateConnection();
}

/*
 * Attach the online frame to the ACTUAL timer frame.
 *
 * Both sprites are local to node[1], so no absolute screen coordinate is
 * needed here.  The frame remains horizontally centred on the timer and its
 * bottom edge lightly overlaps the timer's top edge.
 */
static void HudTimer_LayoutConnectionFrame(void) {
    HudESprite *timer = &gHudTimer->spr[0];
    HudESprite *online = &gHudTimer->spr[HUDT_ONLINE_FRAME];
    s32 width = online->pos[1] - online->pos[0];
    s32 height = online->pos[3] - online->pos[2];
    s32 timerCenter = (timer->pos[0] + timer->pos[1]) / 2;
    s32 left = timerCenter - width / 2;
    s32 top = timer->pos[2] - height +
              (s32)((f32)height * HUDT_ONLINE_TOP_OVERLAP) + HUDT_ONLINE_Y_OFFSET;

    HudSprite_SetRect(online, left, left + width, top, top + height);
}

/* Draw callback: every sprite of the node. */
void HudTimer_Draw(HudENode *node) {
    u32 i;

    for (i = 0; i < node->sprCount; i++) {
        HudSprite_Draw(node->sprList[i], gHudTimer->res, 0);
    }
    if (sHudTimerConnectionPercent >= 0) {
        u16 text[6];
        s32 first = Font_GetCmdCount();
        s32 value = sHudTimerConnectionPercent;

        if (value >= 100) {
            text[0] = '1';
            text[1] = '0';
            text[2] = '0';
            text[3] = '%';
            text[4] = 0;
        } else if (value >= 10) {
            text[0] = (u16)('0' + value / 10);
            text[1] = (u16)('0' + value % 10);
            text[2] = '%';
            text[3] = 0;
        } else {
            text[0] = '0';
            text[1] = (u16)('0' + value);
            text[2] = '%';
            text[3] = 0;
        }
        Font_PushStyle();
        Font_SetScale(HUDT_TEXT_SCALE);
        Font_SetAlign(1);
        Font_SetColorRGBA(0xFF, 0xFF, 0xFF, HUDT_TEXT_ALPHA);
        Font_SetShadowMode(0);
        {
            HudESprite *online = &gHudTimer->spr[HUDT_ONLINE_FRAME];
            HudENode *root = &gHudTimer->node[0];
            HudENode *clock = &gHudTimer->node[1];
            s32 bgHeight = online->pos[3] - online->pos[2];
            s32 x = root->x + root->ofsX + clock->x + clock->ofsX +
                    (online->pos[0] + online->pos[1]) / 2;
            s32 y = root->y + root->ofsY + clock->y + clock->ofsY +
                    online->pos[2] +
                    (s32)((f32)bgHeight * HUDT_TEXT_VPOS - HUDT_TEXT_HALF_H);

            Font_PrintAt(x, y, text);
        }
        Font_PopStyle();
        Font_Flush(first);
    }
}

/* The connection score is display-only and uses live network diagnostics. */
static void HudTimer_UpdateConnection(void) {
#ifdef PORT
    int stats[10];
    int score, ping, jitter, waits, waitedMs, rollbacks;

    if (!Port_NetStats(stats)) {
        HudSprite_Show(&gHudTimer->spr[HUDT_ONLINE_FRAME], 0);
        sHudTimerConnectionPercent = -1;
        return;
    }
    ping = stats[0];
    if (ping < 0) {
        HudSprite_Show(&gHudTimer->spr[HUDT_ONLINE_FRAME], 1);
        sHudTimerConnectionPercent = -1;
        return;
    }
    jitter = Port_NetJitterMs();
    rollbacks = stats[1];
    waits = stats[3];
    waitedMs = stats[4];
    /* Wait impact dominates, followed by jitter; ping and rollback frequency refine the result. */
    score = (70 * (100 - (waitedMs > 1000 ? 100 : waitedMs / 10)) +
             15 * (100 - (jitter > 50 ? 100 : jitter * 2)) +
             10 * (100 - (ping > 250 ? 100 : ping * 100 / 250)) +
              5 * (100 - (rollbacks > 10 ? 100 : rollbacks * 10))) / 100;
    /* Frequent short waits also matter even when their total time is small. */
    if (waits > 30) score -= (waits - 30) / 3;
    if (score < 0) score = 0;
    HudSprite_Show(&gHudTimer->spr[HUDT_ONLINE_FRAME], 1);
    sHudTimerConnectionPercent = score;
#endif
}

/* Slides the part off the screen over `seconds`. */
void HudTimer_SlideOut(f32 seconds) {
    Ramp_Start(&gHudTimer->slide, seconds, 0.0f, 1.0f);
}

/* Slides it back. */
void HudTimer_SlideIn(f32 seconds) {
    Ramp_Start(&gHudTimer->slide, seconds, 1.0f, 0.0f);
}

/* Builds the part: the work, six sprites and two nodes (root, and the clock at (256, 37) with all six sprites). */
void HudTimer_Init(HudENode **out, HudERes *res) {
    HudESprite *spr;
    HudENode *node;
    s32 i;

    gHudTimer = Heap_Alloc(sizeof(HudTimer), 0x20, 0, 2);
    memset(gHudTimer, 0, sizeof(HudTimer));
    gHudTimer->spr = Heap_Alloc(HUDT_SPR_COUNT * sizeof(HudESprite), 0x20, 0, 2);
    memset(gHudTimer->spr, 0, HUDT_SPR_COUNT * sizeof(HudESprite));
    gHudTimer->node = Heap_Alloc(2 * sizeof(HudENode), 0x20, 0, 2);
    memset(gHudTimer->node, 0, 2 * sizeof(HudENode));
    gHudTimer->res = res;

    spr = &gHudTimer->spr[0];
    HudSprite_InitTex(spr, res, 3, 0);
    HudSprite_SetRect(spr, 0, 64, 0, 30);
    HudSprite_SetUv(spr, 0, 64, 0, 30);
    HudSprite_Center(spr);
    HudSprite_Move(spr, 0, -7);

    spr = &gHudTimer->spr[1];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_SetRect(spr, -10, 22, -16, 16);
    HudSprite_SetUv(spr, 0, 32, 0, 32);

    spr = &gHudTimer->spr[5];
    HudSprite_InitTex(spr, res, 5, 0);
    HudSprite_Center(spr);
    HudSprite_Move(spr, 0, 16);

    spr = &gHudTimer->spr[2];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_SetRect(spr, -22, 10, -16, 16);
    HudSprite_SetUv(spr, 0, 32, 0, 32);

    spr = &gHudTimer->spr[3];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_SetRect(spr, -22, 10, -16, 16);
    HudSprite_SetUv(spr, 0, 32, 0, 32);

    spr = &gHudTimer->spr[4];
    HudSprite_InitTex(spr, res, 4, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);
    HudSprite_Move(spr, 0, 26);

    /* Online connection panel: upright background above the timer, with a small edge overlap. */
    spr = &gHudTimer->spr[HUDT_ONLINE_FRAME];
    HudSprite_InitTex(spr, res, 3, 0);
    HudSprite_SetRect(spr, 0, 64, 0, 30);
    HudSprite_SetUv(spr, 0, 64, 0, 30);
    HudSprite_Center(spr);
    HudSprite_Scale(spr, HUDT_ONLINE_SCALE, HUDT_ONLINE_SCALE);
    HudTimer_LayoutConnectionFrame();
    HudSprite_SetMirror(spr, 0);
    HudSprite_Show(&gHudTimer->spr[HUDT_ONLINE_FRAME], 0);

    node = &gHudTimer->node[1];
    node->x = 256;
    node->y = 37;
    node->sprCount = HUDT_SPR_COUNT;
    node->sprList = Heap_Alloc(node->sprCount * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudTimer->spr[0];
    node->sprList[1] = &gHudTimer->spr[1];
    node->sprList[2] = &gHudTimer->spr[2];
    node->sprList[3] = &gHudTimer->spr[3];
    node->sprList[4] = &gHudTimer->spr[4];
    node->sprList[5] = &gHudTimer->spr[5];
    for (i = 6; i < HUDT_SPR_COUNT; i++) node->sprList[i] = &gHudTimer->spr[i];
    node->update = HudTimer_UpdateDigits;
    node->draw = HudTimer_Draw;

    node = &gHudTimer->node[0];
    node->x = 0;
    node->y = 0;
    node->childCount = 1;
    node->childList = Heap_Alloc(node->childCount * sizeof(HudENode *), 0x20, 0, 2);
    memset(node->childList, 0, node->childCount * sizeof(HudENode *));
    node->childList[0] = &gHudTimer->node[1];
    node->update = HudTimer_UpdateRoot;
    node->draw = NULL;

    *out = &gHudTimer->node[0];
}

/* Frees the part. */
void HudTimer_Term(void) {
    s32 i;

    if (gHudTimer->spr != NULL) {
        Heap_Free(gHudTimer->spr);
    }
    for (i = 0; i < 2; i++) {
        if (gHudTimer->node[i].sprList != NULL) {
            Heap_Free(gHudTimer->node[i].sprList);
        }
        if (gHudTimer->node[i].childList != NULL) {
            Heap_Free(gHudTimer->node[i].childList);
        }
    }
    if (gHudTimer->node != NULL) {
        Heap_Free(gHudTimer->node);
    }
    if (gHudTimer != NULL) {
        Heap_Free(gHudTimer);
    }
}

/* Sets the seconds left and shows the clock's frame (64x30 at the top of texture 3); hides the counter's label. */
void HudTimer_SetTime(s32 seconds) {
    HudESprite *spr;

    gHudTimer->prev = gHudTimer->value;
    gHudTimer->value = seconds;
    gHudTimer->isClock = 1;
    spr = &gHudTimer->spr[0];
    HudSprite_SetRect(spr, 0, 64, 0, 30);
    HudSprite_SetUv(spr, 0, 64, 0, 30);
    HudSprite_Center(spr);
    HudSprite_Move(spr, 0, -7);
    HudTimer_LayoutConnectionFrame();
    HudSprite_Show(&gHudTimer->spr[4], 0);
}

/* Sets a count shown in place of the clock (mode 3: opponents left): the other frame (64x32 at v = 32) and the
   label. */
void HudTimer_SetCount(s32 count) {
    HudESprite *spr;

    gHudTimer->prev = count;
    gHudTimer->value = count;
    gHudTimer->isClock = 0;
    spr = &gHudTimer->spr[0];
    HudSprite_SetRect(spr, 0, 64, 0, 32);
    HudSprite_SetUv(spr, 0, 64, 32, 64);
    HudSprite_Center(spr);
    HudSprite_Move(spr, 0, -6);
    HudTimer_LayoutConnectionFrame();
    HudSprite_Show(&gHudTimer->spr[4], 1);
}

/* Shows sprite 5 from now on. Called by the stage (stg_a_b.c). */
void HudTimer_ShowMark(void) {
    gHudTimer->mark = 1;
}

/* The same; no caller. */
void HudTimer_ShowMark2(void) {
    gHudTimer->mark = 1;
}

/* Round reset: value 0, mark off, both ramps cleared. */
void HudTimer_Reset(void) {
    gHudTimer->value = 0;
    gHudTimer->prev = 0;
    gHudTimer->mark = 0;
    memset(&gHudTimer->pulse, 0, sizeof(Ramp));
    memset(&gHudTimer->slide, 0, sizeof(Ramp));
}
