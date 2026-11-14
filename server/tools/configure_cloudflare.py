#!/usr/bin/env python3
"""Generate an ignored Wrangler config from local database settings (no token copied)."""
import argparse
import json
from pathlib import Path
import re
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--name', default='zenkaipower-server', help='public Worker name; use a separate name for staging')
    parser.add_argument('--database-id', help='override the local database UUID (for a separate staging database)')
    parser.add_argument('--staging', action='store_true', help='write wrangler.staging.local.json instead')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,62}', args.name):
        parser.error('Worker name must contain lowercase letters, numbers or hyphens')
    here = Path(__file__).resolve().parents[1]
    try:
        local = json.loads((here / 'cloudflare.local.json').read_text())
        account = local['account_id']
        if not re.fullmatch(r'[a-fA-F0-9]{32}', account):
            raise ValueError()
        database = str(uuid.UUID(args.database_id or local['database_id']))
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        parser.error('Fill account_id and database_id in cloudflare.local.json first')
    config = json.loads((here / 'wrangler.jsonc').read_text())
    config.update(name=args.name, account_id=account)
    config['d1_databases'][0]['database_id'] = database
    # The UUID identifies the database. Its arbitrary dashboard name is not needed.
    config['d1_databases'][0].pop('database_name', None)
    destination = here / ('wrangler.staging.local.json' if args.staging else 'wrangler.local.json')
    destination.write_text(json.dumps(config, indent=2) + '\n')
    print(f'Created server/{destination.name} (API token not copied).')


if __name__ == '__main__':
    main()
