#ifndef BATTLE_PORT_CHEATS_H
#define BATTLE_PORT_CHEATS_H
#ifdef PORT
/* Only attached to landed melee hits, never projectiles, chip damage or recoil. */
#define PORT_DMG_ONE_PUNCH 0x40000000
extern int Port_OnePunchForSide(int side);
#endif
#endif
