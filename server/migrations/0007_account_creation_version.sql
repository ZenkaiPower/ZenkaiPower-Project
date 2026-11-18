-- Keep the release used to create an account, separately from its latest client.
ALTER TABLE accounts ADD COLUMN created_game_version INTEGER CHECK(created_game_version BETWEEN 0 AND 2147483647);
