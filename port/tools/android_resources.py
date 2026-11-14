"""Build Android's visible resources using the desktop release packers."""
from pathlib import Path
import shutil
import tempfile
from data_package import pack_directory
from pack_game_info import pack_game_info
from package import take_fonts, take_translations, take_server, take_setup_text

ROOT = Path(__file__).resolve().parents[2]

def prepare_resources(assets, metadata):
    assets, metadata = Path(assets).resolve(), Path(metadata).resolve()
    assets.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='android-resources-', dir=assets) as temporary:
        stage = Path(temporary)/'engine'
        stage.mkdir()
        licenses = stage/'licenses'
        licenses.mkdir()
        take_fonts(stage, licenses)
        take_translations(stage)
        pack_game_info(stage)
        take_server(stage)
        take_setup_text(stage)
        for source in (assets/'licenses', ROOT/'android/licenses'):
            if source.is_dir(): shutil.copytree(source, licenses, dirs_exist_ok=True)
        for source, name in [('LICENSE','tenkaichi3decomp-agpl.txt'),
                             ('port/third_party/imgui/LICENSE.txt','dear-imgui.txt'),
                             ('port/third_party/monocypher/LICENCE.md','monocypher.txt')]:
            shutil.copy2(ROOT/source, licenses/name)
        support = stage/'setup/engine'
        support.mkdir(parents=True)
        for suffix in ('dat','rel','mem'):
            shutil.copy2(metadata/('libzenkaipower.so.'+suffix), support/('libzenkaipower.so.'+suffix))
        pack_directory(support)
        target = assets/'engine'
        # Only replace the generated APK assets directory, never source resources.
        if target.resolve().parent != assets: raise ValueError('Invalid APK asset target')
        if target.exists(): shutil.rmtree(target)
        shutil.move(str(stage), target)
    for suffix in ('dat','rel','mem'):
        (assets/('libzenkaipower.so.'+suffix)).unlink(missing_ok=True)

if __name__ == '__main__':
    prepare_resources(ROOT/'android/app/src/main/assets', ROOT/'port/build/android')
