"""Check release console suppression, file writes and the bot protocol."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[3]
s=(ROOT/'port/src/plat_log.c').read_text()
a=s.index('static int mirror_console(')
b=s.index('\n#if defined(PORT_DISTRIBUTION_BUILD)',s.index('int Port_LogLine(',a))
fixture=r'''
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static int records;
static void record(const char *source,const char *text) {assert(source && text);records++;}
'''+s[a:b]+r'''
int main(void) {
    unsetenv("ZENKAIPOWER_NET_BOT");
    assert(Port_LogPrintf("test",stdout,"out %d\n",3)==6);
    assert(Port_LogPuts("test","err\n",stderr)>=0);
    assert(Port_LogLine("test","line")>=0);
    assert(records==3);
    FILE *file=tmpfile();char data[32]={0};assert(file);
    assert(Port_LogPrintf("test",file,"file %d",7)==6);
    assert(Port_LogPuts("test"," saved",file)>=0);
    rewind(file);assert(fread(data,1,12,file)==12);assert(!strcmp(data,"file 7 saved"));fclose(file);
    assert(records==3);
    setenv("ZENKAIPOWER_NET_BOT","1",1);
    Port_LogPrintf("test",stdout,"bot\n");
    Port_LogPuts("test","bot err\n",stderr);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory);(tmp/'test.c').write_text(fixture)
    for release in (0,1):
        subprocess.run(['gcc','-std=gnu99','-Wall','-Wextra','-Werror',f'-DPORT_DISTRIBUTION_BUILD={release}',str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
        result=subprocess.run([str(tmp/'test')],capture_output=True,text=True,check=True)
        assert result.stdout==('bot\n' if release else 'out 3\nline\nbot\n')
        assert result.stderr==('bot err\n' if release else 'err\nbot err\n')
print('Release console checks passed: quiet release, debug output, file writes and bot protocol.')
