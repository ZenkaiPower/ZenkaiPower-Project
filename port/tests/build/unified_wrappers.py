"""Verify Android's Docker wrapper keeps APK packaging inside the shared image."""
from pathlib import Path
import json
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as temporary:
    folder = Path(temporary)
    log = folder / 'calls.jsonl'
    docker = folder / 'docker'
    docker.write_text('#!/usr/bin/env python3\nimport json,os,sys\nwith open(os.environ["DOCKER_TEST_LOG"],"a") as f: f.write(json.dumps(sys.argv[1:])+"\\n")\n')
    docker.chmod(0o755)
    env = dict(os.environ, PATH=str(folder) + os.pathsep + os.environ['PATH'],
               DOCKER_TEST_LOG=str(log), ZENKAIPOWER_SKIP_IMAGE_BUILD='1',
               ZENKAI_SIGNING_PROPERTIES=str(folder / 'missing.properties'))
    for args, release in (([], '0'), (['--release'], '1')):
        log.write_text('')
        subprocess.run(['sh', 'android/build.sh', *args], cwd=root, env=env, check=True)
        calls = [json.loads(line) for line in log.read_text().splitlines()]
        assert len(calls) == 1
        command = calls[0]
        assert command[:2] == ['run', '--rm']
        assert command[-2:] == ['zenkaipower', '/host/android/container-build.sh']
        assert 'ZENKAIPOWER_ANDROID_APK=1' in command and 'ZENKAIPOWER_RELEASE=' + release in command
        assert 'type=volume,source=zenkaipower-gradle,target=/root/.gradle' in command
print('PASS: Android debug/release wrappers use one container for engine and APK packaging')
