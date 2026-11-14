"""Compile and exercise production client reporting and ranked preview functions."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
source = (root / 'port/src/gs/net_match.c').read_text()


def function(name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    return source[start:source.index('\n}', start) + 2]


code = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int sStatsLock=1, sBotBattleLock, sBotBattle;
static struct {
    int host,active,ranked,scoringReady,startPoints[2],tolerance;
    char uid[40],key[40];
} sStats;
static int submissions,connections;
static char payload[512];
#define SDL_snprintf snprintf
static void SDL_LockSpinlock(void *p) {(void)p;}
static void SDL_UnlockSpinlock(void *p) {(void)p;}
static void SDL_LockMutex(int p) {(void)p;}
static void SDL_UnlockMutex(int p) {(void)p;}
static void BotBattle_End(void *p,FILE *f,int w,int e,int d) {(void)p;(void)f;(void)w;(void)e;(void)d;}
static void stats_enqueue(const char *action,const char *body) {
    assert(!strcmp(action,"battle/end"));
    submissions++; snprintf(payload,sizeof(payload),"%s",body);
}
static void stats_connection(void) {connections++;}
''' + function('Port_NetStatsEnd') + '\n' + function('Port_NetStatsAdjustPoints') + r'''
int main(void) {
    strcpy(sStats.uid,"battle"); strcpy(sStats.key,"own-key");
    for(int host=0;host<2;host++) {
        sStats.active=1; sStats.ranked=1; sStats.host=host;
        int before=submissions;
        Port_NetStatsEnd(0,1,45123,2147483647,-2147483647);
        assert(submissions==before+1 && !sStats.active);
        assert(strstr(payload,"\"winner\":\"p1\"") && strstr(payload,"\"finish_type\":\"perfect\""));
        assert(strstr(payload,"\"key\":\"own-key\"") && strstr(payload,"45.123"));
        Port_NetStatsEnd(0,1,45123,0,0);
        assert(submissions==before+1);
    }
    sStats.active=1;sStats.ranked=0;sStats.host=0;
    int before=submissions;
    Port_NetStatsEnd(1,0,1000,123,456);
    assert(submissions==before);
    sStats.active=1;sStats.host=1;
    Port_NetStatsEnd(1,0,1000,123,456);
    assert(submissions==before+1);
    assert(strstr(payload,"\"p1_points_won\":0,\"p2_points_won\":0"));
    sStats.scoringReady=1;sStats.startPoints[0]=3000;sStats.startPoints[1]=2000;sStats.tolerance=250;
    int delta[2]={210,-77};
    assert(Port_NetStatsAdjustPoints(0,delta));
    assert(delta[0]==120 && delta[1]==-44);
    Port_NetStatsAdjustPoints(-1,delta);
    assert(delta[0]==0 && delta[1]==0);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ranked-client-') as temporary:
    path = Path(temporary) / 'client.c'
    binary = Path(temporary) / 'client'
    path.write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu99', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print('Client reports: both ranked peers, own keys, no duplicate end, host-only unranked, performance-based scaled preview passed')
