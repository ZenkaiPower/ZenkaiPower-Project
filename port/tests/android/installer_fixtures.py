"""Generate private test assets; synthetic discs plus optional owner-provided icon data."""
from pathlib import Path
import argparse
import importlib.util
import shutil
import subprocess
import sys
import struct

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'port/tools'))
spec = importlib.util.spec_from_file_location('native_extract', ROOT/'port/tests/installer/native_extract.py')
fixture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixture)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--boot', type=Path, help='Owner-extracted boot icons (optional; never in release assets)')
    parser.add_argument('--hud', type=Path, help='Owner-extracted HUD file 00005.bin, required with --boot')
    args = parser.parse_args()
    if bool(args.boot) != bool(args.hud): parser.error('--boot and --hud must be supplied together')
    target = ROOT/'android/app/src/androidTest/assets/installer'
    target.mkdir(parents=True, exist_ok=True)
    fixture.japanese_iso(target/'jp.iso')
    reference = target/'reference-jp'
    fixture.python_jp(target/'jp.iso', reference)
    menu = fixture.wii_archives(target)
    fixture.python_wii(menu, target/'reference-wii', None, None, target/'wzs3eu2.afs', True)
    fixture.gamecube_container(target/'wii.iso', {'wzs3eu1.afs': menu.read_bytes(),
                                              'wzs3eu2.afs': (target/'wzs3eu2.afs').read_bytes()})
    # nod also validates partition metadata; provide an empty, bounded DOL header.
    container = bytearray((target/'wii.iso').read_bytes())
    struct.pack_into('>I', container, 0x420, 0x3000)
    malformed = bytearray(container)
    (target/'invalid-dol.iso').write_bytes(malformed)
    struct.pack_into('>I', container, 0x3000, 0x100)
    struct.pack_into('>I', container, 0x3090, 4)
    (target/'wii.iso').write_bytes(container)
    windows = ROOT/'port/third_party/dolphin-tool/windows-x64/DolphinTool.exe'
    linux = ROOT/'port/third_party/dolphin-tool/linux-x64/dolphin-tool'
    tool = windows if sys.platform == 'win32' else linux
    subprocess.run([str(tool), 'convert', '-i', str(target/'wii.iso'), '-o', str(target/'wii.rvz'),
                    '-f', 'rvz', '-c', 'zstd', '-b', '131072', '-l', '1', '-u', str(target/'tool-user')], check=True)
    if args.boot:
        from android_buttons import button_images
        game = target/'button-fixture'
        game.mkdir(exist_ok=True)
        (game/'pzs3us1.afs').write_bytes(fixture.afs({4: args.boot.read_bytes(), 5: args.hud.read_bytes()}, count=6))
        expected = target/'reference-buttons'; expected.mkdir(exist_ok=True)
        for name, png in button_images(game): (expected/name).write_bytes(png)
    print('Installer fixtures:', target)

if __name__ == '__main__': main()
