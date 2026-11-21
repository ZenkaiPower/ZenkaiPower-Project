#include "common.h"
#include "sys/pad.h"
#ifdef PORT
#include "battle/battle.h"
#include "menu/port_game_mouse.h"
#include "menu/port_pc_bindings.h"
#include "sys/save.h"
unsigned Port_PcActionMask(int action) {
    static const unsigned bits[]={0x2000,0x4000,0x8000,0x1000,0x400,0x100,0x800,0x200};
    static const int semantic[]={5,4,3,0,7,6,2,1};
    int i;
    if(!gSaveData || action<0 || action>=8) return 0;
    for(i=0;i<8;i++) if(gSaveData->key[0][i]==semantic[action]) return bits[i];
    return 0;
}
#include "sys/loading.h"
extern s32 BtlScript_IsTextShown(void);
extern s32 BtlSeq_IsFighting(void);
/* Enabled only around the boot logos and opening, after language/card prompts. */
int gPortBootSkipMouse;

/* Queried on the game thread before controller recording/network exchange. */
int Port_GameMouseMode(void) {
    BattleWork *battle = Battle_GetWork();
    if (gPortBootSkipMouse) return PORT_MOUSE_GAME_SCENE;
    if (gProgress != NULL && (gProgress->flags & PROGRESS_FLAG_LOADING))
        return PORT_MOUSE_GAME_LOADING;
    if (!battle->running || (battle->flags & (BATTLE_FLAG_LOADING |
                                            BATTLE_FLAG_LOAD_CHARA | BATTLE_FLAG_LOAD_OBJECT)))
        return PORT_MOUSE_GAME_OFF;
    if (battle->flags & BATTLE_FLAG_PAUSE) return PORT_MOUSE_GAME_PAUSED;
    if (Battle_GetMode() == 7) return PORT_MOUSE_GAME_SCENE; /* title attract battle */
    /* Story event tasks can live through combat. Only the demo flag marks
       their cinematic phase; task presence alone must not suppress attacks. */
    if (Battle_GetMode() == 1 && (battle->flags & BATTLE_FLAG_DEMO))
        return PORT_MOUSE_GAME_SCENE;
    /* Fighters also speak and display subtitles during normal combat. That
       text must not replace attack buttons with dialogue confirmation. */
    if (BtlSeq_IsFighting())
        return PORT_MOUSE_GAME_FIGHT;
    if (battle->flags & BATTLE_FLAG_DEMO)
        return PORT_MOUSE_GAME_SCENE;
    if (BtlScript_IsTextShown())
        return PORT_MOUSE_GAME_DIALOG;
    return PORT_MOUSE_GAME_FIGHT;
}
#ifdef __ANDROID__
#include "battle/btl_char_flag.h"
#include "sys/port_zenkai.h"
int Port_GameTouchCameraMode(void) {
    extern BtlFlagChr *BtlChar_Get(s32 side);
    extern s32 BattleSide_IsCpu(s32 side);
    extern s32 ChrCam_IsCutActive(void *fighter);
    extern s32 BtlChar_IsFrozen(void *fighter);
    BtlFlagChr *chr;
    if (Port_GameMouseMode() != PORT_MOUSE_GAME_FIGHT || !BtlSeq_IsFighting() ||
        !PORT_ZENKAI_FREE_CAMERA || !Port_ZenkaiPackEnabled() || BattleSide_IsCpu(0)) return 0;
    chr = BtlChar_Get(0);
    if (!chr || ChrCam_IsCutActive(chr) || BtlChar_IsFrozen(chr) ||
        BtlChar_TestFlag(chr,5) || BtlChar_TestFlag(chr,0x11E) ||
        BtlChar_TestFlag(chr,0x11F) || BtlChar_TestFlag(chr,0xDC) ||
        BtlChar_TestFlag(chr,0xB8) || BtlChar_TestFlag(chr,0xCC)) return 0;
    return 2; /* unlocked battle camera */
}
#endif

#endif

extern void *memset(void *dst, s32 value, u32 size);
extern f32 sqrtf(f32 x);

extern s32 sceDbcInit(void);
extern s32 scePad2Init(s32 mode);
extern s32 scePad2CreateSocket(Pad2SocketParam *param, void *work);
extern s32 scePad2Read(s32 socket, u8 *data);
extern s32 scePad2GetButtonProfile(s32 socket, void *profile);
extern s32 scePad2GetState(s32 socket);
extern s32 sceVibGetProfile(s32 socket, u8 *profile);
extern s32 sceVibSetActParam(s32 socket, s32 profileSize, u8 *profile, s32 dataSize, void *data);

extern u32 Pad_CalcRepeat(u32 held, u32 pressed, s32 *timer, u32 *last, s32 delay, s32 interval);
extern void Pad_UpdateGameButtons(s32 pad);

