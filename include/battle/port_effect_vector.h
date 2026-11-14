#ifndef BATTLE_PORT_EFFECT_VECTOR_H
#define BATTLE_PORT_EFFECT_VECTOR_H
#include "sys/math3d.h"
#ifdef PORT
/* Android's soft-float ARM64 ABI aligns these by-value effect arguments to
   even register pairs, matching the modules' 16-byte-aligned vector types.
   Keep ordinary Vec4 storage unchanged, and copy rather than casting a
   potentially unaligned input pointer. Desktop retains its existing ABI. */
#ifdef __ANDROID__
typedef struct PortEffectVector {
    f32 x, y, z, w;
} __attribute__((aligned(16))) PortEffectVector;
#else
typedef Vec4 PortEffectVector;
#endif
static PortEffectVector Port_EffectVector(const Vec4 *v) {
    PortEffectVector value = {v->x, v->y, v->z, v->w};
    return value;
}
#endif
#endif
