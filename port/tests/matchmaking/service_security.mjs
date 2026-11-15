// Run with Node 24+ after npm ci in server.
// Uses real SQL and production handlers; never connects to Cloudflare.
import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFileSync, readdirSync} from 'node:fs';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
import {test} from 'node:test';
import {FullService, hash, codeHash} from '../../../server/src/full_service.js';

const project = new URL('../../../server/', import.meta.url);
const require = createRequire(new URL('package.json', project));
const {build} = require('esbuild');
// Use a synthetic accepted build so authorization tests never depend on local release outputs.
const policy = {scheme: 'zenkaipower-binaries-v2', files: 1,
    signatures: {['ab'.repeat(32)]: 'test-only'}};
const bundle = await build({entryPoints: [fileURLToPath(new URL('src/worker.js', project))], bundle: true,
    write: false, format: 'esm', platform: 'node', plugins: [{name: 'test-build-policy', setup(build) {
        build.onLoad({filter: /game_data_signature\.json$/}, () => ({contents: JSON.stringify(policy), loader: 'json'}));
    }}]});
const {workerHandler, MatchCoordinator, default: edgeWorker} = await import('data:text/javascript;base64,' + Buffer.from(bundle.outputFiles[0].text).toString('base64'));
const wire = '00000000-0000-4000-8000-000000000002';

async function fixture(t) {
    const db = new DatabaseSync(':memory:');
    t.after(() => db.close());
    db.exec('PRAGMA foreign_keys=ON');
    for (const migration of readdirSync(new URL('migrations/', project)).filter(p => p.endsWith('.sql')).sort())
        db.exec(readFileSync(new URL('migrations/' + migration, project), 'utf8'));
    // Minimal D1 adapter; batch transaction semantics match production D1.
    const DB = {
        prepare(sql) {
            const stmt = db.prepare(sql);
            const bound = args => ({
                bind: (...values) => bound(values),
                first: async () => stmt.get(...args) ?? null,
                all: async () => ({results: stmt.all(...args)}),
                run: async () => stmt.run(...args),
                execute: () => stmt.run(...args)
            });
            return bound([]);
        },
        async batch(statements) {
            db.exec('BEGIN');
            try { const out = statements.map(s => s.execute()); db.exec('COMMIT'); return out; }
            catch (error) { db.exec('ROLLBACK'); throw error; }
        }
    };
    const now = Date.now();
    for (const uid of [1, 2, 3]) {
        db.prepare('INSERT INTO accounts VALUES(?,?,?,?,?,?,?)').run(uid, 'User' + uid, 'user' + uid, '00'.repeat(16), 'unused', 1, now);
        db.prepare('INSERT INTO account_tokens VALUES(?,?,?)').run(await hash(String(uid).repeat(64)), uid, now + 86400000);
    }
    db.exec("INSERT INTO battles(uid,start_datetime,end_datetime,player1,player2,player1_uid,player2_uid,battle_type,ranked,winner,p1_points_won,p2_points_won) VALUES(1,1,2,'user1','user2',1,2,1,1,1,500,500)");
    db.prepare("INSERT INTO battles(uid,start_datetime,player1,player2,player1_uid,player2_uid,battle_type,ranked) VALUES(2,?,'user1','user2',1,2,1,1)").run(now);
    db.prepare('INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(2,?,?,?,?,?)')
        .run(wire, await hash('host'), await hash('join'), now / 1000, 'started');
    const service = new FullService({DB});
    const writes = [];
    await service.freeze({uid: 2, ranked: 1, player1_uid: 1, player2_uid: 2}, now, writes);
    await service.commit(writes);
    const request = async (side = 1, action = 'end', changes = {}) => {
        const body = {uid: wire, key: side === 1 ? 'host' : 'join', account_token: String(side).repeat(64),
            winner: 'p1', finish_type: 'ok', battle_duration: 45, p1_points_won: 100, p2_points_won: -60, ...changes};
        const response = await workerHandler.fetch(new Request('https://local.test/v1/battle/' + action,
            {method: 'POST', body: JSON.stringify(body), headers: {'cf-connecting-ip': '127.0.0.1'}}), {DB});
        return response.json();
    };
    return {db, DB, service, request};
}

test('host alone and retries cannot award points', async t => {
    const {request} = await fixture(t);
    await request(); await request();
    const result = await request(1, 'get');
    assert.equal(result.battle.result_status, 'pending');
    assert.equal(result.battle.winner, null);
    assert.deepEqual([result.p1_rank_points, result.p2_rank_points], [500, 500]);
});

