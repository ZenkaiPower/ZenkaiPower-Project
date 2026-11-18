"""Stage the public website without copying game data or local setup artwork."""
from pathlib import Path
import argparse
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def prepare(destination):
    destination = Path(destination).resolve()
    # Require a new directory so staging never overwrites repository files.
    destination.mkdir(parents=True, exist_ok=False)
    for name in ('_config.yml', 'index.md', '_layouts'):
        source = ROOT / 'web' / name
        if source.is_dir():
            shutil.copytree(source, destination / name)
        else:
            shutil.copy2(source, destination / name)
    shutil.copytree(ROOT / 'web/assets', destination / 'assets')
    shutil.copy2(ROOT / 'assets/zp.png', destination / 'assets/zp.png')
    shutil.copytree(ROOT / 'figs', destination / 'figs')
    readme = (ROOT / 'README.md').read_text(encoding='utf-8-sig')
    # Wrap edition content for the website while keeping the GitHub README intact.
    editions_start = readme.index('### Sparking! METEOR')
    editions_end = readme.index('<img src="./assets/zp.png"', editions_start)
    editions = readme[editions_start:editions_end]
    editions = re.sub(
        r'(### [^\n]+\n\n)(.*?)(?=### |\Z)',
        lambda match: match[1] + '<div class="edition-row" markdown="1">\n\n'
        + match[2].rstrip() + '\n\n</div>\n\n',
        editions, flags=re.DOTALL,
    )
    readme = readme[:editions_start] + editions + readme[editions_end:]
    readme = readme.replace('(./LICENSE)', '(https://github.com/ZenkaiPower/ZenkaiPower-Project/blob/main/LICENSE)')
    readme = re.sub(r'\]\((?:\./)?(docs/[^)]+)\)',
                    r'](https://github.com/ZenkaiPower/ZenkaiPower-Project/blob/main/\1)', readme)
    (destination / 'README.md').write_text(readme, encoding='utf-8')
    print(f'Website source: {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    prepare(parser.parse_args().destination)
