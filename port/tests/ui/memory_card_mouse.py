#!/usr/bin/env python3
"""Check the production memory-card mouse adapter without touching save files."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[3]
text = (root / 'src/sys/dialog.c').read_text()
start = text.index('void Port_DialogMemoryMouse(void)')
function = text[start:text.index('\n}\n', start)+3]
erase_text=(root/'src/menu/menu_y_b.c').read_text()
erase_start=erase_text.index('void DcList_InputDialog(DcList *list) {')
erase_function=erase_text[erase_start:erase_text.index('\n}\n',erase_start)+3]
replay_text=(root/'src/menu/menu_za_d.c').read_text()
replay_start=replay_text.index('        ReplayMenu_Update();',replay_text.index('s32 ReplayMenu_Run('))
replay_frame=replay_text[replay_start:replay_text.index('        ColorFade_Draw();',replay_start)]
source = r'''
#include <assert.h>
#include <string.h>
typedef int s32;
#define PADG_CROSS 0x200
#define DIALOG_FLAG_CLOSING 8
#define DIALOG_FLAG_DECIDED 1
struct { int flags; } progress, *gProgress=&progress;
struct Dialog { int flags, choices, port; } dialog, *gDialog=&dialog;
struct { unsigned gamePressed; } gPad[2];
typedef struct { float x,y; int moved,confirm,cancel,wheel; } PortMenuMouse;
static PortMenuMouse input;
static int ready, opened=1, target=-1, selected=-1, reads, consume, captures=1;
void PortMenuMouse_Ready(int value) { ready=value; }
void PortMenuMouse_Read(PortMenuMouse *out) { *out=input; reads++; if(consume) memset(&input,0,sizeof(input)); }
int Dialog_IsOpen(void) { return opened; }
int Port_FlashMenuMouseHit(int scope,const char *name,float w,float h,float pivot,float x,float y) {
    assert(scope==4 && w==128 && h==64 && pivot==256);
    if(!captures) return 0;
    return target==0 ? !strcmp(name,"mc_menu_plate_1") : target==1 && !strcmp(name,"mc_menu_plate_2");
}
void Port_DialogMouseSelect(int choice) { selected=choice; }
'''
source += function
source += r'''
#define DCLIST_ST_DELETED 6
#define DCLIST_ST_MENU 1
#define MFLASH_PAD 2
#define REPLAY_BUSY_NONE 0
#define REPLAY_LEAVE 1
struct { int busy,flags,mcState; struct {int flags;} flash[1]; } replay,*gReplayMenu=&replay;
static int deleted,closed,replayAnswer;
typedef struct { int state,nextState;struct {int flash[1];} view; } DcList;
int Dialog_IsClosed(void) {return closed;}
void DcList_DeleteRec(DcList *list) {deleted++;}
void DcList_Say(DcList *list,int line) {}
void MsgWin_Open(void) {}
void Flash_GotoLabel(void *flash,char *label,int restart) {}
void Dialog_Start(int command) {}
void Dialog_SetChoices(int on) {dialog.choices=on;}
int Dialog_Input(int cancel) {return (gPad[dialog.port].gamePressed&PADG_CROSS) ? (selected==0 ? 1:-2):0;}
void ReplayMenu_Update(void) {}
void Port_UbMouseBegin(int on) {captures=0;}
void ReplayMenu_Draw(void) {
    /* McFlow's second read happens after the parent reset its hit registry. */
    Port_DialogMemoryMouse();
    replayAnswer=Dialog_Input(0);
}
'''
account_text=(root/'src/menu/port_tbo.c').read_text()
account_start=account_text.index('static s32 Tb_AccountConfirmInput(void) {')
account_prefix=account_text[account_text.index('    Dialog *previous = gDialog;',account_start):account_text.index('    if (result == DIALOG_RESULT_SECOND',account_start)]
source += '\ntypedef struct Dialog Dialog;\nstatic Dialog accountDialog,*sTbDisconnectDialog=&accountDialog;\n'
source += 'static int account_input(void) {\n'+account_prefix+'    gDialog=previous;return result;\n}\n'
source += erase_function
source += '\nstatic void replay_frame(void) {\n'+replay_frame+'\n}\n'
source += r'''
int main(void) {
    dialog.choices=1; dialog.port=1; input.moved=1; target=0;
    Port_DialogMemoryMouse(); assert(ready && selected==0 && !gPad[1].gamePressed);
    input.confirm=1; target=1;
    Port_DialogMemoryMouse(); assert(selected==1 && gPad[1].gamePressed==PADG_CROSS);
    gPad[1].gamePressed=0; target=-1;
    Port_DialogMemoryMouse(); assert(!gPad[1].gamePressed);
    input.confirm=0; input.cancel=1;
    Port_DialogMemoryMouse(); assert(!gPad[1].gamePressed);
    input.confirm=1; input.wheel=1; target=0;
    Port_DialogMemoryMouse(); assert(!gPad[1].gamePressed);
    input.wheel=0; dialog.choices=0;
    Port_DialogMemoryMouse(); assert(gPad[0].gamePressed==PADG_CROSS);
    gPad[0].gamePressed=0; opened=0;
    Port_DialogMemoryMouse(); assert(!ready && !gPad[0].gamePressed);
    opened=1; progress.flags=0x100;
    Port_DialogMemoryMouse(); assert(!ready && !gPad[0].gamePressed);
    progress.flags=0; dialog.flags=DIALOG_FLAG_CLOSING;
    Port_DialogMemoryMouse(); assert(!ready);
    dialog.flags=DIALOG_FLAG_DECIDED;
    Port_DialogMemoryMouse(); assert(ready && gPad[0].gamePressed==PADG_CROSS); /* later informational OK */
    gDialog=0; Port_DialogMemoryMouse(); assert(!ready);
    /* Replay questions must read last frame's buttons before clearing captures. */
    gDialog=&dialog;dialog.flags=0;dialog.port=0;dialog.choices=1;input.wheel=0;
    consume=1;captures=1;target=0;input.confirm=1;gReplayMenu->mcState=1;
    replay_frame();assert(replayAnswer==1 && !input.confirm);
    gPad[0].gamePressed=0;captures=1;target=1;input.confirm=1;
    replay_frame();assert(replayAnswer==-2);
    gPad[0].gamePressed=0;captures=1;target=-1;input.confirm=1;
    replay_frame();assert(replayAnswer==0);
    /* Account UI switches gDialog to its private question while reading input. */
    gPad[0].gamePressed=0;accountDialog.choices=1;accountDialog.port=0;captures=1;target=0;input.confirm=1;
    assert(account_input()==1 && gDialog==&dialog);
    gPad[0].gamePressed=0;target=1;input.confirm=1;
    assert(account_input()==-2 && gDialog==&dialog);
    gPad[0].gamePressed=0;target=-1;input.confirm=1;
    assert(account_input()==0 && gDialog==&dialog);
    /* The character deletion question has no McFlow to dispatch mouse input. */
    {
        DcList list={0};captures=1;target=1;input.confirm=1;
        DcList_InputDialog(&list);assert(!deleted && list.nextState==DCLIST_ST_MENU);
        gPad[0].gamePressed=0;dialog.choices=1;target=0;input.confirm=1;
        DcList_InputDialog(&list);assert(deleted==1 && list.nextState==DCLIST_ST_DELETED);
        gPad[0].gamePressed=0;dialog.choices=1;target=-1;input.confirm=1;
        DcList_InputDialog(&list);assert(deleted==1);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'card.c'; exe=Path(tmp)/'card'
    src.write_text(source)
    subprocess.run(['cc','-std=gnu89','-DPORT',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('Memory-card mouse checks passed')
