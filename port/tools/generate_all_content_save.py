"""Create a Zenkai save with all episodes, characters, stages and battle modes available.

Unlocks all BGM, all 100 missions, Survival and the highest shop stock tier. Episodes and
missions remain uncleared; items remain purchasable, and no Zeni is granted.
Uses only Python's standard library. Does not modify an existing save.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from generate_zenkai_save import (
    fresh_save, save_checksum, seal_save, verify_envelope, write_new_file,
)

from generate_dragon_history_save import dragon_history_save, EPISODE_COUNTS
from generate_zenkai_save import CHARACTER_COUNT


def all_content_save(common_file4: Path | None = None) -> tuple[bytearray, int]:
    save, items = dragon_history_save(common_file4)
    bits = (1 << CHARACTER_COUNT) - 1
    struct.pack_into('<QQQ', save, 0xC10,
                     *(bits >> (64 * word) & ((1 << 64) - 1) for word in range(3)))
    struct.pack_into('<Q', save, 0xC28, (1 << 35) - 1)
    # Western tracks plus all five independent METEOR music task rewards.
    struct.pack_into('<I', save, 0xC30, ((1 << 20) - 1) | (0x1F << 26))
    # MisSel_Init reads missionPages at 0x20C: 20 pages of five missions.
    # UbMenu_Init uses bit 0 at 0x208 to open Survival (normally 30 mission wins).
    struct.pack_into('<II', save, 0x208, 1, 20)
    # Shop_BuildLists filters purchasable items by this tier (native maximum 5).
    # Leave item ownership untouched so shop items can still be purchased.
    struct.pack_into('<I', save, 0x100C, 5)
    checksum = save_checksum(save)
    struct.pack_into('<II', save, 0, checksum >> 8, checksum & 0xFF)
    return save, items


def main() -> int:
    repository = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path,
                        default=repository / 'port/build/generated-saves/all-content/zenkaipower.sav',
                        help='destination (default: port/build/generated-saves/all-content/zenkaipower.sav)')
    parser.add_argument('--common-file4', type=Path,
                        help="unpacked common file 4, used to grant the game's normal starter items")
    parser.add_argument('--force', action='store_true', help='replace an existing destination')
    args = parser.parse_args()
    try:
        save, items = all_content_save(args.common_file4)
        envelope = seal_save(bytes(save))
        if verify_envelope(envelope, check_starter_roster=False) != bytes(save):
            raise ValueError('Generated save did not round-trip')
        write_new_file(args.output, envelope, args.force)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f'Created {args.output} ({len(envelope)} bytes)')
    print(f'Dragon History: {sum(EPISODE_COUNTS)} episodes unlocked; none cleared')
    print('Characters: 161. Stages: 35 (all variations). Zeni: 0. Zenkai pack: enabled.')
    print('Mission 100: all 100 available. Survival: available. Shop: maximum stock tier.')
    print('BGM: all 20 Western tracks and all 24 METEOR tracks unlocked.')
    print(f'Starter items: {items}' if args.common_file4 else
          'Starter items: not initialized (provide --common-file4 for normal starter items)')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
