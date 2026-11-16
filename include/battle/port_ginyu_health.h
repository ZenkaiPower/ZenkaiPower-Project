#ifndef BATTLE_PORT_GINYU_HEALTH_H
#define BATTLE_PORT_GINYU_HEALTH_H

/* Battle-only state, isolated for each side and team member. */
typedef struct PortGinyuBodyHealth {
    int hp[161];
    unsigned char known[161];
} PortGinyuBodyHealth;
static PortGinyuBodyHealth sPortGinyuHealth[2][5];
static void Port_GinyuHealthReset(int side) {
    int member, body;
    for (member=0;member<5;member++) for (body=0;body<161;body++)
        sPortGinyuHealth[side][member].known[body]=0;
}
static int Port_GinyuHealthChange(int side,int member,int from,int to,int hp,int maximum) {
    PortGinyuBodyHealth *state;
    if (side<0 || side>=2 || member<0 || member>=5 || from<0 || from>=161 || to<0 || to>=161)
        return maximum;
    state=&sPortGinyuHealth[side][member];
    state->hp[from]=hp;
    state->known[from]=1;
    if (!state->known[to]) return maximum;
    hp=state->hp[to];
    return hp>maximum ? maximum : hp;
}
#endif