test('matching peer reports settle once with performance rewards in either order', async t => {
    const {request} = await fixture(t);
    await request(2);
    const result = await request(1, 'end', {battle_duration: 46});
    assert.equal(result.battle.result_status, 'confirmed');
    assert.deepEqual([result.battle.p1_points_won, result.battle.p2_points_won], [100, -60]);
    for (const side of [1, 2]) {
        const retry = await request(side, 'end', {battle_duration: 50});
        assert.deepEqual([retry.p1_rank_points, retry.p2_rank_points], [600, 440]);
    }
});

test('disputed reports are immutable and cannot change rank', async t => {
    const {request} = await fixture(t);
    await request();
    const result = await request(2, 'end', {winner: 'p2'});
    assert.equal(result.battle.result_status, 'disputed');
    await assert.rejects(request(2), e => e.status === 409);
    const after = await request(1, 'get');
    assert.equal(after.battle.winner, null);
    assert.deepEqual([after.p1_rank_points, after.p2_rank_points], [500, 500]);
});

test('performance rewards vary and scale exactly once on the server', async t => {
    const {request, db} = await fixture(t);
    db.exec('UPDATE rank_battle_scoring SET p1_start=3000,p2_start=2000,tolerance=250 WHERE battle_uid=2');
    const points = {p1_points_won: 210, p2_points_won: -77};
    await request(1, 'end', points);
    for (const side of [2, 1, 2]) {
        const result = await request(side, 'end', points);
        assert.equal(result.battle.result_status, 'confirmed');
        assert.deepEqual([result.battle.p1_points_won, result.battle.p2_points_won], [120, -44]);
    }
});

test('base point disagreement prevents settlement', async t => {
    const {request} = await fixture(t);
    await request(1, 'end', {p1_points_won: 240});
    const result = await request(2, 'end', {p1_points_won: 241});
    assert.equal(result.result_status, 'disputed');
    assert.deepEqual([result.p1_rank_points, result.p2_rank_points], [500, 500]);
});

test('invalid base point reports cannot award points', async t => {
    const {request} = await fixture(t);
    for (const value of [null, true, 1.5, '100', 2147483648, -2147483649])
        await assert.rejects(request(1, 'end', {p1_points_won: value}), e => e.status === 400);
});

test('ties award zero and finish disagreement remains disputed', async t => {
    const {request} = await fixture(t);
    await request(1, 'end', {winner: 'tie'});
    const result = await request(2, 'end', {winner: 'tie'});
    assert.deepEqual([result.battle.p1_points_won, result.battle.p2_points_won], [0, 0]);
    await assert.rejects(request(2, 'end', {winner: 'tie', finish_type: 'perfect'}), e => e.status === 409);
});

test('account and participant ownership remain required independently of build filters', async t => {
    const {request, DB} = await fixture(t);
    for (const changes of [{key: 'host'}, {key: 'bogus'}, {account_token: '3'.repeat(64)}, {account_token: ''}])
        await assert.rejects(request(2, 'end', changes), e => e.status === 403);
    const data = Object.keys(policy.signatures)[0] ?? 'toString';
    const call = (path, body) => workerHandler.fetch(new Request('https://local.test/v1/' + path,
        {method: 'POST', body: JSON.stringify(body)}), {DB});
    const check = await (await call('data-check', {data})).json();
    assert.equal(check.expected, Object.hasOwn(policy.signatures, data) ? data : '');
    assert.equal(check.build_attestation, false);
    assert.equal(check.check, 'compatibility');
    for (const path of ['host', 'quick']) {
        await assert.rejects(call(path, {data, queue: 'RS'}), e => e.status === 403);
        const refused = await call(path, {data: 'unknown-build', queue: 'RS'});
        assert.equal(refused.status, 403);
        assert.deepEqual(await refused.json(), {error: 'game data'});
    }
});

test('inactive opponent prevents settlement', async t => {
    const {request, db} = await fixture(t);
    await request();
    db.exec('UPDATE accounts SET active=0 WHERE uid=2');
    await assert.rejects(request(), e => e.status === 403);
    assert.equal(db.prepare('SELECT winner FROM battles WHERE uid=2').get().winner, null);
});

test('server retains rank gap scaling and balance caps', async t => {
    const {request, db} = await fixture(t);
    db.exec('UPDATE rank_battle_scoring SET p1_start=3000,p2_start=2000,tolerance=250 WHERE battle_uid=2');
    await request();
    const result = await request(2);
    assert.deepEqual([result.battle.p1_points_won, result.battle.p2_points_won], [57, -34]);
});

