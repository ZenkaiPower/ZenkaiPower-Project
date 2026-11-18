"""Run original-game fix regressions without disc assets or an Android build."""
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def function(path, signature):
    source = (ROOT / path).read_text(encoding='utf-8')
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


def gameplay_fixture():
    fixture = r'''
#include "common.h"
#include "sys/port_original_fixes.h"
#include "battle/battle_setup.h"
#include "battle/btl_act_j.h"
#include "battle/eft_n.h"
#include "sys/adx.h"
#include <assert.h>
extern void *memset(void *, int, u32);
static int zenkai, mode, alive, forbidden, aiCharacter, ready, newModel;
static int bolts[2], enabledCalls, sourceFlags[2], panCalls, normalPan, pans[2];
static EftAuraMgr aura;
#define gPool (&aura)
int Port_ZenkaiEnabled(void) { return zenkai; }
int Port_ZenkaiPackEnabled(void) { assert(0); return 0; }
s32 Battle_GetMode(void) { return mode; }
s32 BtlChar_TestFlag(BtlActJChr *c, s32 flag) { return forbidden; }
s32 BtlMember_CountAlive(BtlActJChr *c) { return alive; }
void BtlMember_ApplyItems(BattleMember *m) { aiCharacter = m->chara; }
s32 BtlStage_IsReady(void) { return ready; }
s32 BtlCharApi_IsModelNew(s32 id) { return newModel; }
s32 BtlCharApi_ObjGetParamFlags0(s32 id) { return sourceFlags[id]; }
void EftBolt_Enable(s32 id) { if (!bolts[id]) { bolts[id] = 1; enabledCalls++; } }
void EftBolt_Disable(s32 id) { bolts[id] = 0; }
AdxPlayer gAdxPlayerTbl[6];
s32 BtlScript_IsSlotBusy(s32 a, s32 b) { return 0; }
void ADXT_SetOutPan(ADXT_HN p, s32 ch, s32 pan) { panCalls++; pans[ch] = pan; }
void Adx_SetPan(s32 ch, s32 a, s32 b) { normalPan++; }
void Adx_SetVolume(s32 ch, s32 volume) {}
void ADXT_Pause(ADXT_HN p, s32 pause) {}
void File_StartAdxById(ADXT_HN p, s32 id) {}
'''
    for path, signature in [
        ('src/battle/battle_load.c', 'void BtlMember_Init('),
        ('src/battle/btl_act_j.c', 's32 BtlAct_CanSwitch('),
        ('src/battle/eft_n.c', 'void EftAura_UpdateLightning('),
        ('src/battle/eft_n.c', 'void EftAuraMgr_Update('),
        ('src/sys/adx.c', 'void Adx_Play('),
    ]:
        fixture += function(path, signature)
    source = (ROOT / 'src/menu/menu_m_b.c').read_text(encoding='utf-8')
    start = source.index('gUbTeamSel->sel->cur--;', source.index('case UBTEAM_STEP_TEAM:'))
    end = source.index('UbTeamSel_ClipGoto(1, UBTEAM_CLIP_TEAM, "fl_on_start");', start)
    fixture += r'''
#define UB_CUR_MENU 5
static struct { int cur, memberCount; } selection;
static struct { typeof(selection) *sel; int teamMax; } team = {&selection, 4};
#define gUbTeamSel (&team)
static void backward(void) {
''' + source[start:end] + '\n}\n'
    fixture += r'''
int main(void) {
    for (zenkai = 0; zenkai < 2; zenkai++) {
        int fixed = zenkai && PORT_ZENKAI_ORIGINAL_FIXES;
        BattleMember member;
        BtlActJChr chr = {0};
        int limit;
        BtlMember_Init(&member, 80, 2, 1, 20, 1.0f, NULL);
        assert(aiCharacter == (fixed ? 80 : 0) && member.chara == 80);
        assert(member.costume == 2 && member.variant == 1 && member.cpuLevel == 20);
        mode = 0; alive = 2; forbidden = 0; chr.switchGauge = 100000;
        assert(BtlAct_CanSwitch(&chr, 1, 1) == fixed);
        chr.switchGauge = 99999;
        assert(!BtlAct_CanSwitch(&chr, 1, 1));
        chr.switchGauge = 100000; alive = 1;
        assert(!BtlAct_CanSwitch(&chr, 1, 1));
        alive = 2; forbidden = 1;
        assert(!BtlAct_CanSwitch(&chr, 1, 1));
        forbidden = 0; mode = 1;
        assert(!BtlAct_CanSwitch(&chr, 1, 1)); /* Story locks retained. */
        mode = 6;
        assert(!BtlAct_CanSwitch(&chr, 1, 1)); /* Training locks retained. */
        for (limit = 1; limit <= 5; limit++) {
            team.teamMax = limit; selection.memberCount = limit;
            selection.cur = UB_CUR_MENU;
            backward();
            assert(selection.cur == (fixed || limit != 4 ? limit - 1 : 4));
        }
        memset(&aura, 0, sizeof(aura));
        aura.count = 2; ready = 1; newModel = 0;
        sourceFlags[0] = 4; sourceFlags[1] = 0;
        bolts[0] = bolts[1] = enabledCalls = 0;
        EftAuraMgr_Update();
        assert(bolts[0] && !bolts[1] && enabledCalls == 1);
        bolts[0] = 0; /* Rematch destroys effect tasks without changing the model. */
        EftAuraMgr_Update();
        assert(bolts[0] == fixed && enabledCalls == 1 + fixed);
        EftAuraMgr_Update();
        assert(enabledCalls == 1 + fixed); /* No duplicate task each frame. */
        panCalls = normalPan = 0;
        Adx_Play(ADX_CH_BGM, 1, 64, 0);
        assert(panCalls == (fixed ? 2 : 0) && normalPan == !fixed);
        if (fixed) assert(pans[0] == -128 && pans[1] == -128);
        panCalls = normalPan = 0;
        Adx_Play(ADX_CH_BGM, 1, 64, 20);
        Adx_Play(ADX_CH_SE0, 1, 64, 0);
        assert(panCalls == 0 && normalPan == 2);
    }
    return 0;
}
'''
    return fixture


