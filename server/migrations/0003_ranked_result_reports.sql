-- Immutable participant reports; only matching reports settle ranked battles.
CREATE TABLE IF NOT EXISTS battle_result_reports (
    battle_uid INTEGER NOT NULL REFERENCES battles(uid) ON DELETE CASCADE,
    side INTEGER NOT NULL CHECK(side IN (1,2)),
    result TEXT NOT NULL,
    PRIMARY KEY(battle_uid,side)
);
