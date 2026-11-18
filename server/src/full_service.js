// Account and battle wire contract shared with lan_server.py. All mutations of one
// operation are committed by a single D1 batch. The Worker routes every request
// through one Durable Object so validation reads cannot race other writers.
import { scryptAsync } from '@noble/hashes/scrypt.js';

export class ApiError extends Error {
    constructor(message, status = 400) { super(message); this.status = status; }
}
const slots = [1, 2].flatMap(p => [1, 2, 3, 4, 5].map(c => `p${p}c${c}`));
const types = {unknown: 0, S: 1, T: 2, D10: 3, D15: 4, D20: 5};
const winners = {tie: 0, p1: 1, p2: 2};
const finishes = {ok: 0, perfect: 1, timeout: 2, ringout: 3};
const alphabet = 'ABCDEFGHJKMNPQRSTUVWXYZ23456789';
const own = (o, k) => Object.hasOwn(o, k);
const hex = bytes => Array.from(bytes, b => b.toString(16).padStart(2, '0')).join('');
const random = n => hex(crypto.getRandomValues(new Uint8Array(n)));
export const hash = async value => hex(new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(value))));
const validInt = (n, low, high) => Number.isSafeInteger(n) && n >= low && n <= high;
function username(name) {
    if (typeof name !== 'string' || !/^[A-Za-z][A-Za-z0-9_-]{1,14}[A-Za-z0-9]$/.test(name) || (name.match(/[a-z]/gi) || []).length < 3 || name.toLowerCase() === 'player')
        throw new ApiError('Invalid name, please chose another one.');
    return name;
}
function secretEqual(a, b) {
    if (typeof a !== 'string' || typeof b !== 'string') return false;
    let diff = a.length ^ b.length;
    for (let i = 0; i < Math.max(a.length, b.length); i++) diff |= (a.charCodeAt(i) || 0) ^ (b.charCodeAt(i) || 0);
    return diff === 0;
}
export async function codeHash(code, salt) {
    return hex(await scryptAsync(code, Uint8Array.from(salt.match(/../g), b => parseInt(b, 16)),
        {N: 16384, r: 8, p: 1, dkLen: 64, maxmem: 64 * 1024 * 1024, asyncTick: 1}));
}
function accessCode() {
    // Rejection sampling avoids bias from a non-power-of-two alphabet.
    let out = '';
    while (out.length < 6) {
        const b = crypto.getRandomValues(new Uint8Array(1))[0];
        if (b < Math.floor(256 / alphabet.length) * alphabet.length) out += alphabet[b % alphabet.length];
    }
    return out;
}
export function londonTimestamp(ms) {
    const parts = new Intl.DateTimeFormat('en-GB', {timeZone: 'Europe/London', year: 'numeric', month: '2-digit', day: '2-digit',
        hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23', timeZoneName: 'longOffset'}).formatToParts(ms);
    const p = Object.fromEntries(parts.map(x => [x.type, x.value]));
    return `${p.year}-${p.month}-${p.day}T${p.hour}:${p.minute}:${p.second}.${String(ms % 1000).padStart(3, '0')}${p.timeZoneName === 'GMT' ? '+00:00' : p.timeZoneName.replace('GMT', '')}`;
}
export function populationTolerance(points) {
    if (points.length < 20) return 250;
    const s = [...points].sort((a, b) => a - b);
    const percentile = f => { const p = (s.length - 1) * f, i = Math.floor(p); return s[i] + (s[Math.min(i + 1, s.length - 1)] - s[i]) * (p - i); };
    return Math.min(500, Math.max(100, Math.floor((percentile(.75) - percentile(.25)) * .1 + .5)));
}
export function adjustedPoints(start, winner, normal, tolerance) {
    if (winner === 'tie') return [0, 0];
    const win = winner === 'p1' ? 0 : 1, lose = 1 - win;
    const denominator = 1000 + Math.max(0, start[win] - start[lose] - tolerance);
    return start.map((v, i) => {
        const magnitude = Math.max(1, Math.floor((Math.abs(normal[i]) * 1000 + Math.floor(denominator / 2)) / denominator));
        return (i === win ? 1 : -1) * Math.min(Math.max(0, i === win ? 9999 - v : v), magnitude);
    });
}
export class FullService {
    constructor(env) { this.env = env; this.db = env.DB; }
    stmt(sql, ...args) { return this.db.prepare(sql).bind(...args); }
    first(sql, ...args) { return this.stmt(sql, ...args).first(); }
    async rows(sql, ...args) { return (await this.stmt(sql, ...args).all()).results; }
    async commit(statements) { if (statements.length) await this.db.batch(statements); }
    async limit(key, slot, max) {
        const row = await this.stmt('INSERT INTO used(what,slot,n) VALUES(?,?,1) ON CONFLICT(what,slot) DO UPDATE SET n=n+1 RETURNING n', key, slot).first();
        if (row.n > max) throw new ApiError('too many account requests; try again later', 429);
    }
    async pruneAddresses(now) {
        const seconds = Math.floor(now / 1000);
        await this.commit([
            this.stmt('DELETE FROM rooms WHERE made < ?', seconds - 600),
            this.stmt("DELETE FROM used WHERE (what LIKE 'host:%' OR what LIKE 'quick:%' OR what LIKE 'miss:%') AND slot <= ?", seconds - 3600),
            this.stmt("DELETE FROM used WHERE what LIKE 'relay:%' AND slot <= ?", seconds - 86400),
            this.stmt("DELETE FROM used WHERE what LIKE 'account:ip:%' AND slot <= ?", seconds - 900),
            this.stmt("DELETE FROM used WHERE (what LIKE 'account:hash:ip:%' OR what LIKE 'report:ip:%') AND slot <= ?", seconds - 60),
        ]);
    }
    async botAccess(user, body) {
        const protectedAccount = user && String(this.env.BOT_ACCOUNT_UID || '') === String(user.uid);
        if (body.bot !== undefined && typeof body.bot !== 'boolean') throw new ApiError('invalid bot mode');
        if (!protectedAccount && body.bot === true) throw new ApiError('bot access denied', 403);
        if (!protectedAccount) return;
        const digest = this.env.BOT_CREDENTIAL_SHA256;
        if (!/^[a-f0-9]{64}$/.test(digest || '') ||
            typeof body.bot_credential !== 'string' || !/^[a-f0-9]{64}$/.test(body.bot_credential) ||
            !secretEqual(await hash(body.bot_credential), digest)) throw new ApiError('bot access denied', 403);
    }
    async auth(body, now) {
        if (!/^[a-f0-9]{64}$/.test(body.account_token || '')) throw new ApiError('initialize your player data before matchmaking', 403);
        const user = await this.first('SELECT a.* FROM accounts a JOIN account_tokens t ON t.user_uid=a.uid WHERE t.token_hash=? AND t.expires_datetime>? AND a.active=1', await hash(body.account_token), now);
        if (!user) throw new ApiError('account login expired or inactive; initialize your data again', 403);
        await this.botAccess(user, body);
        await this.commit(this.versionWrites(user, body));
        return user;
    }
    versionWrites(user, body) {
        const writes = [];
        if (body.rel !== undefined && body.rel !== null) {
            if (!validInt(body.rel, 0, 2147483647)) throw new ApiError("invalid game version");
            writes.push(this.stmt("UPDATE accounts SET game_version=? WHERE uid=?", body.rel, user.uid));
        }
        return writes;
    }
    async token(user, now, body, writes = []) {
        const token = random(32), expires = now + 90 * 86400000;
        writes.push(...this.versionWrites(user, body));
        writes.push(this.stmt('DELETE FROM account_tokens WHERE expires_datetime<=?', now),
            this.stmt('INSERT INTO account_tokens VALUES(?,?,?)', await hash(token), user.uid, expires));
        await this.commit(writes);
        return {user_uid: user.uid, username: user.username, display_name: user.display_name, token, expires};
    }
    async nextId(table) {
        // The coordinator is the only application writer. Honor deleted-ID high water marks.
        const row = await this.first(`SELECT MAX(COALESCE((SELECT MAX(uid) FROM ${table}),0),COALESCE((SELECT seq FROM sqlite_sequence WHERE name=?),0))+1 AS id`, table);
        if (!Number.isSafeInteger(row.id)) throw new ApiError('database ID capacity reached', 503);
        return row.id;
    }
    async accountLimits(action, body, ip, now) {
        const login = action === 'login', slot = Math.floor(now / 900000) * 900;
        await this.limit('account:ip:' + await hash(ip), slot, login ? 30 : 120);
        if (login) await this.limit('account:name:' + await hash(String(body.username || '').slice(0, 16).toLowerCase()), slot, 5);
    }
    async prepareAccount(action, body, ip, now) {
        if (!['login', 'prepare', 'register'].includes(action)) throw new ApiError('unknown account endpoint', 404);
        await this.accountLimits(action, body, ip, now);
        const name = username(body.username), user = await this.first('SELECT * FROM accounts WHERE username=?', name.toLowerCase());
        await this.botAccess(user, body);
        if (action === 'login' && (typeof body.code !== 'string' || !/^[A-Za-z0-9]{6}$/.test(body.code)))
            throw new ApiError('code must contain six letters or numbers');
        if (action !== 'login' && user) throw new ApiError('username already exists', 409);
        const slot = Math.floor(now / 60000) * 60;
        // Persisted budgets precede expensive work, including failed logins.
        await this.limit('account:hash:ip:' + await hash(ip), slot, 10);
        await this.limit('account:hash:global', slot, 30);
        return {action, body: {username: name, rel: body.rel, bot: body.bot, bot_credential: body.bot_credential}, ip, code: action === 'login' ? body.code.toUpperCase() : accessCode(),
            salt: action === 'login' ? user?.code_salt || '00'.repeat(16) : random(16), verifier: user?.code_hash ?? null};
    }
    async account(action, body, ip, now, prepared = null) {
        if (['register', 'confirm'].includes(action) && body.rel !== undefined && body.rel !== null &&
            !validInt(body.rel, 0, 2147483647)) throw new ApiError('invalid game version');
        if (!prepared) await this.accountLimits(action, body, ip, now);
        if (['session', 'logout', 'stats'].includes(action)) {
            const user = await this.auth(body, now);
            if (action === 'stats') return {rank_points: await this.points(user.uid)};
            if (action === 'logout') await this.commit([this.stmt('DELETE FROM account_tokens WHERE token_hash=?', await hash(body.account_token))]);
            return {user_uid: user.uid, username: user.username, display_name: user.display_name, active: true};
        }
        if (action === 'confirm') {
            if (!/^[a-f0-9]{64}$/.test(body.ticket || '')) throw new ApiError('invalid registration ticket', 403);
            const ticketHash = await hash(body.ticket);
            const pending = await this.first('SELECT * FROM account_registrations WHERE ticket_hash=? AND expires_datetime>?', ticketHash, now);
            if (!pending) throw new ApiError('registration expired; generate a new access code', 403);
            username(pending.display_name);
            await this.botAccess({uid: pending.user_uid}, body);
            if (pending.user_uid !== null) {
                const user = await this.first('SELECT * FROM accounts WHERE uid=? AND active=1', pending.user_uid);
                if (!user) throw new ApiError('inactive account', 403);
                await this.botAccess(user, body);
                return this.token(user, now, body);
            }
            if (await this.first('SELECT 1 FROM accounts WHERE username=?', pending.username)) throw new ApiError('username already exists', 409);
            const uid = await this.nextId('accounts');
            return this.token({uid, username: pending.username, display_name: pending.display_name}, now, body, [
                this.stmt('INSERT INTO accounts(uid,display_name,username,code_salt,code_hash,created_datetime,created_game_version) VALUES(?,?,?,?,?,?,?)', uid, pending.display_name, pending.username, pending.code_salt, pending.code_hash, now, body.rel ?? null),
                this.stmt('UPDATE account_registrations SET user_uid=? WHERE ticket_hash=?', uid, ticketHash)]);
        }
        if (!['lookup', 'prepare', 'register', 'login'].includes(action)) throw new ApiError('unknown account endpoint', 404);
        const name = username(body.username), canonical = name.toLowerCase();
        const user = await this.first('SELECT * FROM accounts WHERE username=?', canonical);
        if (action === 'lookup') return {exists: !!user, active: !!user?.active};
        // A hash job is created and completed only through the private coordinator
        // protocol. Never do scrypt while holding the global mutation gate.
        if (!prepared) throw new ApiError('account hashing coordinator required', 503);
        if (action === 'login') {
            if (!user || !user.active || user.code_salt !== prepared.salt || user.code_hash !== prepared.verifier ||
                !secretEqual(prepared.computed, user.code_hash)) throw new ApiError('invalid username or code, or inactive account', 403);
            await this.botAccess(user, body);
            return this.token(user, now, body);
        }
        if (user) throw new ApiError('username already exists', 409);
        const {code, salt, computed: hashed} = prepared;
        if (action === 'prepare') {
            const ticket = random(32);
            await this.commit([this.stmt('DELETE FROM account_registrations WHERE expires_datetime<=?', now),
                this.stmt('INSERT INTO account_registrations VALUES(?,?,?,?,?,?,NULL)', await hash(ticket), name, canonical, salt, hashed, now + 900000)]);
            return {code, ticket};
        }
        const uid = await this.nextId('accounts');
        return {...await this.token({uid, username: canonical, display_name: name}, now, body, [this.stmt(
            'INSERT INTO accounts(uid,display_name,username,code_salt,code_hash,created_datetime,created_game_version) VALUES(?,?,?,?,?,?,?)', uid, name, canonical, salt, hashed, now, body.rel ?? null)]), code};
    }
    async points(uid) {
        if (uid === null) return 0;
        return (await this.first('SELECT MIN(9999,MAX(0,COALESCE(SUM(CASE WHEN player1_uid=? THEN p1_points_won ELSE p2_points_won END),0))) AS points FROM battles WHERE ranked=1 AND (player1_uid=? OR player2_uid=?) AND interrupted=0 AND end_datetime IS NOT NULL AND winner IS NOT NULL', uid, uid, uid)).points;
    }
    async policy(now, writes) {
        const day = Math.floor(now / 86400000);
        let policy = await this.first('SELECT * FROM rank_daily_policy WHERE day=?', day);
        if (!policy) {
            const active = await this.rows(`WITH active AS (
                SELECT uid FROM accounts WHERE active=1 AND uid IN (
                    SELECT player1_uid FROM battles WHERE ranked=1 AND interrupted=0 AND winner IS NOT NULL AND end_datetime BETWEEN ? AND ?
                    UNION SELECT player2_uid FROM battles WHERE ranked=1 AND interrupted=0 AND winner IS NOT NULL AND end_datetime BETWEEN ? AND ?)
                ), scores AS (
                    SELECT player1_uid AS uid,COALESCE(p1_points_won,0) AS points FROM battles WHERE ranked=1 AND interrupted=0 AND winner IS NOT NULL AND end_datetime IS NOT NULL
                    UNION ALL SELECT player2_uid,COALESCE(p2_points_won,0) FROM battles WHERE ranked=1 AND interrupted=0 AND winner IS NOT NULL AND end_datetime IS NOT NULL
                ) SELECT a.uid,MIN(9999,MAX(0,COALESCE(SUM(s.points),0))) AS points FROM active a LEFT JOIN scores s USING(uid) GROUP BY a.uid`,
                now - 30 * 86400000, now, now - 30 * 86400000, now);
            const points = active.map(row => row.points);
            policy = {day, tolerance: populationTolerance(points), population: points.length};
            writes.push(this.stmt('INSERT INTO rank_daily_policy VALUES(?,?,?)', day, policy.tolerance, policy.population));
        }
        return policy;
    }
    async freeze(battle, now, writes) {
        let score = await this.first('SELECT * FROM rank_battle_scoring WHERE battle_uid=?', battle.uid);
        if (!score && battle.ranked && battle.player1_uid !== null && battle.player2_uid !== null) {
            const policy = await this.policy(now, writes);
            score = {p1_start: await this.points(battle.player1_uid), p2_start: await this.points(battle.player2_uid), tolerance: policy.tolerance, policy_day: policy.day};
            writes.push(this.stmt('INSERT INTO rank_battle_scoring VALUES(?,?,?,?,?)', battle.uid, score.p1_start, score.p2_start, score.tolerance, score.policy_day));
        }
        return score;
    }
    async ranking(body) {
        const offset = body.offset ?? 0, limit = body.limit ?? 3;
        if (!validInt(offset, 0, 1000000) || !validInt(limit, 1, 50)) throw new ApiError('offset must be 0–1000000 and limit 1–50');
        const cte = `WITH scores AS (SELECT player1_uid AS uid,COALESCE(p1_points_won,0) AS points,(winner=1) AS wins FROM battles WHERE ranked=1 AND interrupted=0 AND end_datetime IS NOT NULL AND winner IS NOT NULL UNION ALL SELECT player2_uid,COALESCE(p2_points_won,0),(winner=2) FROM battles WHERE ranked=1 AND interrupted=0 AND end_datetime IS NOT NULL AND winner IS NOT NULL), totals AS (SELECT uid,SUM(points) AS points,SUM(wins) AS wins FROM scores GROUP BY uid), eligible AS (SELECT a.uid,a.username,a.display_name,MIN(9999,MAX(0,COALESCE(t.points,0))) AS points,COALESCE(t.wins,0) AS wins FROM accounts a LEFT JOIN totals t ON t.uid=a.uid WHERE a.active=1)`;
        const entries = await this.rows(cte + ' SELECT * FROM eligible WHERE points>0 OR wins>0 ORDER BY points DESC,username ASC,uid ASC LIMIT ? OFFSET ?', limit, offset);
        for (let i = 0; i < entries.length; i++) {
            const e = entries[i];
            const char = await this.first(`SELECT CAST(j.value AS INTEGER) AS character,COUNT(*) AS uses FROM battles b,
                json_each(CASE WHEN player1_uid=? AND winner=1 THEN json_array(p1c1,p1c2,p1c3,p1c4,p1c5)
                ELSE json_array(p2c1,p2c2,p2c3,p2c4,p2c5) END) j
                WHERE interrupted=0 AND end_datetime IS NOT NULL AND ((player1_uid=? AND winner=1) OR (player2_uid=? AND winner=2))
                AND j.type='integer' GROUP BY j.value ORDER BY uses DESC,character ASC LIMIT 1`, e.uid, e.uid, e.uid);
            e.rank = offset + i + 1; e.character = char?.character ?? -1;
        }
        const total = (await this.first(cte + ' SELECT COUNT(*) AS n FROM eligible WHERE points>0 OR wins>0')).n;
        return {offset, total, entries};
    }
    async totals(wire) {
        const battle = await this.first('SELECT b.* FROM battles b JOIN battle_sessions s USING(uid) WHERE wire_uid=?', wire);
        if (!battle) return {};
        const out = {stats_ranked: battle.ranked};
        const score = await this.first('SELECT * FROM rank_battle_scoring WHERE battle_uid=?', battle.uid);
        if (score) Object.assign(out, {rank_p1_start: score.p1_start, rank_p2_start: score.p2_start, rank_tolerance: score.tolerance});
        for (const side of [1, 2]) {
            const uid = battle[`player${side}_uid`];
            out[`p${side}_rank_points`] = await this.points(uid);
            for (const ranked of [0, 1]) {
                const row = await this.first('SELECT COUNT(*) AS battles,COALESCE(SUM((player1_uid=? AND winner=1) OR (player2_uid=? AND winner=2)),0) AS wins,COALESCE(SUM((player1_uid=? AND winner=2) OR (player2_uid=? AND winner=1)),0) AS losses FROM battles WHERE (player1_uid=? OR player2_uid=?) AND ranked=? AND end_datetime IS NOT NULL AND interrupted=0 AND winner IS NOT NULL', uid, uid, uid, uid, uid, uid, ranked);
                for (const field of ['battles', 'wins', 'losses']) out[`p${side}_${ranked ? 'ranked' : 'unranked'}_${field}`] = row[field];
            }
        }
        return out;
    }
    async row(wire) {
        const row = await this.first('SELECT b.*,s.wire_uid,s.p1_connection,s.p2_connection FROM battles b JOIN battle_sessions s USING(uid) WHERE wire_uid=?', wire);
        if (!row) return null;
        row.db_uid = row.uid; row.uid = row.wire_uid; delete row.wire_uid; row.items = {};
        for (const slot of slots) if (row[slot + '_items_uid'] !== null) {
            const items = await this.first('SELECT item1,item2,item3,item4,item5,item6,item7 FROM character_items WHERE uid=?', row[slot + '_items_uid']);
            row.items[slot] = Array.from({length: 7}, (_, i) => items['item' + (i + 1)]);
        }
        for (const field of ['p1_connection', 'p2_connection']) if (row[field] !== null) row[field] = JSON.parse(row[field]);
        for (const field of ['start_datetime', 'end_datetime']) if (row[field] !== null) row[field] = londonTimestamp(row[field]);
        for (const [field, values] of [['battle_type', types], ['winner', winners], ['finish_type', finishes]])
            if (row[field] !== null) row[field] = Object.keys(values).find(k => values[k] === row[field]);
        if (row.ranked) {
            const reports = await this.rows('SELECT result FROM battle_result_reports WHERE battle_uid=? ORDER BY side', row.db_uid);
            row.result_status = row.winner !== null && !row.interrupted ? 'confirmed' :
                reports.length === 2 && reports[0].result !== reports[1].result ? 'disputed' : 'pending';
        }
        return row;
    }
    async roomOwner(room, user, key, joining = false) {
        const owners = await this.first('SELECT ar.*,a.active FROM account_rooms ar JOIN accounts a ON a.uid=ar.host_uid WHERE code=?', room.code);
        if (!owners || !owners.active) throw new ApiError('host account is inactive', 403);
        if (joining) {
            if (owners.host_uid === user.uid) throw new ApiError('cannot match your own account', 409);
        } else {
            const host = secretEqual(key, room.host_key), join = room.join_key && secretEqual(key, room.join_key);
            if ((!host && !join) || owners[host ? 'host_uid' : 'join_uid'] !== user.uid) throw new ApiError('unauthorized account', 403);
            if (owners.join_uid !== null && !(await this.first('SELECT uid FROM accounts WHERE uid=? AND active=1', owners.join_uid))) throw new ApiError('participant account is inactive', 403);
        }
        return owners;
    }
    async matchWrites(room, user, key, now) {
        const owners = await this.roomOwner(room, user, null, true), wire = crypto.randomUUID();
        const uid = await this.nextId('battles');
        return {wire, writes: [
            this.stmt('UPDATE account_rooms SET join_uid=? WHERE code=?', user.uid, room.code),
            this.stmt('INSERT INTO battles(uid,start_datetime,player1,player2,player1_uid,player2_uid,battle_type,ranked) VALUES(?,?,(SELECT username FROM accounts WHERE uid=?),?,?,?, ?,?)', uid, now, owners.host_uid, user.username, owners.host_uid, user.uid, types[room.queue === 'RS' ? 'S' : room.queue || 'unknown'], Number(room.queue === 'RS')),
            this.stmt('INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(?,?,?,?,?,?)', uid, wire, await hash(room.host_key), await hash(key), now / 1000, 'matched')
        ]};
    }
    async battle(action, body, user, now) {
        if (typeof body.key !== 'string' || !body.key) throw new ApiError('unauthorized', 403);
        const hashed = await hash(body.key), wire = body.uid;
        if (typeof wire !== 'string') throw new ApiError('bad request');
        const session = await this.first('SELECT s.*,b.player1_uid,b.player2_uid FROM battle_sessions s JOIN battles b USING(uid) WHERE wire_uid=?', wire);
        const host = session && secretEqual(session.host_hash, hashed);
        if (action === 'start') return this.start(body, user, now, hashed, session);
        if (!['end', 'get', 'interrupt', 'connection', 'save'].includes(action)) throw new ApiError('bad request', 404);
        if (!session) throw new ApiError('no battle', 404);
        if ((!host && !secretEqual(session.join_hash, hashed)) || session[host ? 'player1_uid' : 'player2_uid'] !== user.uid) throw new ApiError('unauthorized account', 403);
        const row = await this.row(wire), writes = [];
        // Retained sessions remain readable, but stop accepting mutations after 30 days.
        if (action !== 'get' && now / 1000 - session.started > 30 * 86400) throw new ApiError('battle reporting expired', 403);
        if (action === 'save' || action === 'connection') {
            const field = `p${host ? 1 : 2}_classic_mode`;
            if (body.classic_mode !== undefined && body.classic_mode !== null) {
                if (typeof body.classic_mode !== 'boolean') throw new ApiError('invalid classic mode');
                writes.push(this.stmt(`UPDATE battles SET ${field}=? WHERE uid=? AND ${field} IS NULL`, Number(body.classic_mode), session.uid));
            }
            if (action === 'save') {
                await this.commit(writes);
                return {uid: wire};
            }
        }
        if (action === 'connection') {
            validateConnection(body.connection);
            writes.push(this.stmt(`UPDATE battle_sessions SET p${host ? 1 : 2}_connection=? WHERE uid=?`, JSON.stringify(body.connection), session.uid));
            await this.commit(writes);
            return {uid: wire, player: host ? 1 : 2, connection: body.connection};
        }
        if (action === 'interrupt') {
            if (row.end_datetime === null) writes.push(this.stmt('UPDATE battles SET end_datetime=?,interrupted=1,battle_duration=? WHERE uid=? AND end_datetime IS NULL', now, Math.round(Math.max(0, now / 1000 - session.started) * 1000) / 1000, session.uid));
        } else if (action === 'end') {
            const finish = body.finish_type ?? 'ok';
            if (!own(finishes, finish)) throw new ApiError('finish_type must be ok, perfect, timeout or ringout');
            if (!own(winners, body.winner)) throw new ApiError('winner is required');
            const duration = body.battle_duration ?? Math.round(Math.max(0, now / 1000 - session.started) * 1000) / 1000;
            if (typeof duration !== 'number' || !Number.isFinite(duration) || duration < 0 || duration > 86400) throw new ApiError('battle_duration must be seconds between zero and 86400');
            if (!row.ranked && !host) throw new ApiError('only the host reports unranked results', 403);
            let points = [0, 0];
            if (row.ranked) {
                if (session.player1_uid === null || session.player2_uid === null || session.player1_uid === session.player2_uid ||
                    (await this.first('SELECT COUNT(*) AS n FROM accounts WHERE uid IN (?,?) AND active=1', session.player1_uid, session.player2_uid)).n !== 2)
                    throw new ApiError('ranked results require two active participant accounts', 403);
                const side = host ? 1 : 2;
                const normal = [body.p1_points_won, body.p2_points_won];
                if (!normal.every(p => validInt(p, -2147483648, 2147483647)))
                    throw new ApiError('signed integer base point changes are required');
                // Duration is telemetry, not a scoring input; peers may observe different clocks.
                const result = JSON.stringify({winner: body.winner, finish, points: normal});
                const reports = await this.rows('SELECT side,result FROM battle_result_reports WHERE battle_uid=?', session.uid);
                const prior = reports.find(r => r.side === side);
                if (prior && prior.result !== result) throw new ApiError('result report cannot be changed', 409);
                const other = reports.find(r => r.side !== side);
                if (!prior) writes.push(this.stmt('INSERT INTO battle_result_reports VALUES(?,?,?)', session.uid, side, result));
                if (!other || other.result !== result) {
                    await this.commit(writes);
                    return {battle: await this.row(wire), result_status: other ? 'disputed' : 'pending', ...await this.totals(wire)};
                }
                // Both peers must agree on the original performance-based amounts.
                // Only the server applies rank scaling and balance caps.
                let score = await this.first('SELECT * FROM rank_battle_scoring WHERE battle_uid=?', session.uid);
                if (row.end_datetime === null || row.interrupted) score = await this.freeze({...row, uid: session.uid}, now, writes);
                if (!score) throw new ApiError('ranked scoring snapshot missing', 503);
                points = adjustedPoints([score.p1_start, score.p2_start], body.winner, normal, score.tolerance);
            }
            if (row.end_datetime !== null && !row.interrupted) {
                if (row.winner !== body.winner || row.p1_points_won !== points[0] || row.p2_points_won !== points[1] || row.finish_type !== finish) throw new ApiError('battle already ended with different data', 409);
            } else writes.push(this.stmt('UPDATE battles SET end_datetime=?,winner=?,p1_points_won=?,p2_points_won=?,battle_duration=?,finish_type=?,interrupted=0 WHERE uid=?', now, winners[body.winner], ...points, duration, finishes[finish], session.uid));
        }
        await this.commit(writes);
        return {battle: await this.row(wire), ...await this.totals(wire)};
    }
    async start(body, user, now, hashed, previous) {
        if (!/^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/.test(body.uid)) throw new ApiError('uid must be a canonical UUID');
        const meta = validateStart(body);
        let origin;
        if (own(body, 'session_uid')) {
            origin = await this.first('SELECT b.*,s.host_hash,s.join_hash,s.started FROM battles b JOIN battle_sessions s USING(uid) WHERE wire_uid=?', body.session_uid);
        } else if (body.code) {
            const room = await this.first('SELECT * FROM rooms WHERE code=? AND made>=?', String(body.code).toUpperCase(), now / 1000 - 600);
            if (!room || !secretEqual(body.key, room.host_key) || !room.join_key) throw new ApiError('battle needs two players', 409);
            const owners = await this.roomOwner(room, user, body.key);
            origin = {player1: (await this.first('SELECT username FROM accounts WHERE uid=?', owners.host_uid)).username,
                player2: (await this.first('SELECT username FROM accounts WHERE uid=?', owners.join_uid)).username,
                player1_uid: owners.host_uid, player2_uid: owners.join_uid, ranked: Number(room.queue === 'RS'), host_hash: hashed, join_hash: await hash(room.join_key), started: room.made};
        } else if (previous) origin = await this.first('SELECT b.*,s.host_hash,s.join_hash,s.started FROM battles b JOIN battle_sessions s USING(uid) WHERE wire_uid=?', body.uid);
        if (!origin || !secretEqual(origin.host_hash, hashed) || origin.player1_uid !== user.uid) throw new ApiError('unauthorized account', 403);
        if (now / 1000 - origin.started > 30 * 86400) throw new ApiError('battle reporting expired', 403);
        if (!(await this.first('SELECT uid FROM accounts WHERE uid=? AND active=1', origin.player2_uid))) throw new ApiError('participant account is inactive', 403);
        if (origin.ranked && body.battle_type !== 'S') throw new ApiError('ranked battles require single battle');
        if (previous) {
            if (!secretEqual(previous.host_hash, hashed) || previous.player1_uid !== user.uid) throw new ApiError('unauthorized account', 403);
            if (previous.request_id !== 'matched' || (await this.first('SELECT end_datetime FROM battles WHERE uid=?', previous.uid)).end_datetime !== null) {
                const old = await this.row(body.uid);
                if (old.battle_type !== body.battle_type || slots.some(s => old[s] !== (body[s] ?? null)) || Object.keys(meta.values).some(k => old[k] !== meta.values[k]) || JSON.stringify(old.items) !== JSON.stringify(meta.items)) throw new ApiError('battle already started with different data', 409);
                return {uid: body.uid, start_datetime: old.start_datetime, ...await this.totals(body.uid)};
            }
        }
        const count = await this.first('SELECT COUNT(*) AS n FROM battle_sessions WHERE host_hash=?', hashed);
        if (count.n >= 1000 && !previous) throw new ApiError('session rematch limit reached', 429);
        const pending = await this.first("SELECT b.uid FROM battles b JOIN battle_sessions s USING(uid) WHERE s.host_hash=? AND s.request_id='matched' AND b.end_datetime IS NULL", hashed);
        const uid = pending?.uid ?? await this.nextId('battles');
        const values = {start_datetime: now, player1: origin.player1, player2: origin.player2, player1_uid: origin.player1_uid, player2_uid: origin.player2_uid,
            battle_type: types[body.battle_type], ranked: origin.ranked, ...meta.values, ...Object.fromEntries(slots.map(s => [s, body[s] ?? null]))};
        const writes = [];
        if (pending) {
            writes.push(this.stmt('DELETE FROM battle_sessions WHERE uid=?', uid));
            writes.push(this.stmt(`UPDATE battles SET ${Object.keys(values).map(k => k + '=?').join(',')} WHERE uid=?`, ...Object.values(values), uid));
        } else writes.push(this.stmt(`INSERT INTO battles(uid,${Object.keys(values).join(',')}) VALUES(${Array(Object.keys(values).length + 1).fill('?').join(',')})`, uid, ...Object.values(values)));
        let itemId = Object.keys(meta.items).length ? await this.nextId('character_items') : 0;
        for (const [slot, items] of Object.entries(meta.items)) {
            writes.push(this.stmt('INSERT INTO character_items(uid,item1,item2,item3,item4,item5,item6,item7) VALUES(?,?,?,?,?,?,?,?)', itemId, ...items));
            writes.push(this.stmt(`UPDATE battles SET ${slot}_items_uid=? WHERE uid=?`, itemId++, uid));
        }
        writes.push(this.stmt('INSERT INTO battle_sessions(uid,wire_uid,host_hash,join_hash,started,request_id) VALUES(?,?,?,?,?,?)', uid, body.uid, hashed, origin.join_hash, now / 1000, body.uid));
        await this.freeze({...values, uid}, now, writes);
        await this.commit(writes);
        return {uid: body.uid, start_datetime: londonTimestamp(now), ...await this.totals(body.uid)};
    }
}
function validateStart(body) {
    if (!own(types, body.battle_type) || body.battle_type === 'unknown') throw new ApiError('battle_type must be S, T, D10, D15 or D20');
    const values = {}, items = {};
    for (const field of ['stage', 'music', 'stage_music_picker', ...slots.map(s => s + '_costume')]) {
        const value = body[field] ?? null;
        if (field === 'stage_music_picker') {
            if (value !== null && value !== 1 && value !== 2) throw new ApiError('invalid stage/music picker');
        } else if (value !== null && (!validInt(value, field === 'music' ? -1 : 0, 65535) ||
                (field.endsWith('_costume') && body[field.slice(0, -8)] == null))) throw new ApiError('invalid stage, music or costume');
        values[field] = value;
    }
    for (const side of [1, 2]) {
        let empty = false;
        for (let c = 1; c <= 5; c++) {
            const value = body[`p${side}c${c}`] ?? null;
            if (value === null) { if (c === 1) throw new ApiError('each player needs at least one character'); empty = true; }
            else if (!validInt(value, 0, 65535) || empty || (body.battle_type === 'S' && c > 1)) throw new ApiError('invalid character roster');
        }
    }
    const loadouts = body.items ?? {};
    if (!loadouts || typeof loadouts !== 'object' || Array.isArray(loadouts)) throw new ApiError('invalid character items');
    for (const slot of slots) if (own(loadouts, slot)) {
        const list = loadouts[slot];
        if (body[slot] == null || !Array.isArray(list) || list.length !== 7 || list.some(v => v !== null && !validInt(v, 0, 65535))) throw new ApiError('invalid character items');
        items[slot] = list.map(v => v || null);
    }
    if (Object.keys(loadouts).some(s => !slots.includes(s))) throw new ApiError('invalid character slot');
    return {values, items};
}
function validateConnection(v) {
    const counters = ['rtt_samples','duration_ms','input_packets_sent','input_packets_received','input_bytes_sent','input_bytes_received','wait_count','rollbacks','rollback_frames','input_delay_frames','rollback_window_frames','protocol_version'];
    const ping = ['avg_ping_ms','min_ping_ms','max_ping_ms','jitter_ms'];
    const fields = [...counters, ...ping, 'wait_ms', 'relayed'];
    if (!v || typeof v !== 'object' || Array.isArray(v) || Object.keys(v).some(f => !fields.includes(f) && !['save_mode', 'save_id', 'save_hash'].includes(f)) || fields.some(f => !own(v, f))) throw new ApiError('invalid connection summary fields');
    if (own(v, 'save_mode') && !['regular', 'zenkai'].includes(v.save_mode)) throw new ApiError('invalid save mode');
    if (own(v, 'save_id') !== own(v, 'save_hash')) throw new ApiError('save metadata must be a pair');
    if (own(v, 'save_id') && (v.save_mode !== 'zenkai' || typeof v.save_id !== 'string' || !/^[0-9a-f]{32}$/.test(v.save_id) ||
        typeof v.save_hash !== 'string' || !/^[0-9a-f]{64}$/.test(v.save_hash))) throw new ApiError('invalid save metadata');
    if (counters.some(f => !validInt(v[f], 0, Number.MAX_SAFE_INTEGER)) || v.input_delay_frames > 30 || v.rollback_window_frames > 30) throw new ApiError('invalid connection counter');
    for (const f of [...ping, 'wait_ms']) {
        if (ping.includes(f) && v[f] === null) continue;
        if (typeof v[f] !== 'number' || !Number.isFinite(v[f]) || v[f] < 0 || v[f] > Number.MAX_SAFE_INTEGER) throw new ApiError('invalid connection measurement');
    }
    if (typeof v.relayed !== 'boolean') throw new ApiError('invalid relay flag');
    if (v.rtt_samples === 0) { if (ping.some(f => v[f] !== null)) throw new ApiError('ping requires samples'); }
    else if (ping.filter(f => f !== 'jitter_ms').some(f => v[f] === null) || !(v.min_ping_ms <= v.avg_ping_ms && v.avg_ping_ms <= v.max_ping_ms && v.max_ping_ms < 5000)) throw new ApiError('invalid ping range');
    if ((v.rtt_samples < 2) !== (v.jitter_ms === null)) throw new ApiError('invalid jitter sample count');
}
