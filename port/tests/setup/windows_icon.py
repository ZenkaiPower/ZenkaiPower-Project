"""PNG changes must regenerate icons; unchanged builds must reuse resources."""
import os
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
import windows_resource

with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source = root / 'assets/zp.png'
    source.parent.mkdir(parents=True)
    Image.new('RGBA', (240, 200), (255, 0, 0, 255)).save(source)
    metadata = root / 'game-info.json'
    metadata.write_text(json.dumps({'title': 'Dragon Ball Z Sparking! METEOR'}))
    output = root / 'build/icon.o'
    def compile_resource(args, **kwargs):
        resource = Path(args[args.index('-i') + 1]).read_text()
        assert str(output.with_suffix('.ico').resolve()) in resource
        Path(args[args.index('-o') + 1]).write_bytes(b'compiled')
    with patch.object(windows_resource, 'ROOT', root), patch.object(windows_resource.subprocess, 'run', side_effect=compile_resource) as compile:
        windows_resource.build_icon(output, 'windres')
        with Image.open(output.with_suffix('.ico')) as icon:
            assert len(icon.ico.sizes()) == 7
            assert icon.convert('RGBA').getpixel((128, 128)) == (255, 0, 0, 255)
            assert icon.convert('RGBA').getpixel((0, 0))[3] == 0
        windows_resource.build_icon(output, 'windres')
        assert compile.call_count == 1
        original = source.stat()
        Image.new('RGBA', (240, 200), (0, 0, 255, 255)).save(source)
        os.utime(source, ns=(original.st_atime_ns, original.st_mtime_ns))
        windows_resource.build_icon(output, 'windres')
        assert compile.call_count == 2
        with Image.open(output.with_suffix('.ico')) as icon:
            assert icon.convert('RGBA').getpixel((128, 128)) == (0, 0, 255, 255)
        metadata.write_text(json.dumps({'title': 'New "Game" \u2014 Title'}))
        windows_resource.build_icon(output, 'windres')
        assert compile.call_count == 3
        resource = output.with_suffix('.rc').read_text()
        assert 'VALUE "FileDescription", "New \\"Game\\" \u2014 Title' in resource
        assert 'VALUE "ProductName"' in resource
        padded = Image.new('RGBA', (100, 106), (0, 0, 0, 0))
        padded.paste((255, 128, 0, 255), (3, 25, 94, 84))
        padded.save(source)
        windows_resource.build_icon(output, 'windres')
        with Image.open(output.with_suffix('.ico')) as icon:
            bounds = icon.convert('RGBA').getchannel('A').getbbox()
            assert bounds[0] == 8 and bounds[2] == 248, bounds
            assert bounds[3] - bounds[1] == 156, bounds
print('PASS: PNG conversion, transparency, seven sizes, incremental reuse and PNG content change detection')
