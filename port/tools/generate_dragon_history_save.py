"""Create a fresh Zenkai v3 save with all 48 Dragon History episodes available.

Episodes are unlocked, not cleared. No rewards or Zeni are granted. Uses the
current ten-character Zenkai starting roster and keeps Classic Mode available.
Only Python's standard library is required; existing saves are never read/edited.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from generate_zenkai_save import (
    fresh_save, save_checksum, seal_save, verify_envelope, write_new_file,
)

# Native submenu order, verified against HistResult_IsMenuCleared in menu_c_c.c.
# Special is submenu 4; the ninth SaveSlot is not a Dragon History saga.
EPISODE_COUNTS = (3, 4, 7, 5, 16, 5, 4, 4)
STARTING_CHARACTERS = (0, 13, 22, 25, 26, 27, 28, 60, 66, 67)
STORY_STAGES = (2, 3, 6, 7, 10, 11, 13, 14, 16, 28, 33, 34)
PACK_OFFSET = 0x3EF0


def dragon_history_save(common_file4: Path | None = None) -> tuple[bytearray, int]:
    save, items = fresh_save(common_file4)
    bits = sum(1 << chara for chara in STARTING_CHARACTERS)
    struct.pack_into('<QQQ', save, 0xC10,
                     *(bits >> (64 * word) & ((1 << 64) - 1) for word in range(3)))
    # SPK v1, no earned-Zeni or pack-disabled flag, three reserved zero bytes.
    save[PACK_OFFSET:PACK_OFFSET + 8] = b'SPK\x01\0\0\0\0'
    stages = struct.unpack_from('<Q', save, 0xC28)[0]
    stages &= ~sum(1 << stage for stage in STORY_STAGES)
    struct.pack_into('<Q', save, 0xC28, stages)
    for saga, count in enumerate(EPISODE_COUNTS):
        offset = 0x10 + saga * 0x10
        mask = (1 << count) - 1
        # Saga available/new, all episode unlock/new bits, no completion bits.
        struct.pack_into('<4I', save, offset, 3, mask, 0, mask)
    checksum = save_checksum(save)
    struct.pack_into('<II', save, 0, checksum >> 8, checksum & 0xFF)
    return save, items


def main() -> int:
    repository = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=repository / 'port/build/generated-saves/dragon-history/zenkaipower.sav',
                        help='destination (default: port/build/generated-saves/dragon-history/zenkaipower.sav)')
    parser.add_argument('--common-file4', type=Path,
                        help="unpacked common file 4, used to grant the game's normal starter items")
    parser.add_argument('--force', action='store_true', help='replace an existing destination')
    args = parser.parse_args()
    try:
        save, items = dragon_history_save(args.common_file4)
        envelope = seal_save(bytes(save))
        if verify_envelope(envelope, check_starter_roster=False) != bytes(save):
            raise ValueError('Generated save did not round-trip')
        write_new_file(args.output, envelope, args.force)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f'Created {args.output} ({len(envelope)} bytes)')
    print(f'Dragon History: {sum(EPISODE_COUNTS)} episodes unlocked; none cleared')
    print('Characters: 10 starting entries. Zeni: 0. Zenkai pack: enabled.')
    print(f'Starter items: {items}' if args.common_file4 else
          'Starter items: not initialized (provide --common-file4 for normal starter items)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
