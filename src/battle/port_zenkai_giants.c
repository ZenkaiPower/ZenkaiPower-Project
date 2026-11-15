/* Runtime sizing for the active ZENKAI save; original assets remain untouched. */
#include "common.h"
#ifdef PORT
#include "sys/port_zenkai.h"
#include "battle/bobj_a.h"
extern f32 sqrtf(f32);
extern int Port_NetSession(void);
extern int Port_NetActive(void);
extern s32 EftCam_IsActive(void);

float Port_ZenkaiGiantScale(int type, const void *param) {
#if PORT_ZENKAI_GIANTS
    if (type == 0 && param != NULL && ((const u8 *)param)[2] == 4 &&
        Port_ZenkaiEnabled() && !Port_NetSession() && !Port_NetActive()) {
        return 2.5f;
    }
#endif
    return 1.0f;
}

/* Scripted technique cuts use the original authored model dimensions. Keep
 * gameplay dimensions separate, so cinematic transitions cannot alter them. */
float Port_ZenkaiGiantAnimationScale(int type, const void *param) {
    f32 scale = Port_ZenkaiGiantScale(type, param);
    if (scale != 1.0f && EftCam_IsActive()) return 1.0f;
    return scale;
}

/* Horizontal ray through the opponent's body point. Use the near-facing skin
 * of the animated hurt boxes, rather than a fixed circular radius. This keeps
 * approach and push-out in agreement with the geometry that melee can hit. */
float Port_ZenkaiGiantContactRadius(void *object, const void *target, float fallback) {
    BobjObj *obj = object;
    const Vec4 *point = target;
    BobjBodyPart *part;
    f32 dx, dz, length, radius = -1.0f;
    if (obj == NULL || point == NULL || obj->body.parts == NULL ||
        Port_ZenkaiGiantScale(obj->type, obj->mdl.param) == 1.0f) return fallback;
    dx = point->x - obj->xf.rootPos.x;
    dz = point->z - obj->xf.rootPos.z;
    length = sqrtf(dx * dx + dz * dz);
    if (length < 0.001f) return fallback;
    dx /= length;
    dz /= length;
    part = obj->body.parts;
    for (;;) {
        BobjObb *box = &part->box;
        f32 near = 0.0f, far = 1000000.0f;
        s32 i, intersects = 1;
        for (i = 0; i < 3; i++) {
            Vec4 *axis = &box->axis[i];
            f32 half = ((f32 *)&box->unk10)[i];
            f32 origin = (obj->xf.rootPos.x - box->center.x) * axis->x +
                         (point->y - box->center.y) * axis->y +
                         (obj->xf.rootPos.z - box->center.z) * axis->z;
            f32 direction = dx * axis->x + dz * axis->z;
            f32 axisLengthSq = axis->x * axis->x + axis->y * axis->y + axis->z * axis->z;
            /* A newly bound model may not have a computed collision pose yet. */
            if (half <= 0.0f || axisLengthSq < 0.5f) { intersects = 0; break; }
            if (direction > -0.00001f && direction < 0.00001f) {
                if (origin < -half || origin > half) { intersects = 0; break; }
            } else {
                f32 a = (-half - origin) / direction;
                f32 b = (half - origin) / direction;
                if (a > b) { f32 swap = a; a = b; b = swap; }
                if (a > near) near = a;
                if (b < far) far = b;
                if (near > far) { intersects = 0; break; }
            }
        }
        if (intersects && far > radius) radius = far;
        if (part->flags & 1) break;
        part++;
    }
    return radius > 0.0f ? radius : fallback;
}

extern BobjPart *BtlObj_GetNode(BobjObj *obj, s32 node);
int Port_ZenkaiGiantFocus(void *object, void *position) {
    BobjObj *obj = object;
    BobjPart *head;
    Vec4 *out = position;
    if (obj == NULL || out == NULL ||
        Port_ZenkaiGiantScale(obj->type, obj->mdl.param) == 1.0f) return 0;
    head = BtlObj_GetNode(obj, 0x30);
    if (head == NULL) return 0;
    /* Original lock-on focus is half the model height above its root.
     * Blend that body focus with the animated face, rather than the feet. */
    out->x = (obj->xf.rootPos.x + head->mtx.m[3][0]) * 0.5f;
    out->y = (obj->xf.rootPos.y - obj->body.size[0] * 0.5f + head->mtx.m[3][1]) * 0.5f;
    out->z = (obj->xf.rootPos.z + head->mtx.m[3][2]) * 0.5f;
    out->w = 1.0f;
    return 1;
}

extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern BobjObj *BtlObj_Get(s32 objId);
float Port_ZenkaiGiantEffectScale(int objId) {
    BobjObj *obj;
    if (objId < 0) return 1.0f;
    obj = BtlObj_Get(objId);
    return obj != NULL ? Port_ZenkaiGiantAnimationScale(obj->type, obj->mdl.param) : 1.0f;
}
int Port_ZenkaiGiantVsSmall(void *object) {
    BobjObj *obj = object;
    BobjObj *opponent;
    s32 id;
    if (obj == NULL || Port_ZenkaiGiantScale(obj->type, obj->mdl.param) == 1.0f) return 0;
    /* Fighter header: type, active, object id, character id. */
    id = BtlCharApi_GetOpponentObjId(((s32 *)obj)[2]);
    if (id < 0) return 0;
    opponent = BtlObj_Get(id);
    return opponent != NULL && opponent->type == 0 && opponent->mdl.param != NULL &&
           ((u8 *)opponent->mdl.param)[2] != 4;
}

/* Melee approach aligns animation heights, not the enlarged model's centre.
 * Using the visual height makes giants steer below small opponents or leaves
 * their original-height strike coverage above them at the stage floor. */
float Port_ZenkaiGiantApproachOffset(void *object) {
    BobjObj *obj = object;
    f32 height, scale;
    if (!Port_ZenkaiGiantVsSmall(obj)) return 0.0f;
    height = obj->body.size[0];
    scale = Port_ZenkaiGiantScale(obj->type, obj->mdl.param);
    return (height / scale - height) * 0.5f;
}
#endif
