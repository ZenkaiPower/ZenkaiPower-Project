-- Existing battles used original rules; only two affirmative save reports enable changes.
ALTER TABLE battles ADD COLUMN battle_nature INTEGER NOT NULL DEFAULT 0 CHECK(battle_nature IN (0,1));
ALTER TABLE battles ADD COLUMN p1_sparking_changes INTEGER CHECK(p1_sparking_changes IN (0,1));
ALTER TABLE battles ADD COLUMN p2_sparking_changes INTEGER CHECK(p2_sparking_changes IN (0,1));
