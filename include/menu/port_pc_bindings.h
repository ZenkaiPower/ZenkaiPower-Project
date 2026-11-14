#ifndef MENU_PORT_PC_BINDINGS_H
#define MENU_PORT_PC_BINDINGS_H
#define PORT_PC_ACTIONS 17
/* Keep companion storage compatible; the last three slots are reserved. */
#define PORT_PC_STORAGE_ACTIONS 20
#define PORT_PC_BINDINGS (PORT_PC_STORAGE_ACTIONS*2)
#define PORT_PC_CONTROLLER_PAGE 3 /* native page index; Controller state is 4 */
#define PORT_PC_OPTIONS_PAGE 27
#define PORT_PC_CONTROLLER_ENTRY 20
#define PORT_GAMEPAD_CONTROLLER_ENTRY 21
#define PORT_CLASH_CONTROLLER_ENTRY 22
#define PORT_VIRTUAL_CONTROLLER_ENTRY 23
#define PORT_GAMEPAD_BINDINGS 17
/* Up to three simultaneous gamepad inputs, sorted for stable comparisons. */
#define PORT_GAMEPAD_CHORD 0x01000000
static int Port_GamepadTokenValid(unsigned b) { return (b>=1 && b<=26) || (b>=100 && b<=111); }
static int Port_GamepadBindingValid(unsigned b) {
    unsigned a=b&255,c=(b>>8)&255,d=(b>>16)&255;
    if(!(b&PORT_GAMEPAD_CHORD)) return b==0 || Port_GamepadTokenValid(b);
    return !(b&0xFE000000u) && Port_GamepadTokenValid(a) && Port_GamepadTokenValid(c) && a<c &&
        (!d || (Port_GamepadTokenValid(d) && c<d));
}
static const char *const sPortPcLabels[PORT_PC_ACTIONS]={
    "Ki Charge","Blaster Wave","Ki Blast","Fight","Descend","Ascend","Guard","Dash",
    "Move Forward","Move Backward","Move Left","Move Right",
    "Camera Left","Camera Right","Change Camera","Switch Character","Transform/Progress"
};
#ifdef __cplusplus
extern "C" {
#endif
void PortInput_PcDefaults(void);
void PortInput_GamepadDefaults(void);
int Port_ClashSquare(void);
void Port_ClashSquareApply(int on);
int Port_VirtualGamepad(void);
void Port_VirtualGamepadApply(int on);
void PortInput_GamepadGet(int *bindings);
void PortInput_GamepadSet(const int *bindings);
void PortInput_GamepadBindingName(int action,char *out,int size);
void PortInput_GamepadCaptureStart(int action);
void PortInput_PcGet(int *bindings);
void PortInput_PcSet(const int *bindings);
void PortInput_PcBindingName(int action,char *out,int size);
void PortInput_PcCaptureStart(int action);
int PortInput_PcCapturePoll(void); /* 0 waiting, 1 accepted, -1 cancelled */
int PortInput_PcCapturing(void);
unsigned Port_PcActionMask(int action);
#ifdef __cplusplus
}
#endif
#endif
