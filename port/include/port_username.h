#ifndef PORT_USERNAME_H
#define PORT_USERNAME_H
/* Shared pre-confirmation validation for the name editor and account client. */
static const char *Port_UsernameError(const char *name) {
    int length = 0, letters = 0, i;
    if (name) while (name[length]) length++;
    if (length < 3 || length > 16) return "account/invalid-name";
    if (!((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= 'a' && name[0] <= 'z')) ||
        name[length - 1] == '-' || name[length - 1] == '_') return "account/invalid-name";
    for (i = 0; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) letters++;
        else if (!((c >= '0' && c <= '9') || c == '_' || c == '-')) return "account/invalid-name";
    }
    if (length == 6) {
        static const char reserved[] = "player";
        for (i = 0; i < length; i++) {
            unsigned char c = (unsigned char)name[i];
            if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            if (c != reserved[i]) break;
        }
        if (i == length) return "account/invalid-name";
    }
    return letters >= 3 ? 0 : "account/invalid-name";
}
#endif