def text_fixture():
    import json
    catalog = json.loads((ROOT / 'port/translations/ui/en.json').read_text(encoding='utf-8'))
    fixes = {key: row for key, row in catalog['labels'].items() if row.get('zenkai_fix')}
    assert len(fixes) >= 16
    payload = json.dumps(dict(schema_version=1, locale='en', labels=fixes), ensure_ascii=True)
    return r'''
#undef malloc
#define UI_TEXT_HOST_ALLOC
#include "port/src/gs/ui_text_translate.c"
#include <assert.h>
static int enabled;
int Port_ZenkaiEnabled(void) { return enabled; }
static const char catalog[] =
''' + json.dumps(payload) + r''';
int main(void) {
    int i;
    sRows = calloc(UI_MAX_ROWS, sizeof(*sRows));
    sGroups = calloc(UI_MAX_GROUPS, sizeof(*sGroups));
    assert(ui_parse(catalog, sizeof(catalog) - 1, "en"));
    assert(ui_prepare());
    sReady = 1;
    for (i = 0; i < sRowCount; i++) {
        UiRow *row = sRows[i];
        const uint16_t *fixed;
        char group[1200], output[65536];
        char *slash;
        int index;
        assert(row->corrected && !row->translated);
        enabled = 0;
        assert(UiText_TranslateString(row->original) == row->original);
        enabled = 1;
        fixed = UiText_TranslateString(row->original);
        assert(ui_equal(fixed, PORT_ZENKAI_ORIGINAL_FIXES ? row->corrected : row->original));
        snprintf(group, sizeof(group), "%s", row->id);
        slash = strrchr(group, '/'); index = atoi(slash + 1); *slash = 0;
        assert(UiText_TranslateIdUtf8(group, index, output, sizeof(output)) == PORT_ZENKAI_ORIGINAL_FIXES);
        enabled = 0;
        assert(!UiText_TranslateIdUtf8(group, index, output, sizeof(output)));
    }
    ui_clear();
    sRows = calloc(UI_MAX_ROWS, sizeof(*sRows));
    sGroups = calloc(UI_MAX_GROUPS, sizeof(*sGroups));
    assert(ui_parse(catalog, sizeof(catalog) - 1, "en"));
    /* A subsequent generated skill layer must preserve the keyed correction. */
    {
        UiRow *row = sRows[0];
        char *id = strdup(row->id);
        size_t length = 0;
        uint16_t *same;
        uint16_t *a = ui_utf16("different source", 0);
        while (row->original[length]) length++;
        same = malloc((length + 1) * sizeof(*same));
        memcpy(same, row->original, (length + 1) * sizeof(*same));
        assert(ui_add_row(strdup(row->id), same, NULL, 0));
        assert(row->corrected);
        assert(ui_add_row(id, a, NULL, 0));
        assert(!row->corrected); /* Modified data must not receive a stale fix. */
    }
    enabled = 1;
    sReady = 0;
    { uint16_t unchanged[] = {0xfeff, 'S', 'p', 'i', 'r', 'i', 't', ' ',
                             'B', 'a', 'l', 'l', 0};
      assert(UiText_TranslateString(unchanged) == unchanged); }
    ui_clear();
    return 0;
}
'''


