/* gcc -std=gnu11 -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined
 * port/tests/textures/wii_pack.c -Wl,--gc-sections -lm -o /tmp/wii_pack_test
 * /tmp/wii_pack_test */
#include <assert.h>
#include "../../src/gs/gs_draw.c"

static int ready, capacity = 10, creates, uploads;
int TexPack_Get(int id, TexPackImage *pack) {
    static uint8_t pixels[16];
    assert(id == 2);
    memset(pack, 0, sizeof(*pack));
    pack->w = pack->h = 2;
    pack->levels = 1;
    pack->data[0] = pixels;
    pack->bytes[0] = sizeof(pixels);
    return ready;
}
static int slots(void) { return capacity; }
static GsTex create(uint32_t w, uint32_t h, int format, int levels) {
    assert(w == 2 && h == 2 && format == 0 && levels == 1);
    creates++;
    return 123;
}
static int upload(GsTex tex, int level, uint32_t w, uint32_t h, int format, const void *px, uint32_t bytes) {
    assert(tex == 123 && level == 0 && bytes == 16);
    uploads++;
    free((void *)px);
    return 1;
}
int main(void) {
    GsBackend backend = {0};
    WiiTexturePack state = {0};
    backend.texCreate = create;
    backend.texUpload = upload;
    backend.texSlotsLeft = slots;
    sBackend = &backend;
    sTexPackOn = 1;
    assert(tbo_pack_texture(42, &state) == 42); /* No replacement. */
    state.entry = 3;
    assert(tbo_pack_texture(42, &state) == 42); /* Async read pending. */
    ready = -1;
    assert(tbo_pack_texture(42, &state) == 42); /* Unreadable file. */
    ready = 1; capacity = 0;
    assert(tbo_pack_texture(42, &state) == 42); /* Upload capacity exhausted. */
    capacity = 10;
    assert(tbo_pack_texture(42, &state) == 123);
    assert(creates == 1 && uploads == 1);
    assert(tbo_pack_texture(42, &state) == 123 && creates == 1);
    sTexPackOn = 0;
    assert(tbo_pack_texture(42, &state) == 42);
    sTexPackOn = 1;
    assert(tbo_pack_texture(42, &state) == 123 && creates == 1);
    puts("Wii replacement checks passed: asynchronous arrival, fallback, capacity, cache, pack toggle.");
}
