-- Preserve each participant's reported release when attached to a battle.
ALTER TABLE battles ADD COLUMN p1_game_version INTEGER CHECK(p1_game_version BETWEEN 0 AND 2147483647);
ALTER TABLE battles ADD COLUMN p2_game_version INTEGER CHECK(p2_game_version BETWEEN 0 AND 2147483647);

CREATE TRIGGER IF NOT EXISTS battle_game_versions_insert AFTER INSERT ON battles
BEGIN
    UPDATE battles SET
        p1_game_version=(SELECT game_version FROM accounts WHERE uid=NEW.player1_uid),
        p2_game_version=(SELECT game_version FROM accounts WHERE uid=NEW.player2_uid)
    WHERE uid=NEW.uid;
END;

CREATE TRIGGER IF NOT EXISTS battle_game_versions_players AFTER UPDATE OF player1_uid,player2_uid ON battles
WHEN (OLD.player1_uid IS NULL AND NEW.player1_uid IS NOT NULL)
  OR (OLD.player2_uid IS NULL AND NEW.player2_uid IS NOT NULL)
BEGIN
    UPDATE battles SET
        p1_game_version=CASE WHEN OLD.player1_uid IS NULL THEN (SELECT game_version FROM accounts WHERE uid=NEW.player1_uid) ELSE p1_game_version END,
        p2_game_version=CASE WHEN OLD.player2_uid IS NULL THEN (SELECT game_version FROM accounts WHERE uid=NEW.player2_uid) ELSE p2_game_version END
    WHERE uid=NEW.uid;
END;