def rng_hash(fixed):
    state = [5489]
    for i in range(1, 624):
        state.append((1812433253 * (state[-1] ^ (state[-1] >> 30)) + i) & 0xffffffff)
    if fixed:
        # Python's independently maintained MT19937 implementation, using the
        # reference init_genrand state rather than Python's own seed conversion.
        reference = random.Random()
        reference.setstate((3, tuple(state + [624]), None))
        values = [reference.getrandbits(32) for _ in range(4000)]
    else:
        values = []
        index = 624
        for _ in range(4000):
            if index == 624:
                for k in range(227):
                    y = (state[k] & 0x80000000) | (state[k + 1] & 0x7fffffff)
                    state[k] = state[k + 397] ^ (y >> 1) ^ (0x9908b0df if y & 1 else 0)
                y = (state[623] & 0x80000000) | (state[0] & 0x7fffffff)
                state[623] = state[396] ^ (y >> 1) ^ (0x9908b0df if y & 1 else 0)
                index = 0
            y = state[index]
            index += 1
            y ^= y >> 11
            y ^= (y << 7) & 0x9d2c5680
            y ^= (y << 15) & 0xefc60000
            values.append(y ^ (y >> 18))
    result = 1469598103934665603
    for value in values:
        result = ((result ^ value) * 1099511628211) & ((1 << 64) - 1)
    return result


def main():
    sources = ['src/sys/rand.c', 'src/sys/file.c', 'src/sys/list.c',
               'src/sys/snd.c', 'src/battle/btl_pool.c',
               'port/tests/battle/original_fixes.c']
    with tempfile.TemporaryDirectory(prefix='original-fixes-') as temporary:
        for flag in (0, 1):
            binary = str(Path(temporary) / f'fixes-{flag}')
            subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin',
                            '-ffunction-sections', '-fdata-sections',
                            '-fsanitize=undefined', '-Iinclude', '-Iport/include',
                            '-include', 'port_compat.h', '-Wl,--gc-sections',
                            f'-DPORT_ZENKAI_ORIGINAL_FIXES={flag}',
                            f'-DEXPECT_FIXED={rng_hash(True)}ULL',
                            f'-DEXPECT_ORIGINAL={rng_hash(False)}ULL',
                            *sources, '-o', binary], cwd=ROOT, check=True)
            subprocess.run([binary], check=True)
            for name, fixture in [('gameplay', gameplay_fixture()), ('text', text_fixture())]:
                source = Path(temporary) / f'{name}.c'
                source.write_text(fixture, encoding='utf-8')
                binary = str(Path(temporary) / f'{name}-{flag}')
                # Text is ordinary host code: no game include paths or PORT
                # compatibility header. Match the renderer compilation boundary.
                includes = (['-I.'] if name == 'text' else
                            ['-I.', '-Iinclude', '-Iport/include', '-include', 'port_compat.h'])
                subprocess.run(['gcc', '-w', '-std=gnu99', '-fno-builtin',
                                '-ffunction-sections', '-fdata-sections',
                                '-fsanitize=address,undefined', *includes, '-Wl,--gc-sections',
                                f'-DPORT_ZENKAI_ORIGINAL_FIXES={flag}', str(source),
                                '-o', binary], cwd=ROOT, check=True)
                subprocess.run([binary], check=True)
    print('Original-game fixes: enabled/disabled and Original/Zenkai checks passed')


if __name__ == '__main__':
    main()
