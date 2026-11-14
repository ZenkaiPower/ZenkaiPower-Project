"""Generate a fresh ZENKAI save with the original 132-character starter roster unlocked.

Writes unencrypted v3 saves using only Python's standard library.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import struct
import tempfile
from pathlib import Path


SAVE_SIZE = 0x4000
HEADER_SIZE = 88
SAVE_MAGIC = b"ZENKAI\x00\x03"
CHARACTER_COUNT = 161
STARTER_UNLOCKED_COUNT = 132
STARTER_LOCKED_CHARACTERS = {
    7, 8, 9, 10, 0x18, 0x26, 0x36, 0x45, 0x46, 0x47, 0x48,
    0x4B, 0x4C, 0x61, 0x6E, 0x78, 0x91, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0,
}
ITEM_COUNT = 350
ITEM_TABLE_ENTRY_SIZE = 0x28
ITEM_TABLE_FLAGS_OFFSET = 0x14


def fresh_save(common_file4: Path | None) -> tuple[bytearray, int]:
    save = bytearray(SAVE_SIZE)

    struct.pack_into("<I", save, 0x0C, 1)
    struct.pack_into("<I", save, 0x10, 3)
    struct.pack_into("<I", save, 0x14, 1)
    struct.pack_into("<I", save, 0x1C, 1)

    character_bits = sum(
        1 << character
        for character in range(CHARACTER_COUNT)
        if character not in STARTER_LOCKED_CHARACTERS
    )
    struct.pack_into("<QQQ", save, 0x0C10, *(character_bits >> (64 * i) & ((1 << 64) - 1) for i in range(3)))

    locked_stages = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x1F, 0x20}
    stage_bits = sum(1 << stage for stage in range(35) if stage not in locked_stages)
    struct.pack_into("<Q", save, 0x0C28, stage_bits)
    struct.pack_into("<I", save, 0x0C30, (1 << 20) - 1)
    struct.pack_into("<6i", save, 0x0C34, 3, 2, 2, 0, 0, 0)
    struct.pack_into("<i", save, 0x020C, 5)
    struct.pack_into("<i", save, 0x077C, 99)

    for record in range(14):
        struct.pack_into("<i", save, 0x2D40 + record * 0x1C + 0x18, -1)

    default_keys = (2, 1, 0, 3, 4, 5, 6, 7)
    for base in (0x160C, 0x164C):
        for pad in range(2):
            struct.pack_into("<8i", save, base + pad * 0x20, *default_keys)
    struct.pack_into("<i", save, 0x1608, 6)
    struct.pack_into("<i", save, 0x16A8, 9)
    struct.pack_into("<i", save, 0x16AC, 9)

    initial_item_count = 0
    if common_file4 is not None:
        archive = common_file4.read_bytes()
        if len(archive) < 12:
            raise ValueError(f"{common_file4} is too short to contain the common-file-4 header")
        item_offset = struct.unpack_from("<I", archive, 8)[0] & ~3
        table_end = item_offset + ITEM_COUNT * ITEM_TABLE_ENTRY_SIZE
        if table_end > len(archive):
            raise ValueError(f"{common_file4} does not contain the complete item table")
        for item in range(ITEM_COUNT):
            entry = item_offset + item * ITEM_TABLE_ENTRY_SIZE
            flags = struct.unpack_from("<i", archive, entry + ITEM_TABLE_FLAGS_OFFSET)[0]
            if flags & 2:
                save[0x2EC8 + item] = 3
                initial_item_count += 1

    checksum = save_checksum(save)
    struct.pack_into("<II", save, 0, checksum >> 8, checksum & 0xFF)
    return save, initial_item_count


def save_checksum(save: bytes | bytearray) -> int:
    checksum = 0
    for index, value in enumerate(save[8:]):
        checksum = (checksum + (value << 8 if index & 1 else value)) & 0xFFFF
    return checksum


def seal_save(plaintext: bytes) -> bytes:
    save_id = os.urandom(16)
    digest = hashlib.blake2b(plaintext, digest_size=32).digest()
    header = SAVE_MAGIC + save_id + digest
    checksum = hashlib.blake2b(header + plaintext, digest_size=32).digest()
    return header + checksum + plaintext


def verify_envelope(envelope: bytes) -> bytes:
    if len(envelope) != HEADER_SIZE + SAVE_SIZE or envelope[:8] != SAVE_MAGIC:
        raise ValueError("Generated save has an invalid size or version header")
    plaintext = envelope[HEADER_SIZE:]
    if hashlib.blake2b(envelope[:56] + plaintext, digest_size=32).digest() != envelope[56:HEADER_SIZE]:
        raise ValueError("Generated save envelope checksum does not match")
    if hashlib.blake2b(plaintext, digest_size=32).digest() != envelope[24:56]:
        raise ValueError("Generated save digest does not match its payload")
    stored_checksum = struct.unpack_from("<II", plaintext, 0)
    calculated_checksum = save_checksum(plaintext)
    if stored_checksum != (calculated_checksum >> 8, calculated_checksum & 0xFF):
        raise ValueError("Generated save has an invalid native checksum")
    character_bits = sum(
        1 << character
        for character in range(CHARACTER_COUNT)
        if character not in STARTER_LOCKED_CHARACTERS
    )
    expected_character_words = tuple(
        character_bits >> (64 * word) & ((1 << 64) - 1) for word in range(3)
    )
    if struct.unpack_from("<QQQ", plaintext, 0x0C10) != expected_character_words:
        raise ValueError("Generated save does not match the original starter roster")
    return plaintext


def write_new_file(path: Path, data: bytes, force: bool) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and not force:
        raise FileExistsError(f"{path} already exists; pass --force to replace it")

    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=path.name + ".", suffix=".part", delete=False) as file:
            temporary_path = Path(file.name)
            file.write(data)
            file.flush()
            os.fsync(file.fileno())
        if force:
            os.replace(temporary_path, path)
        else:
            os.link(temporary_path, path)
            temporary_path.unlink()
    finally:
        if temporary_path is not None and temporary_path.exists():
            temporary_path.unlink()


def main() -> int:
    repository = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=repository / "savegame" / "zenkaipower.sav",
        help="destination save path (default: savegame/zenkaipower.sav)",
    )
    parser.add_argument(
        "--common-file4",
        type=Path,
        help="unpacked common file 4, used to grant the game's default starter items",
    )
    parser.add_argument("--force", action="store_true", help="replace an existing destination")
    args = parser.parse_args()

    try:
        plaintext, item_count = fresh_save(args.common_file4)
        plaintext_bytes = bytes(plaintext)
        envelope = seal_save(plaintext_bytes)
        verified = verify_envelope(envelope)
        if verified != plaintext_bytes:
            raise ValueError("Generated save did not round-trip")
        write_new_file(args.output, envelope, args.force)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))

    print(f"Created {args.output} ({len(envelope)} bytes)")
    print(f"Unlocked characters: {STARTER_UNLOCKED_COUNT}")
    if args.common_file4 is None:
        print("Starter items: not initialized (provide --common-file4 to reproduce the game's item grants)")
    else:
        print(f"Starter items: {item_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
