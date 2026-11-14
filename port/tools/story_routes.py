#!/usr/bin/env python3
"""Decode Dragon History ending actions and their incoming routes, without running the game.

Reads user-owned loose files/mods or pzs3us1.afs. Output stays in port/build by default.
It reports script facts, never assigns a speculative 'canon' label.
"""
import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import struct

from make_fingerprint import bpe_decode

ROOT = Path(__file__).resolve().parents[2]
TIME_EVENTS = (10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90, 100, 120, 140, 160, 180)
RESULTS = {0: 'defeat / KO reason', 1: 'victory / KO reason',
           2: 'defeat / flag-7 reason', 3: 'victory / flag-7 reason'}


class GameFiles:
    def __init__(self, root):
        self.root = Path(root)
        path = self.root / 'pzs3us1.afs'
        self.archive = path.open('rb') if path.exists() else None
        if self.archive:
            magic, self.count = struct.unpack('<4sI', self.archive.read(8))
            if magic != b'AFS\0':
                raise ValueError('Not an AFS archive')

    def close(self):
        if self.archive:
            self.archive.close()

    def read(self, file_id):
        rel = Path('pzs3us1') / f'{file_id-1:05d}.bin'
        for path in (self.root / 'mods' / rel, self.root / rel):
            if path.exists():
                raw = path.read_bytes()
                return bpe_decode(raw) or raw
        if not self.archive or not 1 <= file_id <= self.count:
            raise ValueError(f'Cannot find game file 0x{file_id:X}')
        self.archive.seek(8 + (file_id-1)*8)
        offset, size = struct.unpack('<II', self.archive.read(8))
        self.archive.seek(offset)
        raw = self.archive.read(size)
        if len(raw) != size:
            raise ValueError('Truncated AFS member')
        return bpe_decode(raw) or raw


def chunk(data, offset):
    if offset < 0 or offset + 16 > len(data):
        raise ValueError('Truncated GSC chunk header')
    tag, head, size, arg = struct.unpack_from('<4sIIi', data, offset)
    end = offset + head + size
    if head < 16 or end + 16 > len(data) or data[end:end+4] != b'EOFC':
        raise ValueError(f'Invalid {tag!r} chunk bounds/trailer')
    return tag, offset + head, size, arg, end + 16


def decode_script(data):
    tag, start, size, _, _ = chunk(data, 0)
    if tag != b'GSCF':
        raise ValueError('Expected GSCF')
    parts, offset = {}, start
    while offset < start + size:
        tag, body, length, _, following = chunk(data, offset)
        if tag in parts:
            raise ValueError('Duplicate GSC section')
        parts[tag] = (body, length)
        offset = following
    if offset != start + size:
        raise ValueError('Misaligned GSC sections')
    pool_start, pool_size = parts[b'GSDT']
    values = data[pool_start:pool_start+pool_size]
    offset, length = parts[b'GSCD']
    end, actions = offset + length, {}
    while offset < end:
        tag, body, size, action_id, following = chunk(data, offset)
        if tag != b'GSAC' or size % 4 or following > end or action_id in actions:
            raise ValueError('Invalid or duplicate GSAC action')
        words = struct.unpack_from(f'<{size//4}I', data, body)
        commands, index = [], 0
        while index < len(words):
            word = words[index]
            kind = word & 15
            if kind == 0:
                break
            if kind in (2, 3):
                target = struct.unpack('<h', struct.pack('<H', word >> 16))[0]
                commands.append(dict(call=target, wait=kind == 3))
                index += 1
                continue
            if kind != 1:
                raise ValueError(f'Unexpected word kind {kind} in action {action_id}')
            command = dict(id=word >> 16, args=[], options=[])
            expected = (word >> 8) & 255
            destination = command['args']
            index += 1
            while index < len(words) and words[index] & 8:
                word = words[index]
                kind = word & 15
                if kind == 8:
                    if len(destination) != expected:
                        raise ValueError('Operand count mismatch')
                    expected = (word >> 16) & 255
                    destination = []
                    command['options'].append([chr((word >> 8) & 255), destination])
                elif kind == 10:
                    pool_index, value_type = word >> 8, (word >> 4) & 15
                    if pool_index * 4 + 4 > len(values):
                        raise ValueError('Operand outside GSDT')
                    if value_type in (0, 1):
                        value = struct.unpack_from('<i' if value_type == 0 else '<f', values, pool_index*4)[0]
                    else:
                        value = dict(type=value_type, pool_index=pool_index)
                    destination.append(value)
                else:
                    raise ValueError('Invalid operand word')
                index += 1
            if len(destination) != expected:
                raise ValueError('Operand count mismatch')
            commands.append(command)
        if index == len(words):
            raise ValueError('Action without END word')
        actions[action_id] = commands
        offset = following
    if offset != end:
        raise ValueError('Misaligned action chunks')
    return actions


def text_line(data, line):
    count = struct.unpack_from('<I', data)[0]
    if not 0 <= line < count:
        raise ValueError('Dialogue index outside text table')
    offset = struct.unpack_from('<I', data, 4*(line+1))[0] & ~3
    end = offset
    while end + 2 <= len(data) and data[end:end+2] != b'\0\0':
        end += 2
    if offset < 4*(count+2) or end + 2 > len(data):
        raise ValueError('Invalid text bounds')
    return data[offset:end].decode('utf-16-le').lstrip('\ufeff').replace('\n', ' ')


