"""Check save metadata validation, capture and persistence without real accounts or D1."""
from pathlib import Path
import json
import subprocess
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'server/src'))
from lan_server import Matchmaking, ApiError, digest

summary = {f: 0 for f in ('rtt_samples', 'duration_ms', 'input_packets_sent', 'input_packets_received',
           'input_bytes_sent', 'input_bytes_received', 'wait_count', 'rollbacks', 'rollback_frames',
           'input_delay_frames', 'rollback_window_frames', 'protocol_version')}
summary.update({f: None for f in ('avg_ping_ms', 'min_ping_ms', 'max_ping_ms', 'jitter_ms')})
summary.update(wait_ms=0, relayed=False)
SAVE_ID = '0123456789abcdef' * 2
SAVE_HASH = 'fedcba9876543210' * 4
metadata = dict(save_id=SAVE_ID, save_hash=SAVE_HASH)
zenkai = dict(summary, save_mode='zenkai', **metadata)
cases = [(summary, True), (zenkai, True)]  # Old clients remain valid.
for value in ('regular', 'zenkai', '', 'ZENKAI', None, 1, [], {}):
    cases.append((dict(summary, save_mode=value), value in ('regular', 'zenkai')))
cases.extend([
    (dict(summary, save_mode='regular', **metadata), False),
    (dict(summary, **metadata), False),
    (dict(summary, save_mode='zenkai', save_id=SAVE_ID), False),
    (dict(summary, save_mode='zenkai', save_hash=SAVE_HASH), False),
    (dict(summary, save_mode='zenkai', save_id='0' * 31, save_hash=SAVE_HASH), False),
    (dict(summary, save_mode='zenkai', save_id='A' * 32, save_hash=SAVE_HASH), False),
    (dict(summary, save_mode='zenkai', save_id=SAVE_ID, save_hash='A' * 64), False),
    (dict(summary, save_mode='zenkai', save_id=SAVE_ID, save_hash='0' * 65), False),
    (dict(summary, save_mode='zenkai', save_id=123, save_hash=SAVE_HASH), False),
    (dict(summary, save_mode='zenkai', save_id=SAVE_ID, save_hash=None), False),
    (dict(summary, save_mode='regular', unexpected=1), False),
])
for value, accepted in cases:
    try:
        Matchmaking.connection_summary(value)
        result = True
    except ApiError:
        result = False
    assert result == accepted, value

with tempfile.TemporaryDirectory(prefix='net-save-mode-') as tmp:
    service = Matchmaking(Path(tmp) / 'test.sqlite3', require_accounts=False)
    db = service.connect()
    with db:
        uid = db.execute("INSERT INTO battles(start_datetime,player1,player2,battle_type) VALUES(0,'Host','Guest',0)").lastrowid
        db.execute('INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(?,?,?,?,0,?)',
                   (uid, 'test-wire', digest('host-key'), digest('join-key'), 'request'))
        for key, connection in (('host-key', zenkai), ('join-key', dict(summary, save_mode='regular'))):
            service.battle(db, 'connection', dict(uid='test-wire', key=key, connection=connection), 1)
        row = service.battle_row(db, 'test-wire')
        assert row['p1_connection']['save_mode'] == 'zenkai'
        assert row['p1_connection']['save_id'] == SAVE_ID
        assert row['p1_connection']['save_hash'] == SAVE_HASH
        assert row['p2_connection']['save_mode'] == 'regular'
        assert 'save_id' not in row['p2_connection'] and 'save_hash' not in row['p2_connection']
        for invalid, accepted in cases:
            if accepted:
                continue
            try:
                service.battle(db, 'connection', dict(uid='test-wire', key='host-key', connection=invalid), 1)
                raise AssertionError('Malformed LAN save metadata accepted')
            except ApiError:
                pass
        assert service.battle_row(db, 'test-wire')['p1_connection']['save_id'] == SAVE_ID
        try:
            service.battle(db, 'connection', dict(uid='test-wire', key='wrong-key', connection=summary), 1)
            raise AssertionError('Unauthorized report accepted')
        except ApiError as error:
            assert error.status == 403
    db.close()

# Run the real Worker validator and its battle/connection method without D1 access.
source = (ROOT / 'server/src/full_service.js').read_text()
def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

