/* Menu and battle text loaded from a locale JSON in game-text. */
#ifndef ZENKAIPOWER_MENU_TRANSLATION_H
#define ZENKAIPOWER_MENU_TRANSLATION_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MenuTranslationOption {
    char locale[16];
    char name[64];
    int order; /* smaller numbers appear first; missing order sorts after numbered languages */
} MenuTranslationOption;

#define PORT_TEXT_QUIT_CONFIRM "quit/confirm"
/* The start of an online match (gs/net.c): before the other player is there, and once they are ({name}: theirs). */
#define PORT_TEXT_NET_RIVAL "net/rival_here"

/* Copies bundled label and port catalogs without replacing local edits, then loads
   ZENKAIPOWER_MENU_LANG or the saved menu_lang setting (English if neither is set). */
int MenuTranslation_Load(const char *outDir, char *why, size_t whyLen);
int MenuTranslation_Ensure(void);
int MenuTranslation_UiCatalogSource(const char *path, char *out, size_t size);
int MenuTranslation_BundledUiCatalogPath(const char *locale, char *path, size_t size);
int MenuTranslation_List(const char *outDir, MenuTranslationOption *out, int capacity);
int MenuTranslation_Available(MenuTranslationOption *out, int capacity);
int MenuTranslation_EuropeanAvailable(void);
int MenuTranslation_ShouldPrompt(const MenuTranslationOption *options, int count);
int MenuTranslation_BootPending(void);
void MenuTranslation_SaveChoice(const char *locale);
const char *MenuTranslation_Locale(void);
const char *MenuTranslation_Text(const char *id);
/* Port-owned UI text, decoded from translations/port/<locale>.json; NULL uses the built-in English string. */
const char *MenuTranslation_PortText(const char *id);
int MenuTranslation_PortTextCopy(const char *id,const char *fallback,char *out,int capacity);
const char *MenuTranslation_Original(const char *id);
const char *MenuTranslation_Group(const char *id);
const char *const *MenuTranslation_StageNames(void); /* 36 entries */
const char *MenuTranslation_MeteorSong(int slot); /* optional regional names, with English fallback */
const char *const *MenuTranslation_SongNames(void);  /* 24 entries */
const char *MenuTranslation_Main(void);              /* first 11 main menu items, separated by | */
const char *MenuTranslation_MainOriginal(void);      /* wording painted on the disc */

#ifdef __cplusplus
}
#endif
#endif
