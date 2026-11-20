"""Prepared installations discover newly bundled label-only languages."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'port/tools'))
from data_package import pack_directory

SOURCE = r'''
#include "port/src/gs/menu_translation.c"
#include <assert.h>
void UiText_ExtractOnStartup(const char *outDir) { (void)outDir; }
int main(void) {
    MenuTranslationOption options[32];
    int count=MenuTranslation_List("game-text",options,32), portuguese=0;
    for(int i=0;i<count;i++) if(!strcmp(options[i].locale,"pt")) {
        portuguese++;
        assert(!strcmp(options[i].name,"Portuguese test"));
    }
    assert(portuguese==1);
    assert(access("game-text/translations/labels/pt.json",F_OK)!=0);
    assert(count==2);
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix='bundled-label-discovery-') as tmp:
    root = Path(tmp)
    source, executable = root / 'test.c', root / 'test'
    source.write_text(SOURCE)
    subprocess.run(['cc', '-std=gnu99', '-ffunction-sections', '-fdata-sections',
                    '-fsanitize=address,undefined', '-I' + str(ROOT), str(source),
                    '-Wl,--gc-sections', '-pthread', '-o', str(executable)], check=True)
    for packaged in (False, True):
        fixture = root / ('packaged' if packaged else 'loose')
        labels = fixture / 'translations/labels'
        local = fixture / 'game-text/translations/labels'
        labels.mkdir(parents=True)
        local.mkdir(parents=True)
        (fixture / 'game-text/.prepared').write_text('1')
        for locale, name in [('en', 'English'), ('pt', 'Portuguese test')]:
            catalog = json.dumps(dict(schema_version=1, locale=locale, name=name,
                                      labels={'test/0/0': dict(original='Test', text='Test')}))
            (labels / f'{locale}.json').write_text(catalog)
            if locale == 'en':
                (local / 'en.json').write_text(catalog)
        if packaged:
            pack_directory(fixture / 'translations')
        subprocess.run([str(executable)], cwd=fixture, check=True)
print('PASS: prepared installs discover loose and packaged label-only locales without copying or duplicates')