test('unranked remains host-only and awards zero', async t => {
    const {request, db} = await fixture(t);
    db.exec('UPDATE battles SET ranked=0 WHERE uid=2');
    await assert.rejects(request(2), e => e.status === 403);
    const result = await request();
    assert.deepEqual([result.battle.p1_points_won, result.battle.p2_points_won], [0, 0]);
});

test('ranked balances remain capped', async t => {
    const {request, db} = await fixture(t);
    db.exec('UPDATE rank_battle_scoring SET p1_start=9999,p2_start=0 WHERE battle_uid=2');
    await request();
    const result = await request(2);
    assert.deepEqual([result.battle.p1_points_won, result.battle.p2_points_won], [0, 0]);
});

function accountCoordinator(DB) {
    let tail = Promise.resolve();
    const state = {blockConcurrencyWhile(fn) {
        const result = tail.then(fn); tail = result.catch(() => {}); return result;
    }};
    return new MatchCoordinator(state, {DB});
}

function accountRequest(path, body) {
    return new Request('https://local.test/' + path, {method: 'POST',
        headers: {'cf-connecting-ip': '127.0.0.1'}, body: JSON.stringify(body)});
}

test('edge registration/login/confirmation work without accepting supplied hashes', async t => {
    const {DB} = await fixture(t), coordinator = accountCoordinator(DB);
    const env = {DB, COORDINATOR: {idFromName: name => name, get: () => coordinator}};
    const call = (action, body) => edgeWorker.fetch(accountRequest('v1/account/' + action, body), env);
    const registration = await call('register', {username: 'FreshUser', computed: '0'.repeat(128)});
    assert.equal(registration.status, 200);
    const user = await registration.json();
    const login = await call('login', {username: 'FreshUser', code: user.code.toLowerCase()});
    assert.equal(login.status, 200);
    assert.equal((await login.json()).user_uid, user.user_uid);
    assert.equal((await call('login', {username: 'FreshUser', code: '000000', computed: '0'.repeat(128)})).status, 403);
    const prepared = await (await call('prepare', {username: 'TicketUser'})).json();
    const confirmed = await call('confirm', {ticket: prepared.ticket});
    assert.equal(confirmed.status, 200);
    assert.equal((await confirmed.json()).username, 'ticketuser');
    for (const route of ['_account/prepare', '_account/finish'])
        assert.equal((await edgeWorker.fetch(accountRequest(route, {job: 'forged', computed: '0'.repeat(128)}), env)).status, 404);
});

test('outstanding account jobs are bounded without holding matchmaking gate', async t => {
    const {DB} = await fixture(t), coordinator = accountCoordinator(DB);
    const prepare = username => coordinator.fetch(accountRequest('_account/prepare',
        {action: 'register', body: {username}, ip: '127.0.0.1'}));
    assert.equal((await prepare('PendingOne')).status, 200);
    assert.equal((await prepare('PendingTwo')).status, 200);
    assert.equal((await prepare('PendingThree')).status, 429);
    assert.equal((await coordinator.fetch(accountRequest('v1/hello', {}))).status, 200);
    for (const job of coordinator.hashJobs.values()) job.expires = 0;
    assert.equal((await prepare('AfterExpiry')).status, 200);
});

test('distributed hash budget survives coordinator restart and counts failed logins', async t => {
    const fixedNow = Date.now();
    t.mock.method(Date, 'now', () => fixedNow);
    const {DB} = await fixture(t);
    let coordinator = accountCoordinator(DB);
    for (let i = 0; i < 30; i++) {
        const prepared = await coordinator.fetch(accountRequest('_account/prepare', {
            action: 'login', body: {username: 'MissingUser' + i, code: 'ABCDEF'}, ip: '192.0.2.' + i}));
        assert.equal(prepared.status, 200);
        const {job} = await prepared.json();
        const finished = await coordinator.fetch(accountRequest('_account/finish', {job, computed: '0'.repeat(128)}));
        assert.equal(finished.status, 403);
    }
    coordinator = accountCoordinator(DB);
    const refused = await coordinator.fetch(accountRequest('_account/prepare', {
        action: 'login', body: {username: 'AnotherMissing', code: 'ABCDEF'}, ip: '203.0.113.1'}));
    assert.equal(refused.status, 429);
    assert.equal(coordinator.hashJobs.size, 0);
});

