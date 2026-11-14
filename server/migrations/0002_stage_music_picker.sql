ALTER TABLE battles ADD COLUMN stage_music_picker INTEGER CHECK(stage_music_picker IN (1,2));
