/* The list of menu labels of the "Menu font" setting, made from the player's own disc data (menu_labels.c). */
#ifndef GS_MENU_LABELS_H
#define GS_MENU_LABELS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Header of the current labels.tsv layout. A list without this version is made again. */
#define MENU_LABELS_VERSION "ZENKAI Power! menu text v20:"

enum { MENU_LABELS_MADE = 0, MENU_LABELS_OTHER_DISC = 1, MENU_LABELS_FAILED = -1 };

/* Reads the HUD and menu archives in `dataDir` (pzs3us1/00005.bin, 00448.bin ...) and writes labels.tsv and name-styles.tsv (and
   labels.json, the form to edit, unless one is there) into `outDir`, each whole or not at all. `anyDisc`: the
   archives are not checked to be the USA release's (testing; a changed archive). Returns one of the values above;
   `why` gets one line: how many labels, or what kept the list from being made. Takes a few tenths of a second, reads
   nothing of the game's memory, and may run in a thread of its own; not twice at a time. */
int MenuLabels_Make(const char *dataDir, const char *outDir, int anyDisc, char *why, size_t whyLen);

/* Native memory-card/quit dialog frame atlas, decoded from USA archive 473, boot section 2,
   Dialog section 1, texture part 4. Host RGBA (alpha 255 opaque); caller frees the result.
   Reads disc resources only, so it is safe during an online match. */
unsigned char *MenuLabels_DialogFrame(const char *dataDir, int *width, int *height);

/* Blue or highlighted choice plate from the same Dialog resource. Host RGBA; caller frees. */
unsigned char *MenuLabels_DialogButton(const char *dataDir, int highlighted, int *width, int *height);

/* USA title-screen logo, host RGBA; caller frees. */
unsigned char *MenuLabels_TitleLogo(const char *dataDir, int *width, int *height);
/* Optional Japanese title logo; NULL when unavailable. */
unsigned char *MenuLabels_MeteorLogo(const char *dataDir, int *width, int *height);

/* Main menu Options animation: five horizontal 48px cells, 100ms per cell. Host RGBA, caller frees. */
/* Original reserve frame atlas with the inset gauge rails cleared from its interior. GS alpha. */
unsigned char *MenuLabels_HudNameFrame(const char *dataDir, int *width, int *height);
unsigned char *MenuLabels_SaveIcon(const char *dataDir, int *width, int *height);

/* The European disc's menu art in another language: `bank` 1 Spanish, 2 German, 3 French, 4 Italian. The setup
   (or port/tools/extract_eu.py) has put the European disc's menu and HUD archives into `dataDir`/pzs3eu1; every
   texture file of them that the language paints otherwise is paired with the USA one in `dataDir`/pzs3us1 by the
   hash of the decoded USA pixels, the one the renderer sees. Returns how many textures, or -1 (`why` says why);
   once, in a thread of its own (a second or two). Then MenuLabels_ArtSwap puts the European picture into a decoded
   texture that is one of them (1), and MenuLabels_ArtKey is no longer 0 (the renderer's cache key: textures decoded
   before are decoded again). */
int MenuLabels_Art(const char *dataDir, int bank, char *why, size_t whyLen);
int MenuLabels_ArtSwap(unsigned char *rgba, int w, int h);
int MenuLabels_SoundtrackArtInfo(const unsigned char *rgba, int w, int h, int *first, int *selected);
int MenuLabels_SoundtrackArtSwap(unsigned char *rgba, int w, int h);
unsigned MenuLabels_ArtKey(void);

/* Native startup language selector pictures from the player's European disc (PZS3EU0.AFS, entry 1).
   `id` is TBO_LANG_BG/ARROW_LEFT/ARROW_RIGHT. On success `pixels` is malloc-owned RGBA in GS alpha
   (0x80 is opaque); the caller frees it. No PNG assets are involved. */
int MenuLabels_LanguageArtAvailable(void);
int MenuLabels_LanguageArtPixels(int id, unsigned char **pixels, int *w, int *h);

#ifdef __cplusplus
}
#endif
#endif