/* Starts libdbc/libpad2, opens one socket per controller port and sets the default key repeat. */
void Pad_Init(void) {
    Pad2SocketParam param;
    s32 i;

    sceDbcInit();
    scePad2Init(0);
    memset(gPad, 0, sizeof(gPad));

    memset(&param, 0, sizeof(param));
    param.option = 2;
    param.slot = 0;
    param.port = 0;
    gPad[0].socket = scePad2CreateSocket(&param, &gPad[0]);

    memset(&param, 0, sizeof(param));
    param.port = 1;
    param.option = 2;
    param.slot = 0;
    gPad[1].socket = scePad2CreateSocket(&param, &gPad[1]);

    for (i = 0; i < PAD_COUNT; i++) {
        gPad[i].repeatDelay = 20;
        gPad[i].repeatInterval = 1;
    }
}

/* Empty. */
void Pad_Stub0(void) {
}

/* Empty. */
void Pad_Stub1(void) {
}

/* Empty. */
void Pad_Stub2(void) {
}

/* Empty. */
void Pad_Stub3(void) {
}

/* Once per frame: polls both controllers, builds the stick values and the held/pressed/repeat words, sends vibration. */
void Pad_Update(void) {
    s32 i;
    Pad *pad;
    u32 btn;
    f32 len;
    f32 inv;

    pad = gPad;
    for (i = 0; i < PAD_COUNT; i++, pad++) {
        pad->state = scePad2GetState(pad->socket);
        if (pad->state == PAD_STATE_STABLE) {
            pad->status = 0;
            switch (pad->phase) {
            case PAD_PHASE_VIB_PROFILE:
                Pad_GetVibProfile(i);
                /* fall through */
            case PAD_PHASE_BTN_PROFILE:
                pad->profileSize = scePad2GetButtonProfile(pad->socket, pad->profile);
                if (pad->profileSize >= 0) {
                    pad->phase++;
                }
                break;
            case PAD_PHASE_READ:
                pad->dataSize = scePad2Read(pad->socket, pad->data);
                btn = ((pad->data[PAD_DATA_BTN_HI] << 8) | pad->data[PAD_DATA_BTN_LO]) ^ 0xFFFF;
                pad->right[0] = Pad_StickToFloat(pad->data[PAD_DATA_RX], PAD_STICK_DEADZONE);
                pad->right[1] = Pad_StickToFloat(pad->data[PAD_DATA_RY], PAD_STICK_DEADZONE);
                pad->left[0] = Pad_StickToFloat(pad->data[PAD_DATA_LX], PAD_STICK_DEADZONE);
                pad->left[1] = Pad_StickToFloat(pad->data[PAD_DATA_LY], PAD_STICK_DEADZONE);

                len = pad->right[0] * pad->right[0] + pad->right[1] * pad->right[1];
                if (1.0f < len) {
                    inv = 1.0f / sqrtf(len);
                    pad->right[0] *= inv;
                    pad->right[1] *= inv;
                }
                len = pad->left[0] * pad->left[0] + pad->left[1] * pad->left[1];
                if (1.0f < len) {
                    inv = 1.0f / sqrtf(len);
                    pad->left[0] *= inv;
                    pad->left[1] *= inv;
                }

                if (__builtin_fabsf(pad->left[0]) > __builtin_fabsf(pad->left[1])) {
                    if (0.5f < pad->left[0]) {
                        btn |= PAD_LSTICK_RIGHT;
                    }
                    if (pad->left[0] < -0.5f) {
                        btn |= PAD_LSTICK_LEFT;
                    }
                } else {
                    if (0.5f < pad->left[1]) {
                        btn |= PAD_LSTICK_DOWN;
                    }
                    if (pad->left[1] < -0.5f) {
                        btn |= PAD_LSTICK_UP;
                    }
                }
                if (__builtin_fabsf(pad->right[0]) > __builtin_fabsf(pad->right[1])) {
                    if (0.5f < pad->right[0]) {
                        btn |= PAD_RSTICK_RIGHT;
                    }
                    if (pad->right[0] < -0.5f) {
                        btn |= PAD_RSTICK_LEFT;
                    }
                } else {
                    if (0.5f < pad->right[1]) {
                        btn |= PAD_RSTICK_DOWN;
                    }
                    if (pad->right[1] < -0.5f) {
                        btn |= PAD_RSTICK_UP;
                    }
                }

#ifdef PORT
                /* Share the existing deadzone/dominant-axis decision with the
                 * D-pad during combat, before deriving pressed and repeat. */
                if (Port_GameMouseMode() == PORT_MOUSE_GAME_FIGHT) {
                    if (btn & PAD_LSTICK_UP) btn |= PAD_UP;
                    if (btn & PAD_LSTICK_DOWN) btn |= PAD_DOWN;
                    if (btn & PAD_LSTICK_LEFT) btn |= PAD_LEFT;
                    if (btn & PAD_LSTICK_RIGHT) btn |= PAD_RIGHT;
                }
#endif
                pad->held = btn;
                pad->pressed = btn & ~pad->prev;
                pad->prev = btn;
                pad->repeat = Pad_CalcRepeat(btn, pad->pressed, &pad->repeatTimer, &pad->repeatLast,
                                             pad->repeatDelay, pad->repeatInterval);
                /* A pad without analog sticks is treated as not connected. */
                if (Pad_IsDigitalOnly(i)) {
                    Pad_Reset(i);
                }
                Pad_UpdateGameButtons(i);
                break;
            }
        } else {
            pad->phase = 0;
            Pad_Reset(i);
        }
        Pad_SendActuator(i);
    }
}

