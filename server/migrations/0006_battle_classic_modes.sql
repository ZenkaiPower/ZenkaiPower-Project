-- Each participant reports their own save's Classic Mode for this battle.
ALTER TABLE battles ADD COLUMN p1_classic_mode INTEGER CHECK(p1_classic_mode IN (0, 1));
ALTER TABLE battles ADD COLUMN p2_classic_mode INTEGER CHECK(p2_classic_mode IN (0, 1));
