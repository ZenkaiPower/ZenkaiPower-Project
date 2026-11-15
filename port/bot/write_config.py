#!/usr/bin/env python3
"""Create operator-local bot launcher configuration beside a built executable."""
import json
import os
from pathlib import Path
import re
import sys


def write_config(directory, credential):
    destination = Path(directory) / 'bot.local.json'
    if destination.exists():
        return 'Kept existing bot.local.json'
    if credential and not re.fullmatch(r'[a-f0-9]{64}', credential):
        raise ValueError('ZENKAIPOWER_BOT_CREDENTIAL must contain 64 lowercase hex characters')
    try:
        # Exclusive creation preserves a file made by another concurrent build.
        fd = os.open(destination, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError:
        return 'Kept existing bot.local.json'
    with os.fdopen(fd, 'w', encoding='utf-8') as output:
        json.dump({'ZENKAIPOWER_BOT_CREDENTIAL': credential}, output, indent=2)
        output.write('\n')
    return ('Created private bot.local.json from the environment' if credential else
            'Created bot.local.json template: fill in your original private credential before online use')


if __name__ == '__main__':
    try:
        print(write_config(Path(sys.argv[1]).parent, os.environ.get('ZENKAIPOWER_BOT_CREDENTIAL', '')))
    except (OSError, ValueError) as error:
        sys.exit(str(error))
