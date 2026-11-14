"""Check generated homepage links, image files and navigation anchors."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit
import sys


class Page(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.links = []

    def handle_starttag(self, tag, attributes):
        attributes = dict(attributes)
        if 'id' in attributes:
            self.ids.add(attributes['id'])
        for key in ('src', 'href'):
            if key in attributes:
                self.links.append(attributes[key])


def check(directory):
    root = Path(directory).resolve()
    html = (root / 'index.html').read_text(encoding='utf-8')
    page = Page()
    page.feed(html)
    errors = []
    for link in page.links:
        url = urlsplit(link)
        if url.scheme or url.netloc:
            continue
        path = unquote(url.path)
        if path.startswith('/'):
            prefix = '/ZenkaiPower-Project/'
            if not path.startswith(prefix):
                errors.append(f'Wrong project prefix: {link}')
                continue
            path = path[len(prefix):]
        if path and not (root / path).exists():
            errors.append(f'Missing file: {link}')
        if not path and url.fragment and unquote(url.fragment) not in page.ids:
            errors.append(f'Missing anchor: {link}')
    if '{{' in html or '{%' in html:
        errors.append('Unprocessed Liquid template')
    if errors:
        raise SystemExit('\n'.join(errors))
    print(f'PASS: {len(page.links)} links and images; project prefix and navigation anchors')


if __name__ == '__main__':
    check(sys.argv[1])
