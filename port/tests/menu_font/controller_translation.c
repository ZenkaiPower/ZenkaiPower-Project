/* Per-player Controller labels must load through the actual catalog parser. */
#include "../../src/gs/menu_translation.c"
#include <assert.h>
int MenuText_FirstFontId(int setting) { (void)setting;return 0; }
int Port_ZenkaiEnabled(void) {return 0;}
int Port_ZenkaiSelected(void) {return 0;}
int Port_NetSession(void) {return 0;}
int main(void) {
    TrPortCatalog catalog;
    int i;
    {
        static const char *locales[]={"en","de","es","fr","it"};
        static const char *titles[][2]={{"1P Controller","2P Controller"},{"S1-Controller","S2-Controller"},
            {"Mando J1","Mando J2"},{"Manette J1","Manette J2"},{"Controller 1P","Controller 2P"}};
        int lang,player;
        for(lang=0;lang<5;lang++) {
            assert(tr_port_read("port",locales[lang],&catalog));
            for(i=0;i<TR_PORT_TEXT_COUNT;i++) {
                for(player=0;player<2;player++) {
                    const char *id=player ? "controls/player2/title":"controls/player1/title";
                    if(!strcmp(kPortText[i].id,id))
                        assert(catalog.present[i] && !strcmp(catalog.text[i],titles[lang][player]));
                }
                if(!strcmp(kPortText[i].id,"controls/player1/help") || !strcmp(kPortText[i].id,"controls/player2/help") || !strncmp(kPortText[i].id,"controls/clash-mode/",20) ||
                   !strcmp(kPortText[i].id,"controls/help/gamepad/clash-mode"))
                    assert(catalog.present[i] && catalog.text[i][0]);
            }
        }
    }
    puts("Controller translations passed for English, German, Spanish, French and Italian");
    return 0;
}
