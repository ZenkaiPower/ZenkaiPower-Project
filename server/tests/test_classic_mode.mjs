import assert from 'node:assert/strict';
const {FullService, ApiError, hash} = await import(process.env.FULL_SERVICE_MODULE || '../src/full_service.js');
const service = new FullService({DB: {prepare(sql) { return {bind(...args) { return {sql, args}; }}; }}});
const user = {uid: 42};
assert.deepEqual(service.versionWrites(user, {classic_mode: true}), []);
assert.equal(service.versionWrites(user, {rel: 129, classic_mode: true}).length, 1);
const session = {uid: 5, player1_uid: 42, player2_uid: 43,
    host_hash: await hash('host'), join_hash: await hash('join'), started: Date.now() / 1000};
service.first = async () => session;
service.row = async () => ({});
let writes;
service.commit = async value => { writes = value; };
for (const [uid, key, field, enabled] of [[42, 'host', 'p1_classic_mode', true], [43, 'join', 'p2_classic_mode', false]]) {
    await service.battle('save', {uid: 'battle', key, classic_mode: enabled}, {uid}, Date.now());
    assert.equal(writes[0].sql, `UPDATE battles SET ${field}=? WHERE uid=? AND ${field} IS NULL`);
    assert.deepEqual(writes[0].args, [Number(enabled), 5]);
}
for (const invalid of [0, 1, 'true', [], {}]) {
    await assert.rejects(service.battle('save', {uid: 'battle', key: 'host', classic_mode: invalid}, user, Date.now()), ApiError);
}
await assert.rejects(service.battle('save', {uid: 'battle', key: 'join', classic_mode: true}, user, Date.now()), ApiError);
console.log('Cloudflare battle-only Classic Mode checks passed');

service.accountLimits = service.botAccess = async () => {};
service.nextId = async () => 7;
service.first = async () => null;
const prepared = {salt: 'salt', computed: 'hash', code: 'ABCDEF'};
await service.account('register', {username: 'CreatedUser', rel: 129}, 'test', Date.now(), prepared);
assert.equal(writes.find(w => w.sql.startsWith('INSERT INTO accounts(')).args[6], 129);
await service.account('register', {username: 'LegacyUser'}, 'test', Date.now(), prepared);
assert.equal(writes.find(w => w.sql.startsWith('INSERT INTO accounts(')).args[6], null);
const pending = {display_name: 'ConfirmUser', username: 'confirmuser', code_salt: 'salt', code_hash: 'hash', user_uid: null};
service.first = async sql => sql.includes('account_registrations') ? pending : null;
await service.account('confirm', {ticket: 'a'.repeat(64), rel: 130}, 'test', Date.now());
assert.equal(writes.find(w => w.sql.startsWith('INSERT INTO accounts(')).args[6], 130);
pending.user_uid = 7;
service.first = async sql => sql.includes('account_registrations') ? pending : {uid: 7, username: 'confirmuser'};
await service.account('confirm', {ticket: 'a'.repeat(64), rel: 131}, 'test', Date.now());
assert(writes.every(w => !w.sql.includes('created_game_version')));
for (const rel of [true, -1, 2147483648, '129']) {
    await assert.rejects(service.account('register', {username: 'InvalidUser', rel}, 'test', Date.now(), prepared), ApiError);
}
console.log('Cloudflare account creation version checks passed');
