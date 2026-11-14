/* cc -std=gnu99 -ffunction-sections -fdata-sections
 * port/tests/menu_font/net_language_prompt.c -Wl,--gc-sections -pthread
 * -o /tmp/net-language-prompt && /tmp/net-language-prompt */
#include "../../src/gs/menu_translation.c"
#include <assert.h>

static int session, always = 1;
int Port_NetSession(void) { return session; }
int Port_Setting(const char *name, int fallback) { return always; }
void Port_SettingText(const char *name, const char *fallback, char *out, int size) {
    snprintf(out, size, "%s", "en");
}

int main(void) {
    MenuTranslationOption options[2] = {{"en", "English", 0}, {"es", "Spanish", 1}};
    char directory[] = "/tmp/net-language-XXXXXX", path[256];
    assert(mkdtemp(directory));
    snprintf(path, sizeof(path), "%s/pzs3eu1.afs", directory);
    FILE *file = fopen(path, "wb");assert(file);fclose(file);
    setenv("ZENKAIPOWER_DATA", directory, 1);
    const char *ignored[] = {"ZENKAIPOWER_MENU_LANG", "ZENKAIPOWER_NET_SESSION", "ZENKAIPOWER_PAD_PLAY",
                            "ZENKAIPOWER_PAD_TABLE", "ZENKAIPOWER_SYNCTEST"};
    for (unsigned i = 0; i < sizeof(ignored) / sizeof(*ignored); i++) unsetenv(ignored[i]);
    assert(MenuTranslation_ShouldPrompt(options, 2));
    /* The in-game lobby starts a session without the process-launch flag. */
    session = 1;
    assert(!getenv("ZENKAIPOWER_NET_SESSION"));
    assert(!MenuTranslation_ShouldPrompt(options, 2));
    /* A completed session must not suppress later offline boot prompts. */
    session = 0;
    assert(MenuTranslation_ShouldPrompt(options, 2));
    setenv("ZENKAIPOWER_NET_SESSION", "1", 1);
    assert(!MenuTranslation_ShouldPrompt(options, 2));
    unsetenv("ZENKAIPOWER_NET_SESSION");
    always = 0;
    assert(!MenuTranslation_ShouldPrompt(options, 2));
    unlink(path);rmdir(directory);
    puts("Online session language prompt checks passed");
    return 0;
}
