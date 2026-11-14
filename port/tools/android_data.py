#!/usr/bin/env python3
"""Export prepared game data for Android, without copying saves or credentials."""
import argparse
import re
import tempfile
from pathlib import Path
from zipfile import ZipFile, ZIP_STORED
from data_package import read_data

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=Path('gamedata'))
    parser.add_argument('--output', type=Path, default=Path('release/ZenkaiPower-android-data.zip'))
    args = parser.parse_args()
    data = args.data.resolve()
    for name in ('disc/SLUS_216.78', 'disc/BIN/DBZP.BIN', 'game-text.pak'):
        try:
            read_data(data / name)
        except (OSError, ValueError) as error:
            parser.error(f'Cannot read {data / name}: {error}. Run the desktop setup first.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.output, 'w', ZIP_STORED, allowZip64=True) as archive:
        if (data / 'pzs3us1.afs').is_file() or (data / 'pzs3us1').is_dir():
            from android_buttons import button_images
            from data_package import pack_directory
            with tempfile.TemporaryDirectory() as temporary:
                buttons = Path(temporary) / 'touch-buttons'
                buttons.mkdir()
                for name, png in button_images(data):
                    (buttons / name).write_bytes(png)
                archive.write(pack_directory(buttons), 'gamedata/touch-buttons.pak')
        for f in sorted(data.rglob('*')):
            rel = f.relative_to(data)
            # Export installed assets only. Saves, mods and validation recordings stay on the PC.
            packaged = rel.name in ('disc.pak', 'game-text.pak', 'sparking-meteor.pak', 'dragon-net-battle.pak')
            afs = re.fullmatch(r'pzs3(?:us|eu|jp)[012]\.afs', rel.name.lower()) is not None
            if f.is_file() and (rel.parts[0] in ('disc', 'title', 'eu') or len(rel.parts) == 1 and (packaged or afs)):
                archive.write(f, 'gamedata/' + rel.as_posix())
    print(args.output.resolve())

if __name__ == '__main__':
    main()
