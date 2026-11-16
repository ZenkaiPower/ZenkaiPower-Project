-- Latest client-reported release (rel / PORT_RELEASE); NULL for older clients.
ALTER TABLE accounts ADD COLUMN game_version INTEGER CHECK(game_version BETWEEN 0 AND 2147483647);