js = '''class ApiError extends Error {}
const own = (v,k) => Object.prototype.hasOwnProperty.call(v,k);
const validInt = (n,lo,hi) => Number.isSafeInteger(n) && n >= lo && n <= hi;
''' + extract(source, 'function validateConnection(v) {') + '\nconst cases = ' + json.dumps(cases) + ''';
for (const [value, expected] of cases) {
 let accepted=true; try { validateConnection(value); } catch { accepted=false; }
 if (accepted!==expected) throw Error('Worker validator disagreement: '+JSON.stringify(value));
}
const hash = async x => x; // This test only checks report routing/storage.
const secretEqual = (a,b) => a === b;
class WorkerBattle {''' + extract(source, '    async battle(action, body, user, now) {') + '''}
async function checkStorage() {
 const session = {uid:17, host_hash:'host-key', join_hash:'join-key',
                  player1_uid:11, player2_uid:22, started:0};
 const stored = {p1_connection:null, p2_connection:null};
 const service = {
  first: async () => session,
  row: async () => ({end_datetime:null}),
  stmt: (sql,...args) => ({sql,args}),
  commit: async writes => {
   for (const {sql,args} of writes) {
    const found = /^UPDATE battle_sessions SET (p[12]_connection)=\\? WHERE uid=\\?$/.exec(sql);
    if (!found || args[1] !== 17) throw Error('Unexpected D1 write: '+sql);
    stored[found[1]] = args[0];
   }
  }
 };
 const a = await WorkerBattle.prototype.battle.call(service,'connection',
      {uid:'wire',key:'host-key',connection:cases[1][0]},{uid:11},1000);
 const b = await WorkerBattle.prototype.battle.call(service,'connection',
      {uid:'wire',key:'join-key',connection:{...cases[0][0],save_mode:'regular'}},{uid:22},1000);
 if (a.player!==1 || b.player!==2 ||
     JSON.parse(stored.p1_connection).save_id!==cases[1][0].save_id ||
     JSON.parse(stored.p1_connection).save_hash!==cases[1][0].save_hash ||
     'save_id' in JSON.parse(stored.p2_connection)) throw Error('Incorrect D1 JSON storage');
 for (const [value,accepted] of cases) {
  if (accepted) continue;
  try {
   await WorkerBattle.prototype.battle.call(service,'connection',
       {uid:'wire',key:'host-key',connection:value},{uid:11},1000);
   throw Error('Accepted invalid metadata');
  } catch(e) { if (!(e instanceof ApiError)) throw e; }
 }
 try {
  await WorkerBattle.prototype.battle.call(service,'connection',
      {uid:'wire',key:'join-key',connection:cases[1][0]},{uid:11},1000);
  throw Error('Wrong account succeeded');
 } catch(e) { if (!(e instanceof ApiError)) throw e; }
}
await checkStorage();
'''
node = shutil.which('node') or shutil.which('nodejs')
if node:
    subprocess.run([node, '--input-type=module', '-e', js], check=True)
else:
    chrome = shutil.which('chromium') or shutil.which('google-chrome') or '/opt/google/chrome/chrome'
    with tempfile.TemporaryDirectory(prefix='net-save-mode-js-') as tmp:
        page = Path(tmp) / 'test.html'
        page.write_text('<body><script>(async()=>{try {' + js +
                        ';document.body.textContent="VALIDATOR_PASS";} catch(e) {' +
                        'document.body.textContent="VALIDATOR_FAIL: "+e.message;}})();</script>')
        result = subprocess.run([chrome, '--headless', '--no-sandbox', '--disable-gpu',
                                 '--user-data-dir=' + tmp + '/profile', '--dump-dom', page.as_uri()],
                                capture_output=True, text=True, check=True, timeout=30)
        assert 'VALIDATOR_PASS' in result.stdout and 'VALIDATOR_FAIL' not in result.stdout, result.stdout

# Exercise real capture/restore functions across the isolated-save switch.
net = (ROOT / 'port/src/gs/net.c').read_text()
def c_function(name):
    return extract(net, ('static int ' if name == 'source_hex' else 'static void ') + name + '(')

c = r'''
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define NET_UNLOCK_WORDS 9
static uint32_t sUnlocks[2][NET_UNLOCK_WORDS];
static uint8_t data[0x2000], *gSaveData=data;
static int sSourceZenkai, active;
static char sSourceSaveId[33], sSourceSaveHash[65];
static void capture_custom(int player) { (void)player; }
int Port_ZenkaiEnabled(void) { return active && !getenv("ZENKAIPOWER_SAVES"); }
void Port_SaveCardPath(char *out, size_t size, const char *name) {
 (void)name;snprintf(out,size,"savegame/zenkaipower.sav");
}
int Port_ZenkaiMetadata(const char *path,char id[33],char hash[65]) {
 assert(!strcmp(path,"savegame/zenkaipower.sav") && !getenv("ZENKAIPOWER_SAVES"));
 strcpy(id,"0123456789abcdef0123456789abcdef");
 strcpy(hash,"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210");
 return 1;
}
''' + ''.join(c_function(n) + '\n' for n in ('source_hex','capture_source_metadata',
    'restore_source_metadata','capture_unlocks','restore_unlocks')) + r'''
int main(void) {
 unsetenv("ZENKAIPOWER_NET_BOT");unsetenv("ZENKAIPOWER_NET_UNLOCKS");unsetenv("ZENKAIPOWER_SAVES");
 active=1;capture_unlocks(0);assert(sSourceZenkai==1);
 assert(!strcmp(sSourceSaveId,"0123456789abcdef0123456789abcdef"));
 assert(!strcmp(sSourceSaveHash,"fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210"));
 setenv("ZENKAIPOWER_NET_SAVE_ID",sSourceSaveId,1);setenv("ZENKAIPOWER_NET_SAVE_HASH",sSourceSaveHash,1);
 setenv("ZENKAIPOWER_NET_SAVE_MODE","zenkai",1);setenv("ZENKAIPOWER_SAVES","net_session",1);
 assert(!Port_ZenkaiEnabled());sSourceZenkai=0;restore_unlocks(0);assert(sSourceZenkai==1);
 assert(!strcmp(sSourceSaveId,getenv("ZENKAIPOWER_NET_SAVE_ID")) && !strcmp(sSourceSaveHash,getenv("ZENKAIPOWER_NET_SAVE_HASH")));
 setenv("ZENKAIPOWER_NET_SAVE_MODE","regular",1);restore_unlocks(1);assert(sSourceZenkai==0 && !sSourceSaveId[0]);
 unsetenv("ZENKAIPOWER_NET_SAVE_MODE");restore_unlocks(1);assert(sSourceZenkai==0);
 unsetenv("ZENKAIPOWER_SAVES");active=0;capture_unlocks(1);assert(sSourceZenkai==0 && !sSourceSaveHash[0]);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='net-save-mode-c-') as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(c)
    subprocess.run(['cc', '-std=gnu99', '-fsanitize=undefined', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('Save-mode checks passed: ZENKAI metadata capture/restore, LAN and Worker JSON persistence, player ownership and validation.')
