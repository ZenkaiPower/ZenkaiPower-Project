/* Standalone fresh-scan harness; driven by online_data.py. */
#include <stdio.h>
static const char *sRoot;
const char *Port_FileRoot(void) { return sRoot; }
extern int Port_BuildSignature(const char *exe, char out[65]);
int main(int argc, char **argv) {
    char signature[65];
    if (argc != 3) return 2;
    sRoot = argv[1];
    if (!Port_BuildSignature(argv[2], signature)) return 1;
    puts(signature);
    return 0;
}
