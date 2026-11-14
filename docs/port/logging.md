# Release logs

Release builds create `logs` beside the game executable at startup:

- `zenkaipower.log`: startup, rendering, audio, game-data diagnostics, and crash reports.
- `dragonnetbattle.log`: matchmaking, relay, connection, rollback, and synchronization diagnostics.

Ordinary messages have UTC timestamps. Native crash reports are appended directly to
the game log, including the backtrace when available. Console output is preserved,
so the bot and setup can still read their existing messages.

Bot processes do not write these files. CPU-opponent announcements and successful
`battle/get` polling are omitted. Successful data verification is logged once per
distinct signature in a process. Failed requests, connection errors, and desync
reports are retained.

Both files retain the last seven days of entries. Cleanup runs at startup and,
while messages are being written, once an hour. The files are appended across
launches. Multiple game processes sharing an installation use file locks
when writing or cleaning logs. Logging failures do not prevent the game from running.

Development builds keep their normal console output without automatically creating
these files. Verify routing, retention, and concurrent writers with
`make -f port/release/tests.mk release-logging-test`.
