"""Count CPU level concentration in user-owned mode tables and story scripts."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct

from story_routes import GameFiles, decode_script
from make_fingerprint import bpe_decode


def section(data, index):
    count = struct.unpack_from('<I', data)[0]
    if not 1 <= index <= count:
        raise ValueError(f'Invalid section {index}/{count}')
    start, end = (struct.unpack_from('<I', data, i * 4)[0] & ~3
                  for i in (index, index + 1))
    if not 0 <= start <= end <= len(data):
        raise ValueError('Invalid section bounds')
    return data[start:end]


def integers(data, offset, count):
    return struct.unpack_from(f'<{count}i', data, offset)


def tapered_level(base):
    boost = 7 if base < 10 else 6 if base < 15 else 5 if base < 20 else 4 if base < 25 else 3 if base < 29 else 0
    return min(base + boost, 29)


def summarize(name, groups, offset=None, original_offset=0):
    bases = [b for group in groups for b in group]
    if not bases or min(bases) < 0 or max(bases) > 29:
        raise ValueError(f'Unexpected CPU base in {name}')
    convert = tapered_level if offset is None else lambda b: max(0, min(29, tapered_level(b) + offset))
    effective = [convert(b) for b in bases]
    original = [b + original_offset for b in bases]
    return dict(name=name, entries=len(bases), groups=len(groups),
                bases=dict(sorted(Counter(bases).items())),
                effective=dict(sorted(Counter(effective).items())),
                original_average=sum(original) / len(original),
                effective_average=sum(effective) / len(effective),
                original_mid=sum(20 <= n <= 24 for n in original),
                mid=sum(20 <= n <= 24 for n in effective),
                original_high=sum(25 <= n <= 29 for n in original),
                original_capped=sum(n == 29 for n in original),
                high=sum(n >= 25 for n in effective),
                capped=sum(n == 29 for n in effective),
                groups_all_high=sum(all(convert(b) >= 25 for b in g) for g in groups),
                groups_any_high=sum(any(convert(b) >= 25 for b in g) for g in groups))


def audit(root):
    files = GameFiles(root)
    fingerprints = {}

    def read(file_id):
        data = files.read(file_id)
        fingerprints[hex(file_id)] = hashlib.sha256(data).hexdigest()
        return data

    def table_groups(pack, rules_section, opponents_section, count, stride, start, slots):
        rules, opponents = section(pack, rules_section), section(pack, opponents_section)
        groups = []
        for row in range(count):
            group = []
            for index in integers(rules, row * stride + start, slots):
                if index == 999:
                    break
                chara, _, level = integers(opponents, index * 44, 3)
                if chara == 999:
                    break
                if not 0 <= chara <= 164 and chara != 998:
                    raise ValueError('Invalid opponent reference')
                group.append(level)
            if not group:
                raise ValueError('Empty battle/course')
            groups.append(group)
        return groups

    try:
        arc = read(0x1C4)
        raw = section(arc, 2)
        menu = bpe_decode(raw) or raw
        rows = [summarize('Mission 100', table_groups(menu, 10, 11, 100, 52, 32, 5))]
        rows.append(summarize('Survival', table_groups(menu, 12, 13, 3, 224, 24, 50)))
        pack = read(0x1DD)
        rows.append(summarize('Disc Fusion courses', table_groups(pack, 8, 9, 5, 56, 24, 8)))
        pack = read(0x1DC)
        rows.append(summarize('Disc Fusion ranking ladder', table_groups(pack, 8, 10, 100, 28, 24, 1)))
        rows.append(summarize('Disc Fusion intruders', table_groups(pack, 9, 11, 37, 28, 24, 1)))
        pack = read(0x1DB)
        rows.append(summarize('Sim Dragon rounds', table_groups(pack, 25, 26, 7, 28, 24, 1)))
        for tour in range(5):
            cpu = section(read(0x1C7 + tour), 31)
            for difficulty in range(3):
                groups = [[integers(cpu, (difficulty * 5 + r) * 20, 1)[0]] for r in range(5)]
                rows.append(summarize(f'Tournament {tour + 1}, tier {difficulty + 1} presets', groups))
        setups, changes = [], []
        for story in range(48):
            members, updates = [], []
            for commands in decode_script(read(0x1FF + story)).values():
                for command in commands:
                    if command.get('id') == 12:
                        args = command['args']
                        if ((args[0] >> 15) & 1) == 1 and args[1] >= 0:
                            members.append(args[4])
                    if command.get('id') == 14:
                        updates.extend(values[0] for letter, values in command['options'] if letter == 'l')
            if not members:
                raise ValueError(f'No enemy setups in story {story}')
            setups.append(members)
            if updates:
                changes.append(updates)
        for difficulty, offset, original in [('Easy', -5, 0), ('Normal', 0, 6), ('Hard', 5, 9)]:
            rows.append(summarize(f'Story {difficulty} enemy setup commands', setups, offset, original))
            rows.append(summarize(f'Story {difficulty} level-change commands', changes, offset, original))
        return dict(data=str(Path(root).resolve()), source_sha256=fingerprints, rows=rows)
    finally:
        files.close()


def duel_label(average):
    """Nearest original Duel preset; exact midpoint ties choose the higher one."""
    presets = [(0, 'Very Easy'), (6, 'Easy'), (13, 'Normal'),
               (21, 'Hard'), (29, 'Very Hard')]
    return min(presets, key=lambda preset: (abs(preset[0] - average), -preset[0]))[1]


def markdown(result):
    lines = ['# CPU level distribution audit', '',
             'Static inventory of referenced opponents and scripted enemy level commands.', '',
             '| Mode or sample | Entries | Original average level | With changes average level | Approximate Duel label before → after | Original 20–24 | With changes 20–24 | Original 25–29 | With changes 25–29 | Original at 29 | With changes at 29 |',
             '|---|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|']
    for row in result['rows']:
        n = row['entries']
        lines.append(f"| {row['name']} | {n} | {row['original_average']:.2f} | {row['effective_average']:.2f} | "
                     f"{duel_label(row['original_average'])} → {duel_label(row['effective_average'])} | "
                     f"{row['original_mid']} ({row['original_mid']/n:.1%}) | {row['mid']} ({row['mid']/n:.1%}) | "
                     f"{row['original_high']} ({row['original_high']/n:.1%}) | "
                     f"{row['high']} ({row['high']/n:.1%}) | "
                     f"{row['original_capped']} ({row['original_capped']/n:.1%}) | {row['capped']} ({row['capped']/n:.1%}) |")
    lines += ['', '## Counting rules', '',
              '- Mission teams count each referenced opponent slot; course sequences count each opponent appearance.',
              '- Shared opponents count again when referenced by another mission or course. Unused table rows and alignment padding are excluded.',
              '- Original averages are arithmetic means over the listed entries, including original story offsets of 0, +6, and +9. They are not weighted by play frequency.',
              '- Approximate Duel labels use the nearest original preset: Very Easy (0), Easy (6), Normal (13), Hard (21), Very Hard (29). Midpoint ties choose the higher preset. This describes the numeric average, not measured combat strength or the in-game label conversion.',
              '- Tournament rows count the five level presets per tier, including presets that a particular tournament may not reach; they are not a frequency estimate of played rounds.',
              '- Story rows count static enemy setup commands and level-change commands separately across all 48 scripts and all actions. Branches, repeated execution, and time spent at each level are not weighted.',
              '- Story difficulties are separate scenarios, not additional distinct battles. The JSON includes per-episode group counts and base/effective histograms.',
              '- Duel, Training, Ultimate Training, and network sessions are excluded because their CPU levels are not raised.',
              '- No combined percentage is given: these samples have different units and do not measure how often players play each mode.',
              '- Original story counts apply the original offsets without the new cap; levels above 29, if present, are not counted in the exact 25–29 range.',
              '', '## Reproduce', '', '```sh',
              'python port/tools/audit_cpu_levels.py --data build/ZenkaiPower-Windows/gamedata', '```', '']
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('port/build/cpu-level-audit'))
    args = parser.parse_args()
    result = audit(args.data)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.with_suffix('.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    report = markdown(result)
    args.output.with_suffix('.md').write_text(report, encoding='utf-8')
    print(report)
