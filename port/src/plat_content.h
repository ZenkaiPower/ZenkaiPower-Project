#ifndef PORT_CONTENT_H
#define PORT_CONTENT_H
#include <stdio.h>
/* Built-in assets are generated from the character asset manifests. */
int PortContent_Enabled(void);
int PortContent_Has(const char *relative_path);
FILE *PortContent_Open(const char *relative_path);
int PortContent_CostumeCount(int character, int fallback);
#endif
