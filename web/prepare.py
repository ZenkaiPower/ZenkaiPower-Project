"""Stage the public website without copying game data or local setup artwork."""
from pathlib import Path
from html.parser import HTMLParser
import argparse
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def carousel_slides(readme):
    class Images(HTMLParser):
        def __init__(self):
            super().__init__()
            self.slides = []

        def handle_starttag(self, tag, attributes):
            attributes = dict(attributes)
            if tag != 'img' or 'data-carousel-caption' not in attributes:
                return
            src = attributes.get('src', '').removeprefix('./')
            if not re.fullmatch(r'figs/[^/\\]+\.gif', src, re.IGNORECASE):
                raise ValueError(f'Carousel image must be a local GIF in figs/: {src}')
            if not (ROOT / src).is_file():
                raise ValueError(f'Missing carousel GIF: {src}')
            caption = (attributes['data-carousel-caption'] or '').strip()
            if not caption:
                raise ValueError(f'Missing carousel caption: {src}')
            duration = int(attributes.get('data-carousel-duration', '7000'))
            if not 1000 <= duration <= 300000:
                raise ValueError(f'Invalid carousel duration: {src}')
            self.slides.append(dict(src='/' + src, caption=caption,
                                    alt=attributes.get('alt') or caption,
                                    duration=duration,
                                    icon=attributes.get('width', '') in ('32', '64')))

    parser = Images()
    parser.feed(readme)
    return parser.slides


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
    shutil.copy2(ROOT / 'assets/zp-avatar.png', destination / 'assets/zp-avatar.png')
    shutil.copytree(ROOT / 'figs', destination / 'figs')
    readme = (ROOT / 'README.md').read_text(encoding='utf-8-sig')
    readme = re.sub(r'<!-- readme-only:start -->.*?<!-- readme-only:end -->\s*', '', readme, flags=re.DOTALL)
    (destination / '_data').mkdir()
    (destination / '_data/carousel.json').write_text(
        json.dumps(carousel_slides(readme), ensure_ascii=False, indent=2), encoding='utf-8')
    # Wrap edition content for the website while keeping the GitHub README intact.
    editions_start = readme.index('### Sparking! METEOR')
    editions_end = readme.index('<img src="./assets/zp-avatar.png"', editions_start)
    editions = readme[editions_start:editions_end]
    editions = re.sub(
        r'(### [^\n]+\n\n)(.*?)(?=### |\Z)',
        lambda match: match[1] + '<div class="edition-row" markdown="1">\n\n'
        + match[2].rstrip() + '\n\n</div>\n\n',
        editions, flags=re.DOTALL,
    )
    readme = readme[:editions_start] + editions + readme[editions_end:]
    # Each Sparking! Changes subsection becomes a responsive grid item.
    def sparking_grid(match):
        parts = re.split(r'(?=^### )', match[2], flags=re.MULTILINE)
        if len(parts) == 1:
            return match[0]
        items = [
            '<div class="sparking-feature" markdown="1">\n\n'
            + part.rstrip() + '\n\n</div>'
            for part in parts[1:]
        ]
        return (match[1] + parts[0]
                + '<div class="sparking-grid" markdown="1">\n\n'
                + '\n\n'.join(items) + '\n\n</div>\n\n')

    readme = re.sub(r'(^## Sparking! Changes\n)(.*?)(?=^## |\Z)',
                    sparking_grid, readme, flags=re.MULTILINE | re.DOTALL)
    readme = readme.replace('(./LICENSE)', '(https://github.com/ZenkaiPower/ZenkaiPower-Project/blob/main/LICENSE)')
    readme = re.sub(r'\]\((?:\./)?(docs/[^)]+)\)',
                    r'](https://github.com/ZenkaiPower/ZenkaiPower-Project/blob/main/\1)', readme)
    (destination / 'README.md').write_text(readme, encoding='utf-8')
    print(f'Website source: {destination}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    prepare(parser.parse_args().destination)
