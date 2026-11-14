/* Compile with -ffunction-sections -fdata-sections -Wl,--gc-sections -pthread. */
#include "../../src/gs/menu_translation.c"
#include <assert.h>

int MenuText_FirstFontId(int setting) { (void)setting; return 0; }

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "port/translations/labels/en.json";
    int ok = tr_read(path, "en");
    fprintf(stderr, "catalog=%s loaded=%d rows=%d groups=%d\n", path, ok, sNRows, sNGroups);
    assert(ok);
    assert(!strcmp(tr_find("fighter-actions-selected/0/0")->resolved, "Z Item"));
    assert(!strcmp(tr_find("fighter-actions-selected/0/1")->resolved, "Password"));
    assert(!strcmp(tr_find("fighter-actions-selected/0/2")->resolved, "Erase Data"));
    tr_clear();
    {
        const char missing[] = "{\"schema_version\":1,\"locale\":\"en\",\"labels\":{"
            "\"test/0/0\":{\"original\":\"Test\",\"text_from\":\"absent/0/0\"}}}";
        const char cycle[] = "{\"schema_version\":1,\"locale\":\"en\",\"labels\":{"
            "\"test/0/0\":{\"original\":\"Test\",\"text_from\":\"test/0/0\"}}}";
        const char invalid[] = "{\"schema_version\":1,\"locale\":\"en\",\"labels\":{"
            "\"test/0/0\":{\"original\":\"Test\"}}}";
        assert(tr_parse(missing, sizeof(missing) - 1, "en"));
        assert(tr_resolve(tr_find("test/0/0"), 0) == NULL);
        tr_clear();
        assert(tr_parse(cycle, sizeof(cycle) - 1, "en"));
        assert(tr_resolve(tr_find("test/0/0"), 0) == NULL);
        tr_clear();
        assert(!tr_parse(invalid, sizeof(invalid) - 1, "en"));
        tr_clear();
    }
    return 0;
}
