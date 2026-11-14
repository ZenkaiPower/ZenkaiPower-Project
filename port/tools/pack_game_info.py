#!/usr/bin/env python3
"""Package root game-info.json for the settings footer (no game build needed)."""
import argparse
import json
from pathlib import Path
import tempfile
from data_package import pack_directory

ROOT = Path(__file__).resolve().parents[2]
FIELDS = ('title', 'subtitle', 'version', 'source', 'original_source', 'page')


def pack_game_info(out=ROOT):
    source = ROOT / 'game-info.json'
    raw = source.read_bytes()
    info = json.loads(raw)
    if not isinstance(info, dict):
        raise ValueError('game-info.json must contain an object')
    for key in FIELDS:
        value = info.get(key, '')
        if not isinstance(value, str) or len(value.encode('utf-8')) > 1023 or any(ord(c) < 32 for c in value):
            raise ValueError(f'Invalid game-info.json field: {key}')
        if key in ('source', 'original_source', 'page') and value and not value.startswith(('https://', 'http://')):
            raise ValueError(f'{key} must be an HTTP or HTTPS URL')
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        staging = Path(temporary) / 'game-info'
        staging.mkdir()
        (staging / 'game-info.json').write_bytes(raw)
        for key in FIELDS:
            (staging / (key + '.txt')).write_text(info.get(key, ''), encoding='utf-8')
        return pack_directory(staging, package=out / 'game-info.pak')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT)
    print(pack_game_info(parser.parse_args().out))
