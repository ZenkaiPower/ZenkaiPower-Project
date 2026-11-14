/* Compile-time and machine-code checks for the PS2 layout on AArch64. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef struct {
    u8 * __ptr32 __uptr pointer;
    unsigned value;
} GameRecord;
_Static_assert(sizeof(GameRecord) == 8, "game pointer widened");
_Static_assert(__builtin_offsetof(GameRecord, value) == 4, "game field moved");

void abi_store_byte(u8 * __ptr32 __uptr p, unsigned v) { *p = (u8)v; }
void abi_store_half(u16 * __ptr32 __uptr p, unsigned v) { *p = (u16)v; }
float abi_add(float a, float b) { return a + b; }