def trigger_description(trigger):
    if trigger == 0xFFFF:
        return 'wait flag off'
    event, side = trigger & 0xFFF, (trigger >> 15) & 1
    if event < len(TIME_EVENTS):
        name = f'sub-clock reaches {TIME_EVENTS[event]} seconds'
    elif 0x16 <= event <= 0x1C:
        name = f'at least {(event-0x15)*10000} HP lost (range trigger)'
    else:
        name = {0x48: 'opponent knocked out', 0x4A: 'own fighter knocked out',
                0x49: 'fighter flag 7 (ring-out/forced defeat)', 0x50: 'R3 pressed while able to act'}.get(event, f'event 0x{event:02X}')
    return f'side {side}: {name}'


def analyze(actions, text):
    edges, endings = [], []
    for action, commands in actions.items():
        for command in commands:
            if 'call' in command:
                edges.append(dict(source=action, target=command['call'], kind='call', wait=command['wait']))
            elif command['id'] == 7:
                for option, args in command['options']:
                    if option == 'a':
                        if len(args) != 2:
                            raise ValueError('Invalid event registration')
                        trigger, target = args
                        edges.append(dict(source=action, target=target, kind='trigger', trigger=trigger,
                                          description=trigger_description(trigger)))
            elif command['id'] == 9 and command['args'][0] >= 0:
                result = command['args'][0]
                dialogue = [dict(speaker=c['args'][0], line=c['args'][1], text=text_line(text, c['args'][1]))
                            for c in commands if c.get('id') == 1603]
                endings.append(dict(action=action, result=result, outcome=RESULTS.get(result, 'unknown'), dialogue=dialogue))
    if any(edge['target'] not in actions for edge in edges):
        raise ValueError('Route references an undefined action')
    # This graph expresses potential registrations/calls, not a simulated playthrough.
    reachable, queue = set(), deque([1000, -1])
    while queue:
        node = queue.popleft()
        if node not in actions or node in reachable:
            continue
        reachable.add(node)
        queue.extend(edge['target'] for edge in edges if edge['source'] == node)
    for ending in endings:
        ending['incoming'] = [edge for edge in edges if edge['target'] == ending['action']]
        ending['reached_in_static_graph'] = ending['action'] in reachable
    return dict(actions=actions, edges=edges, endings=endings)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT / 'release/linux-x64/gamedata')
    parser.add_argument('--output', type=Path, default=ROOT / 'port/build/story_routes')
    args = parser.parse_args()
    files = GameFiles(args.data)
    stories = []
    try:
        for story in range(48):
            data = files.read(0x1FF + story)
            actions = decode_script(data)
            analysis = analyze(actions, files.read(0x231 + story))
            stories.append(dict(story=story, file_id=0x1FF + story,
                                script_sha256=hashlib.sha256(data).hexdigest(), **analysis))
    finally:
        files.close()
    counts = Counter(ending['result'] for story in stories for ending in story['endings'])
    output = dict(data=str(args.data.resolve()), scope='48 original Dragon History scripts, IDs 0..47',
                  limitations='Static potential routes, not runtime verification or a canon classification. '
                              'A false reached_in_static_graph flag does not prove an action is unreachable '
                              '(external engine paths are not modeled).',
                  result_counts=dict(counts), stories=stories)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    path = Path(str(args.output) + '.json')
    path.write_text(json.dumps(output, indent=2, ensure_ascii=False) + '\n')
    lines = ['# Dragon History ending inventory', '',
             'Static script facts; no automatic canon classification. Script IDs are zero-based.', '',
             '| Script | File | Victory endings | Defeat endings |', '|---:|---:|---|---|']
    for story in stories:
        wins = ', '.join(str(e['action']) for e in story['endings'] if e['result'] in (1, 3))
        losses = ', '.join(str(e['action']) for e in story['endings'] if e['result'] in (0, 2))
        lines.append(f"| {story['story']} | 0x{story['file_id']:X} | {wins} | {losses} |")
    lines.extend(['', '## Ending details', '',
                  'Triggers are registered by the source action; they replace its earlier trigger list. '
                  'Timing is relative to the sub-clock reset. Player-confirmed transitions may also require R3. '
                  'These are static registrations, not a tested walkthrough. Empty incoming lists do not prove '
                  'an action is unused.', ''])
    for story in stories:
        lines.extend([f"### Script {story['story']} — file 0x{story['file_id']:X}", '',
                      '| Ending action | Outcome | Incoming registrations/calls | First dialogue line |',
                      '|---:|---|---|---|'])
        for ending in story['endings']:
            incoming = '; '.join(f"{e['source']}: {e.get('description', 'CALL')}" for e in ending['incoming']) or 'Not found in this static graph'
            clue = ending['dialogue'][0]['text'] if ending['dialogue'] else '—'
            lines.append(f"| {ending['action']} | {ending['outcome']} | {incoming} | {clue.replace('|', chr(92)+'|')} |")
    markdown = Path(str(args.output) + '.md')
    markdown.write_text('\n'.join(lines) + '\n')
    print(f'Decoded {len(stories)} scripts, {sum(len(s["actions"]) for s in stories)} actions, '
          f'{sum(counts.values())} ending actions; all route targets resolve.')
    print(f'Results: {dict(counts)}\nJSON: {path}\nInventory: {markdown}')


if __name__ == '__main__':
    main()
