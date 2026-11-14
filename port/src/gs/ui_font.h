#ifndef PORT_GS_UI_FONT_H
#define PORT_GS_UI_FONT_H
#include "imgui.h"
#include "../port_pack.h"

// Load font bytes through the virtual filesystem: release fonts live in fonts.pak.
static ImFont *ui_font_load(const char *path, float pixels) {
    FILE *fp = Port_PackFopen(path, "rb");
    if (!fp) return NULL;
    long size;
    if (fseek(fp, 0, SEEK_END) || (size = ftell(fp)) <= 0 || size > 16 * 1024 * 1024 || fseek(fp, 0, SEEK_SET)) {
        fclose(fp);
        return NULL;
    }
    void *data = IM_ALLOC((size_t)size);
    if (!data) { fclose(fp); return NULL; }
    bool read = fread(data, 1, (size_t)size, fp) == (size_t)size;
    fclose(fp);
    if (!read) { IM_FREE(data); return NULL; }
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(data, (int)size, pixels);
}

#endif
