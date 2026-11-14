#!/usr/bin/env python3
"""Install, migrate and deploy the Worker using private local credentials."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid

HERE = Path(__file__).resolve().parents[1]


def settings(path, name=None):
    """Keep tokens in memory; preserve existing local Worker settings on updates."""
    try:
        credentials = json.loads(path.read_text(encoding='utf-8'))
        account = credentials['account_id']
        database = str(uuid.UUID(credentials['database_id']))
        token = credentials.get('deploy_api_token') or credentials.get('api_token')
        if not isinstance(account, str) or not re.fullmatch(r'[a-fA-F0-9]{32}', account):
            raise ValueError()
        if not isinstance(token, str) or not token.strip():
            raise ValueError()
    except (OSError, ValueError, TypeError, KeyError, AttributeError):
        raise ValueError('Fill account_id, database_id and api_token (or deploy_api_token) in the credentials file.') from None
    config = json.loads((HERE / 'wrangler.jsonc').read_text())
    local = HERE / 'wrangler.local.json'
    if local.exists():
        existing = json.loads(local.read_text())
        config.update(existing)
    config['main'] = 'src/worker.js'
    config['account_id'] = account
    config['name'] = name or config['name']
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,62}', config['name']):
        raise ValueError('Worker name must contain lowercase letters, numbers or hyphens.')
    # Preserve custom bindings/settings; replace only this service's D1 target.
    binding = next((b for b in config['d1_databases'] if b['binding'] == 'DB'), None)
    if binding is None:
        raise ValueError('Local Wrangler configuration is missing the DB binding.')
    binding['database_id'] = database
    binding.pop('database_name', None)
    return config, token


def run(command, env):
    subprocess.run(command, cwd=HERE, env=env, check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--credentials', type=Path, default=HERE / 'cloudflare.local.json')
    parser.add_argument('--name', help='public Worker name; defaults to existing local configuration')
    parser.add_argument('--dry-run', action='store_true', help='install/bundle only; no remote migration or deployment')
    args = parser.parse_args(argv)
    try:
        config, token = settings(args.credentials, args.name)
        if not shutil.which('node') or not shutil.which('npm'):
            raise ValueError('Node.js 24+ and npm are required. Use the Docker command in the tutorial.')
        version = subprocess.check_output(['node', '--version'], text=True).strip()
        if int(version.lstrip('v').split('.')[0]) < 24:
            raise ValueError('Node.js 24+ is required. Rebuild the Docker image or update Node.')
        config_path = HERE / 'wrangler.local.json'
        config_path.write_text(json.dumps(config, indent=2) + '\n')
        base_env = dict(os.environ, CI='true', WRANGLER_SEND_METRICS='false')
        # Dependency installation/tests never receive the deployment token.
        base_env.pop('CLOUDFLARE_API_TOKEN', None)
        base_env.pop('CLOUDFLARE_ACCOUNT_ID', None)
        print('Installing locked dependencies...', flush=True)
        run(['npm', 'ci', '--no-audit', '--no-fund'], base_env)
        wrangler = ['node', str(HERE / 'node_modules/wrangler/bin/wrangler.js')]
        config_arg = ['--config', str(config_path)]
        print('Checking the deployment bundle...', flush=True)
        run(wrangler + ['deploy', '--dry-run', *config_arg], base_env)
        if args.dry_run:
            print('Dry run passed. No remote database or Worker was changed.')
            return 0
        remote_env = dict(base_env, CLOUDFLARE_ACCOUNT_ID=config['account_id'], CLOUDFLARE_API_TOKEN=token)
        print('Applying additive migrations to the selected remote database...', flush=True)
        run(wrangler + ['d1', 'migrations', 'apply', 'DB', '--remote', *config_arg], remote_env)
        print('Deploying the full Worker...', flush=True)
        run(wrangler + ['deploy', *config_arg], remote_env)
        print('Deployment complete. Use the workers.dev URL printed above as ZENKAIPOWER_MATCH_URL.')
        return 0
    except subprocess.CalledProcessError:
        print('Deployment stopped at the failed command. Check the error above; no later steps ran.\n'
              'For authentication errors, the token needs Workers deployment and D1 Edit permissions.', file=sys.stderr)
    except (OSError, ValueError, KeyError, TypeError):
        # Do not include JSON parsing errors: their source could contain a token.
        print('Deployment setup failed. Check the private credentials file, local Wrangler configuration, '
              'and Node.js 24+/npm installation. See the tutorial for required fields and permissions.', file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
