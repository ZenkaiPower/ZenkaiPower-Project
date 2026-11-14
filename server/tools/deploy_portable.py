#!/usr/bin/env python3
"""Run the Worker deployer from writable container storage."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
HERE=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='zenkai-cloudflare-') as temporary:
    project=Path(temporary)/'server'
    shutil.copytree(HERE,project,ignore=shutil.ignore_patterns('node_modules','.wrangler','__pycache__','cloudflare.local.json','*.log','deploy_portable.py'))
    result=subprocess.call([sys.executable,str(project/'tools/deploy_cloudflare.py'),'--credentials',str(HERE/'cloudflare.local.json'),*sys.argv[1:]])
    sys.exit(result)
