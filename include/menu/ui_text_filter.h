#ifndef MENU_UI_TEXT_FILTER_H
#define MENU_UI_TEXT_FILTER_H

/* Presentation only: retain original strings for translation and resource IDs.
   The USA font also encodes its TM mark as U+3327 in character names. */
static inline int Port_UiTextHiddenSymbol(unsigned code) {
    return code == 0x00AE || code == 0x2122 || code == 0x00A9 || code == 0x3327;
}

/* Remove a trailing blank or the redundant blank before a hidden mark when
   another blank follows it. Keep a separator in "word (mark)word". */
static inline int Port_UiTextSkipSpace16(const unsigned short *text) {
    int mark = 0, spaceAfter = 0;
    if (*text++ != ' ') return 0;
    while (*text == ' ' || Port_UiTextHiddenSymbol(*text)) {
        if (*text == ' ') { if (mark) spaceAfter = 1; }
        else mark = 1;
        text++;
    }
    return !*text || *text == '\n' || *text == '\r' || (mark && spaceAfter);
}

static inline int Port_UiTextHiddenUtf8Bytes(const unsigned char *text) {
    if (text[0] == 0xC2 && (text[1] == 0xAE || text[1] == 0xA9)) return 2;
    if (text[0] == 0xE2 && text[1] == 0x84 && text[2] == 0xA2) return 3;
    if (text[0] == 0xE3 && text[1] == 0x8C && text[2] == 0xA7) return 3;
    return 0;
}

static inline int Port_UiTextSkipSpace8(const unsigned char *text) {
    int mark = 0, spaceAfter = 0, bytes;
    if (*text++ != ' ') return 0;
    for (;;) {
        if (*text == ' ') { if (mark) spaceAfter = 1; text++; }
        else if ((bytes = Port_UiTextHiddenUtf8Bytes(text)) != 0) { mark = 1; text += bytes; }
        else break;
    }
    return !*text || *text == '\n' || *text == '\r' || (mark && spaceAfter);
}

#endif