/* True when any button of mask is down. */
s32 Pad_IsHeld(s32 pad, u32 mask) {
    return Pad_TestMask(pad, gPad[pad].held, mask);
}

/* True when any button of mask went down this frame. */
s32 Pad_IsPressed(s32 pad, u32 mask) {
    return Pad_TestMask(pad, gPad[pad].pressed, mask);
}

/* True when any button of mask went down or auto-repeated this frame. */
s32 Pad_IsRepeat(s32 pad, u32 mask) {
    return Pad_TestMask(pad, gPad[pad].repeat, mask);
}

/* Copies the left and right stick vectors (x, y in -1..1). */
void Pad_GetSticks(s32 pad, f32 *left, f32 *right) {
    left[0] = gPad[pad].left[0];
    left[1] = gPad[pad].left[1];
    right[0] = gPad[pad].right[0];
    right[1] = gPad[pad].right[1];
}

/* Adds to this frame's vibration request: large motor power (summed, capped at 1) and the small motor flag. */
void Pad_AddVibration(s32 pad, s32 small, f32 power) {
    Pad *p = &gPad[pad];

    p->vibPower += power;
    p->vibSmall |= small != 0;
    if (1.0f < p->vibPower) {
        p->vibPower = 1.0f;
    }
}

/* Reads the controller's actuator profile. */
s32 Pad_GetVibProfile(s32 pad) {
    Pad *p = &gPad[pad];

    return sceVibGetProfile(p->socket, p->vibProfile);
}

/* Sends the accumulated vibration request to the controller and clears it. */
void Pad_SendActuator(s32 pad) {
    Pad *p = &gPad[pad];
    s32 power = p->vibPower * 127.0f;

    if (power >= 0x80) {
        power = 0x7F;
    }
    if (power < 0) {
        power = 0;
    }
    power <<= 1;
    power |= p->vibSmall;
    p->vibData = power;
    sceVibSetActParam(p->socket, 1, p->vibProfile, 2, &p->vibData);
    p->vibPower = 0.0f;
    p->vibSmall = 0;
}

/* Puts a port into the "nothing connected" state: no buttons, centred sticks. */
void Pad_Reset(s32 pad) {
    Pad *p = &gPad[pad];

    gPad[pad].status = PAD_STATUS_NONE;
    gPad[pad].gameHeld = 0;
    gPad[pad].gamePressed = 0;
    gPad[pad].gameRepeat = 0;
    gPad[pad].gameRepeatTimer = 0;
    gPad[pad].gameRepeatLast = 0;
    memset(p->data, 0, sizeof(p->data));
    p->data[PAD_DATA_BTN_LO] = 0xFF;
    p->data[PAD_DATA_BTN_HI] = 0xFF;
    p->data[PAD_DATA_RX] = 0x80;
    p->data[PAD_DATA_RY] = 0x80;
    p->data[PAD_DATA_LX] = 0x80;
    p->data[PAD_DATA_LY] = 0x80;
    p->left[0] = 0.0f;
    p->left[1] = 0.0f;
    p->right[0] = 0.0f;
    p->right[1] = 0.0f;
    p->unk140 = 0;
    p->unk144 = 0;
    p->held = 0;
    p->prev = 0;
    p->pressed = 0;
    p->repeat = 0;
    p->repeatTimer = 0;
    p->repeatLast = 0;
}

/* True when the button profile is F9 FF 00 00: every digital button except L3/R3 and no analog input.
   (The four byte tests compile to one masked 64-bit compare.) */
s32 Pad_IsDigitalOnly(s32 pad) {
    Pad *p = &gPad[pad];

    return p->profile[0] == 0xF9 && p->profile[1] == 0xFF && p->profile[2] == 0 && p->profile[3] == 0;
}

/* True when bits and mask have a bit in common. */
s32 Pad_TestMask(s32 pad, u32 bits, u32 mask) {
    return (bits & mask) != 0;
}

/* Converts a raw stick byte to -1..1 with a dead zone around the centre (127.5). */
f32 Pad_StickToFloat(u8 raw, s32 deadzone) {
    f32 value = (f32)raw - 127.5f;

    if (0.0f < value) {
        value -= (f32)deadzone;
        if (value < 0.0f) {
            value = 0.0f;
        }
    } else {
        value += (f32)deadzone;
        if (0.0f < value) {
            value = 0.0f;
        }
    }
    return value / (127.5f - (f32)deadzone);
}
