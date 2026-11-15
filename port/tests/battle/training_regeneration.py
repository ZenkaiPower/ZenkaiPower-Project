#!/usr/bin/env python3
"""Native training pause entries, On/Off callbacks and Original isolation."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
s=(root/'src/battle/hud_0.c').read_text()
a=s.index('#include "battle/port_training_options.h"')
b=s.index('\n#endif\n\n/* Returns the menu tree',a)
source=r'''
#include <assert.h>
#include <stddef.h>
#include "battle/hud_0.h"
#define PORT 1
static int zenkai=1;
int Port_ZenkaiEnabled(void) {return zenkai;}
BtlMenu gPauseMenuMode5,gPauseMenuMode6,D_002C4FD0;
s32 PauseMenu_CpuLevelFunc(BtlMenuEvent *ev) { return 0; }
void Port_TrainingTextCopy(int index,char *out,int capacity) {
 const char *labels[]={"Health Regeneration","On","Off","Ki Regeneration","Blast Stock Regeneration","Training Settings","Switch Gauge Regeneration"};
 int i=0;while(labels[index][i] && i<capacity-1) {out[i]=labels[index][i];i++;}out[i]=0;
}
'''+s[a:b]+r'''
int main(void) {
 BtlMenuItem items[2]={{0},{0}};items[0].id=1;items[0].sub=&D_002C4FD0;items[1].id=2;
 gPauseMenuMode5.items=gPauseMenuMode6.items=items;
 gPauseMenuMode5.count=gPauseMenuMode6.count=2;
 gPauseMenuMode5.scale=gPauseMenuMode6.scale=1;
 PauseMenu_PortTrainingInit();assert(sPortTrainingMenusReady);
 int *settings[]={&gPortTrainingHealthRegeneration,&gPortTrainingKiRegeneration,&gPortTrainingBlastRegeneration,&gPortTrainingSwitchRegeneration};
 int rows[]={0,3,4,6};
 for(int side=0;side<2;side++) {
  assert(sPortTrainingMenu[side].count==3);
  assert(sPortTrainingMenu[side].items[0].id==1 && sPortTrainingMenu[side].items[2].id==2);
  BtlMenuItem *entry=&sPortTrainingMenu[side].items[1];
  assert(entry->text[0]==PORT_TRAINING_REGEN_TEXT+5);
  assert(entry->sub==&sPortTrainingSettingsMenu && entry->sub->count==5);
  for(int kind=0;kind<4;kind++) {
   BtlMenuItem *setting=&entry->sub->items[kind+1];
   assert(setting->text[0]==PORT_TRAINING_REGEN_TEXT+rows[kind]);
   assert(setting->sub->count==3 && *settings[kind]==1);
   assert(setting->sub->items[0].disabled);
   BtlMenuEvent event={0};event.selected=1;event.arg=setting->arg;
   setting->func(&event);assert(setting->sub->cursor==1);
   event.menu=setting->sub;event.arg=setting->sub->items[2].arg;
   setting->sub->items[2].func(&event);
   assert(*settings[kind]==0 && event.menu->state==BTL_MENU_STATE_CLOSE);
   for(int other=0;other<4;other++) if(other!=kind) assert(*settings[other]==1);
   event.arg=setting->arg;setting->func(&event);assert(setting->sub->cursor==2);
   event.arg=setting->sub->items[1].arg;setting->sub->items[1].func(&event);assert(*settings[kind]==1);
  }
 }
 assert(Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT+6)[0]=='S');
 assert(!Port_TrainingMenuText(PORT_TRAINING_REGEN_TEXT+7));
 gPortTrainingSwitchRegeneration=0;PauseMenu_PortTrainingInit();assert(gPortTrainingSwitchRegeneration==1);
 assert(gPauseMenuMode5.count==2 && gPauseMenuMode6.count==2);
 zenkai=0;PauseMenu_PortTrainingInit();assert(!sPortTrainingMenusReady);
}

'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'training.c';e=Path(tmp)/'training';p.write_text(source)
 subprocess.run(['cc','-std=gnu99','-fsanitize=address,undefined','-I'+str(root/'include'),str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
print('Native training regeneration menu checks passed')
