#!/usr/bin/env python3
"""Native training pause entries, On/Off callbacks and Original isolation."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/battle/hud_0.c').read_text()
a=s.index('#include "battle/port_training_options.h"')
b=s.index('\n#endif',a)
source=r'''
#include <assert.h>
#include <stddef.h>
#include "battle/hud_0.h"
#define PORT 1
static int zenkai=1;
int Port_ZenkaiEnabled(void) {return zenkai;}
BtlMenu gPauseMenuMode5,gPauseMenuMode6,D_002C4FD0;
s32 PauseMenu_CpuLevelFunc(BtlMenuEvent *ev) { return 0; }
'''+s[a:b]+r'''
int main(void) {
 BtlMenuItem items[2]={{0},{0}};items[0].id=1;items[0].sub=&D_002C4FD0;items[1].id=2;
 gPauseMenuMode5.items=gPauseMenuMode6.items=items;
 gPauseMenuMode5.count=gPauseMenuMode6.count=2;
 gPauseMenuMode5.scale=gPauseMenuMode6.scale=1;
 PauseMenu_PortTrainingInit();assert(gPortTrainingHealthRegeneration && gPortTrainingKiRegeneration && gPortTrainingBlastRegeneration && sPortTrainingMenusReady);
 for(int i=0;i<2;i++) {
  assert(sPortTrainingMenu[i].count==5 && sPortTrainingMenu[i].items[0].id==1);
  assert(sPortTrainingMenu[i].items[4].id==2);
  BtlMenuItem *entry=&sPortTrainingMenu[i].items[1];
  assert(entry->text[0]==PORT_TRAINING_REGEN_TEXT && entry->text[1]==entry->text[0]);
  assert(entry->sub->count==2 && entry->sub->scale==1);
  BtlMenuEvent event={0};event.selected=1;event.menu=entry->sub;
  event.arg=0;entry->sub->items[1].func(&event);
  assert(!gPortTrainingHealthRegeneration && event.menu->state==BTL_MENU_STATE_CLOSE);
  entry->func(&event);assert(entry->sub->cursor==1);
  event.arg=1;entry->sub->items[0].func(&event);assert(gPortTrainingHealthRegeneration);
  event.arg=0;entry->func(&event);assert(entry->sub->cursor==0);
  BtlMenuItem *ki=&sPortTrainingMenu[i].items[2];
  assert(ki->text[0]==PORT_TRAINING_REGEN_TEXT+3 && ki->sub!=entry->sub);
  event.menu=ki->sub;event.arg=2;ki->sub->items[1].func(&event);
  assert(!gPortTrainingKiRegeneration && gPortTrainingHealthRegeneration);
  event.arg=1;ki->func(&event);assert(ki->sub->cursor==1);
  event.arg=3;ki->sub->items[0].func(&event);assert(gPortTrainingKiRegeneration);
  BtlMenuItem *blast=&sPortTrainingMenu[i].items[3];
  assert(blast->text[0]==PORT_TRAINING_REGEN_TEXT+4 && blast->sub!=ki->sub);
  event.menu=blast->sub;event.arg=blast->sub->items[1].arg;
  blast->sub->items[1].func(&event);
  assert(!gPortTrainingBlastRegeneration && gPortTrainingKiRegeneration && gPortTrainingHealthRegeneration);
  event.arg=blast->arg;blast->func(&event);assert(blast->sub->cursor==1);
  event.arg=blast->sub->items[0].arg;blast->sub->items[0].func(&event);
  assert(gPortTrainingBlastRegeneration);
  event.arg=blast->arg;blast->func(&event);assert(blast->sub->cursor==0);
 }
 assert(Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT)[0]=='H');
 assert(Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT+1)[1]=='n');
 assert(Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT+2)[1]=='f');
 assert(Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT+4)[0]=='B');
 assert(!Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT+5));
 assert(!Port_TrainingMenuText(12));
 assert(gPauseMenuMode5.count==2 && gPauseMenuMode6.count==2);
 zenkai=0;PauseMenu_PortTrainingInit();assert(!sPortTrainingMenusReady);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'training.c';e=Path(tmp)/'training';p.write_text(source)
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include'),str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('Native training regeneration menu checks passed')
