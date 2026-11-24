"""Exercise the actual completion state in both ordinary and new-save flows."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[3]
source=(root/'src/sys/mcflow_a.c').read_text()
helper=re.search(r'^static void McFlow_FinishSave\(void\) \{.*?^\}',source,re.M|re.S).group()
prefix=r'''#include <assert.h>
typedef int s32;
static struct {int state,timer,req;} flow,*gMcFlow=&flow;
static struct {int result;} card;
static int sMcCommitPending,commitResult,mainWrites;
static void *gSaveData;
#define MCF_CARD card
#define MCF_GO(st) do {gMcFlow->state=(st);gMcFlow->timer=120;} while(0)
#define MCF_POLL() do {} while(0)
int Port_OptionsSaveCommitAsync(void) {return commitResult;}
int McCard_IsFatalError(int result) {return result<0;}
void Dialog_SetChoices(int choices) {}
int McCard_WriteSave(int port,void *data,int bytes,int *request) {mainWrites++;return 1;}
'''
# msg is also used by the real state.
prefix=prefix.replace('state,timer,req','state,timer,req,msg')
with tempfile.TemporaryDirectory() as tmp:
    for name in ['McFlow_UpdateSave','McFlow_UpdateNewSave']:
        function=source[source.index('void '+name+'(void) {'):]
        begin=function.index('    case 0xF:')
        end=function.index('    case 6:',begin)
        state=function[begin:end]
        test=r'''
static void update(void) {switch(gMcFlow->state) {
'''+state+r'''
}}
int main(void) {
    flow.state=0xF;flow.timer=0;commitResult=-1;
    update();assert(mainWrites==1 && sMcCommitPending && flow.state==0xF);
    for(int frame=0;frame<120;frame++) {update();assert(mainWrites==1 && sMcCommitPending && flow.state==0xF);}
    commitResult=1;update();assert(mainWrites==1 && !sMcCommitPending && flow.state==0x10);
    flow.state=0xF;flow.timer=0;commitResult=-1;update();assert(mainWrites==2);
    commitResult=0;update();assert(mainWrites==2 && !sMcCommitPending && flow.state==0x11);
    return 0;
}
'''
        c=Path(tmp)/'flow.c';exe=Path(tmp)/'flow';c.write_text(prefix+helper+test)
        subprocess.run(['cc','-std=c99','-DPORT=1',str(c),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
print('Async save flows: pending frames preserve animation, never repeat main writes, and propagate companion errors')
