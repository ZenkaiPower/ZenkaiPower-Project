# Ranked points

Ranked Single (`RS`) uses the original performance-based score from
`UbScore_OnlinePoints`: remaining health, hits, damage, and battle-event bonuses.
The winner's base gain is their score; the loser's base change is their score
minus the winner's score. The game converts these hundredths to displayed points
before reporting them, using the original positive ceiling/negative truncation.
A shared
multiplier reduces both when the winner has many more
rank points than the loser. Similar opponents and upsets keep the full base
amount. Non-ranked battles continue to record zero points.

## Result settlement and trust

Both participants submit `battle/end` using their own account token and room key.
Ranked rewards require two distinct active participant accounts and matching
winner, finish type, and both unadjusted base point amounts. Point amounts must
be signed 32-bit integers. The first report from each side is immutable. One report
is `pending`; conflicting reports are `disputed`; neither changes balances or
records a ranked win/loss. Matching reports settle once, including ties (zero
points). Repeated identical reports cannot award points again. Each peer's
duration is validated as telemetry, but durations need not match.

`battle_result_reports` persists reports across restarts. `battle.result_status`
is `pending`, `disputed`, or `confirmed`. Both Cloudflare and Python implement the
same rule. The result screen holds balances unchanged until the server confirms
settlement. A disconnect alone does not award a ranked win.

This prevents unilateral forged results and point amounts that the other peer
does not confirm. The server does not independently verify performance. Two
colluding accounts can still submit invented matching results or farm wins.
Build hashes and embedded HMAC keys do not establish honest execution; they are
compatibility filters. Strong anti-cheat requires trusted simulation or replay
validation, plus abuse controls for colluding accounts.

Apply Cloudflare migration `0003_ranked_result_reports.sql` before deploying the
Worker, and rebuild/update both clients. The Python server creates the table at
startup. Older clients submit only the host report, so their new ranked results
remain pending. Existing completed battles and balances remain unchanged.

## Population-based tolerance

Once per UTC calendar day, on the first ranked battle start, the server collects
accounts that are active and have completed at least one uninterrupted ranked
battle in the preceding 30 days. Each account counts once, including accounts
used by bots. Unlinked historical names are not treated as accounts. The sample
contains each eligible account's current lifetime rank-point balance, not the
points earned in just those 30 days.

With at least 20 eligible accounts:

```text
Q25 = 25th percentile of account balances
Q75 = 75th percentile of account balances
spread = Q75 - Q25

tolerance = clamp(roundHalfUp(spread * 0.10), 100, 500)
```

Percentiles use linear interpolation at position `(count - 1) * percentile` in
the sorted balances. With fewer than 20 accounts, tolerance is **250**. The policy
is cached for the remainder of the UTC day; it is recalculated lazily on the
first ranked start on a later day. Restarts preserve the cached policy.

Examples:

| Q25 | Q75 | Spread | Tolerance |
| ---: | ---: | ---: | ---: |
| 800 | 1,800 | 1,000 | 100 |
| 1,000 | 3,500 | 2,500 | 250 |
| 1,000 | 7,000 | 6,000 | 500, capped |

## Battle calculation

The server freezes both starting balances and the day's tolerance at each
ranked battle start. Each rematch gets a fresh snapshot. Subsequent point or
policy changes do not change the calculation for that battle.

```text
gap = max(0, winnerStartPoints - loserStartPoints - tolerance)
multiplier = 1000 / (1000 + gap)

winnerGain = max(1, roundHalfUp(abs(normalWinnerChange) * multiplier))
loserLoss  = max(1, roundHalfUp(abs(normalLoserChange)  * multiplier))

winnerGain = min(winnerGain, 9999 - winnerStartPoints)
loserLoss  = min(loserLoss, loserStartPoints)
```

The signed changes saved are `+winnerGain` and `-loserLoss`. A tie saves `(0, 0)`.
The performance-based calculation runs before applying balance caps and rank scaling:
first scale the normal amounts, then cap against the frozen available balances.
A loser at zero loses zero; a winner at the 9,999 cap gains zero. Other amounts
are at least one. A zero base amount also becomes one under the existing minimum rule.

Both implementations use the following integer expression for rounding, so the
client preview and saved result agree even at exact halves:

```text
denominator = 1000 + gap
roundedMagnitude = (abs(base) * 1000 + denominator / 2) / denominator
```

Division above is integer division. Base changes are whole displayed performance
points agreed by both peers, not fixed win/loss rewards. Ties award `(0, 0)`.

For base changes **+100 / -60**, with tolerance **250**:

| Winner starts | Loser starts | Winner gains | Loser loses |
| ---: | ---: | ---: | ---: |
| 2,000 | 2,000 | 100 | 60 |
| 2,200 | 2,000 | 100 | 60 |
| 3,000 | 2,000 | 57 | 34 |
| 5,000 | 2,000 | 27 | 16 |
| 9,000 | 1,000 | 11 | 7 |
| 2,000 | 5,000 | 100 | 60 |
| 1,000 | 10 | 57 | 10 |
| 1,000 | 0 | 57 | 0 |

## Storage and client behavior

- `rank_daily_policy`: UTC day number, tolerance, and eligible population count.
- `rank_battle_scoring`: battle UID, both starting balances, tolerance, and policy
  day. The snapshot survives retries and server restarts.
- `battles.p1_points_won` / `p2_points_won`: final adjusted, signed point changes.

Battle API responses expose `rank_p1_start`, `rank_p2_start`, and `rank_tolerance`
with their normal account totals. The server applies the formula only after
matching participant reports, and compares the resulting values on retries.
The result screen refreshes its totals and arrows from the server-confirmed
values. Interruptions award no new points.

Existing completed battles keep their recorded ranked points. An older in-flight
ranked session without a snapshot freezes its inputs on matching end reports;
new sessions freeze at start. Account balances remain clamped to 0–9,999. This
policy is also implemented by the hosted Worker backend.

Rebuild both clients and update the server together when restoring performance
scoring. Reports from the previous fixed-reward policy do not match the new
report format; finish or leave those sessions before updating. No new database
migration is needed beyond the existing result-report migration.

## Implementation and checks

The pure server formula is in `server/src/rank_scoring.py`; policy caching,
snapshots, and result storage are in `lan_server.py`. The client preview and
server-result cache are in `port/src/gs/net_match.c`; the result plate is drawn in
`src/menu/port_tbo.c`.

```sh
python3 -m unittest discover -s server/tests -p test_result_security.py
# Node 24+, after npm ci in server:
node --test port/tests/matchmaking/service_security.mjs
```

Tests cover variable performance rewards, invalid and conflicting point reports,
missing/conflicting results, immutable retries,
participant ownership, inactive accounts, persisted reports, rank scaling, balance
limits, ties, and independence from compatibility hashes. Client reporting is
also exercised by `python3 port/tests/matchmaking/result_client.py` with a C
compiler and sanitizers. Restart the Python server and both game clients after
updating; the server creates the new tables on startup.
