/* Required heading defaults must support older catalogs without replacing edits.
 * Build with menu_translation's usual host dependencies and --gc-sections. */
#include "../../src/gs/menu_translation.c"
#include <assert.h>

int MenuText_FirstFontId(int setting) { (void)setting; return 0; }

int main(void) {
    const char *id = "battle-registration-heading/0/0";
    const char custom[] = "{\"schema_version\":1,\"locale\":\"en\",\"labels\":{"
        "\"battle-registration-heading/0/0\":{\"original\":\"Battle\\nRegistration\","
        "\"text\":\"Custom\\nHeading\"}}}";
    TrRow *row;
    assert(tr_required_defaults());
    assert(!strcmp(tr_find(id)->original, "Battle\nRegistration"));
    assert(!strcmp(tr_find(id)->text, "Battle\nRegistration"));
    assert(tr_find("fusion-ranking-heading/0/0"));
    assert(tr_find("fusion-course-heading/0/0"));
    assert(sNRows == 9);
    assert(!strcmp(tr_find("fighter-actions-normal/0/2")->text, "Erase Data"));
    assert(!strcmp(tr_find("fighter-actions-selected/0/1")->text, "Password"));
    assert(tr_required_defaults() && sNRows == 9);
    tr_clear();
    assert(tr_parse(custom, sizeof(custom) - 1, "en"));
    row = tr_find(id);
    assert(row && tr_required_defaults());
    assert(tr_find(id) == row);
    assert(!strcmp(row->text, "Custom\nHeading"));
    assert(!strcmp(tr_resolve(row, 0), "Custom\nHeading"));
    tr_clear();
    puts("Required label defaults preserve existing translations.");
    return 0;
}