test('login completion rechecks account credentials and jobs cannot replay', async t => {
    const {DB, db} = await fixture(t), coordinator = accountCoordinator(DB);
    const salt = '12'.repeat(16), computed = await codeHash('ABCDEF', salt);
    db.prepare('UPDATE accounts SET code_salt=?,code_hash=? WHERE uid=1').run(salt, computed);
    const plan = await (await coordinator.fetch(accountRequest('_account/prepare', {
        action: 'login', body: {username: 'User1', code: 'ABCDEF'}, ip: '127.0.0.1'}))).json();
    db.exec('UPDATE accounts SET active=0 WHERE uid=1');
    const finish = () => coordinator.fetch(accountRequest('_account/finish', {job: plan.job, computed}));
    assert.equal((await finish()).status, 403);
    assert.equal((await finish()).status, 409);
});


test('bot account requires private credential for login and existing tokens', async t => {
    const {service} = await fixture(t);
    const credential = 'ab'.repeat(32);
    service.env.BOT_ACCOUNT_UID = '1';
    service.env.BOT_CREDENTIAL_SHA256 = await hash(credential);
    const token = {account_token: '1'.repeat(64)};
    await assert.rejects(service.auth(token, Date.now()), /bot access denied/);
    await assert.rejects(service.auth({...token, bot_credential: 'cd'.repeat(32)}, Date.now()), /bot access denied/);
    assert.equal((await service.auth({...token, bot_credential: credential}, Date.now())).uid, 1);
    await assert.rejects(service.auth({account_token: '2'.repeat(64), bot: true, bot_credential: credential}, Date.now()), /bot access denied/);
    assert.equal((await service.auth({account_token: '2'.repeat(64)}, Date.now())).uid, 2);
    await assert.rejects(service.prepareAccount('login', {username: 'user1', code: 'ABC123'}, '127.0.0.1', Date.now()), /bot access denied/);
    const plan = await service.prepareAccount('login', {username: 'user1', code: 'ABC123', bot: true, bot_credential: credential}, '127.0.0.1', Date.now());
    assert.equal(plan.body.bot_credential, credential);
    service.env.BOT_CREDENTIAL_SHA256 = await hash('ef'.repeat(32));
    await assert.rejects(service.auth({...token, bot_credential: credential}, Date.now()), /bot access denied/);
    delete service.env.BOT_CREDENTIAL_SHA256;
    await assert.rejects(service.auth({...token, bot_credential: credential}, Date.now()), /bot access denied/);
    delete service.env.BOT_ACCOUNT_UID;
    await assert.rejects(service.auth({...token, bot: true, bot_credential: credential}, Date.now()), /bot access denied/);
});


test('address cleanup expires rooms and IP limits without resetting active windows', async t => {
    const {db, service} = await fixture(t);
    const now = Date.now(), seconds = Math.floor(now / 1000);
    const insert = db.prepare('INSERT INTO rooms(code,made,version,ip,host_key,host_name,host_addrs) VALUES(?,?,1,?,?,?,?)');
    insert.run('OLD123', seconds - 601, '192.0.2.1', 'old-key', 'User1', '192.0.2.1:1234');
    insert.run('NEW123', seconds - 599, '192.0.2.2', 'new-key', 'User2', '192.0.2.2:1234');
    db.prepare('INSERT INTO account_rooms(code,host_uid) VALUES(?,1)').run('OLD123');
    const limits = [['host:192.0.2.1',3600], ['quick:192.0.2.1',3600], ['miss:192.0.2.1',3600],
        ['relay:192.0.2.1',86400], ['account:ip:hash',900], ['account:hash:ip:hash',60], ['report:ip:hash',60]];
    for (const [key, window] of limits) {
        db.prepare('INSERT INTO used VALUES(?,?,3)').run(key, seconds - window);
        db.prepare('INSERT INTO used VALUES(?,?,4)').run(key, seconds - window + 1);
    }
    db.prepare('INSERT INTO used VALUES(?,?,5)').run('relayoff', 1);
    await service.pruneAddresses(now);
    assert.equal(db.prepare('SELECT COUNT(*) AS n FROM rooms').get().n, 1);
    assert.equal(db.prepare('SELECT COUNT(*) AS n FROM account_rooms').get().n, 0);
    for (const [key] of limits) assert.deepEqual(db.prepare('SELECT n FROM used WHERE what=?').all(key).map(r=>r.n), [4]);
    assert.equal(db.prepare("SELECT n FROM used WHERE what='relayoff'").get().n, 5);
    assert.equal(db.prepare('SELECT COUNT(*) AS n FROM battles').get().n, 2);
    await service.pruneAddresses(now); // Repeated cleanup is safe.
});
