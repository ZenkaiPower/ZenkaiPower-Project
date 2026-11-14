Monocypher 4.0.2
================

Unmodified `src/monocypher.c`, `src/monocypher.h`, and `LICENCE.md` from
https://github.com/LoupVaillant/Monocypher/tree/4.0.2

Used by `port/src/zenkai_crypto.c` for XChaCha20-Poly1305 encrypted saves.
The implementation is included by that translation unit so all PC build
variants compile it without an external crypto-library dependency.
