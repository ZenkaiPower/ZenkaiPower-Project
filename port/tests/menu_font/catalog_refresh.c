#include "../../src/gs/ui_text_extract.c"
#include <assert.h>
#include <unistd.h>
static void catalog(const char *path, const char *source) {
    FILE *file = fopen(path, "wb"); assert(file);
    fputs("{\"labels\":{\"example\":{\"original\":\"", file);
    for (int i=0;i<2000;i++) fputc('x',file);
    fprintf(file,"\",\"text\":\"value\"}},\"source\":\"%s\"}",source);
    fclose(file);
}
int main(void) {
    char path[]="/tmp/zenkai-catalog-XXXXXX", source[160];
    int fd=mkstemp(path); assert(fd>=0); close(fd);
    catalog(path,"ZENKAI Power!");
    assert(MenuTranslation_UiCatalogSource(path,source,sizeof(source)) && !strcmp(source,"ZENKAI Power!"));
    assert(ut_stale(path) && ut_eu_wanted(path,"es"));
    catalog(path,UT_SOURCE); assert(!ut_stale(path));
    catalog(path,UT_EU_SOURCE); assert(!ut_eu_wanted(path,"es"));
    catalog(path,"European disc, version 1"); assert(ut_eu_wanted(path,"es"));
    catalog(path,"My custom translation"); assert(!ut_stale(path) && !ut_eu_wanted(path,"es"));
    unlink(path); puts("PASS: source metadata at end, bundled refresh, current and custom catalog preservation");
}
