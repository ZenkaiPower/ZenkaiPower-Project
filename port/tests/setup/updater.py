#!/usr/bin/env python3
"""Run with the compiled updater.cpp test driver as the first argument. No network or user files are used."""
import hashlib, json, os, pathlib, shutil, subprocess, sys, tempfile, time, zipfile
exe = str(pathlib.Path(sys.argv[1]).resolve())
folder = 'ZenkaiPower-Windows' if os.name == 'nt' else 'ZenkaiPower-Linux'
subprocess.run([exe], check=True)
with tempfile.TemporaryDirectory() as t:
    base = pathlib.Path(t)
    def archive(name):
        p = base / 'update.zip'
        with zipfile.ZipFile(p, 'w', zipfile.ZIP_DEFLATED) as z:
            z.writestr(name, b'data')
        return p
    def valid(path):
        return subprocess.run([exe, 'zip', str(path)], capture_output=True).returncode == 0
    game = 'ZenkaiPower.exe' if os.name == 'nt' else 'ZenkaiPower'
    assert valid(archive(folder + '/' + game))
    assert valid(archive(folder + '/' + game + '.mem'))
    assert not valid(archive(folder + '/other.exe.mem'))
    # Exercise Windows archive names even when running the suite on Linux.
    for name, accepted in [('ZenkaiPower.exe.mem', True), ('other.exe.mem', False)]:
        path = archive('ZenkaiPower-Windows/' + name)
        result = subprocess.run([exe, 'zip-windows', str(path)], capture_output=True)
        assert (result.returncode == 0) == accepted, result.stderr
    for name in [folder + '/../evil', '/tmp/evil', folder + '/gamedata/user.afs',
                 folder + '/savegame/settings', folder + '/C:/evil', folder + '/a\\evil']:
        assert not valid(archive(name)), name
    p = base / 'update.zip'
    with zipfile.ZipFile(p, 'w') as z:
        i = zipfile.ZipInfo(folder + '/lib/link'); i.create_system = 3; i.external_attr = 0o120777 << 16
        z.writestr(i, '../../evil')
    assert not valid(p)
    p.write_bytes(b'not a zip'); assert not valid(p)
    # Fake curl returns a local GitHub response, exercising the actual parser and platform asset selection.
    curl = base / 'curl'
    curl.write_text('#!/bin/sh\nwhile [ "$1" != "--output" ]; do shift; done\nprintf "%s" "$3" > "$UPDATE_REQUEST"\ncp "$UPDATE_FIXTURE" "$2"\n')
    curl.chmod(0o755)
    fixture = base / 'release.json'
    request = base / 'request.txt'
    env = dict(os.environ, PATH=str(base) + os.pathsep + os.environ['PATH'], UPDATE_FIXTURE=str(fixture), UPDATE_REQUEST=str(request))
    release = {'tag_name': 'v1.1.0', 'draft': False, 'prerelease': False, 'assets': [
        {'name': 'ZenkaiPower-linux-x64.zip', 'browser_download_url':
         'https://github.com/example/game/releases/download/v1.1.0/ZenkaiPower-linux-x64.zip',
         'digest': 'sha256:' + 'a' * 64}]}
    def check():
        fixture.write_text(json.dumps(release))
        return subprocess.check_output([exe, 'check'], env=env, text=True)
    assert check() == '1|v1.1.0|'
    assert request.read_text() == 'https://api.github.com/repos/example/game/releases/latest'
    release['assets'].append(dict(release['assets'][0], name='ZenkaiPower-v1.1.0-linux-x64.zip'))
    release['assets'][0]['digest'] = ''
    assert check() == '1|v1.1.0|', 'Versioned asset should take priority over legacy asset'
    release['assets'] = [release['assets'][1]]
    release['assets'][0]['name'] = 'ZenkaiPower-v9.9.9-linux-x64.zip'
    assert check().startswith('0|v1.1.0|Release needs'), 'Another version must not match'
    release['assets'][0]['name'] = 'ZenkaiPower-v1.1.0-linux-x64.zip'
    assert check() == '1|v1.1.0|'

    release['tag_name'] = 'v1.0.0'; assert check() == '0|v1.0.0|'
    release['tag_name'] = 'v1.1.0'; release['assets'][0]['digest'] = ''
    assert check().startswith('0|v1.1.0|Release needs')
    release['assets'][0]['digest'] = 'sha256:' + 'a' * 64
    release['assets'][0]['browser_download_url'] = 'https://example.com/malware.zip'
    assert check().startswith('0|v1.1.0|Release needs')
    release['prerelease'] = True; assert 'No stable release' in check()
    # Exercise the real filesystem helper: successful replacement preserves saves; launch failure rolls back.
    for success in [True, False]:
        root = base / ('success' if success else 'rollback'); root.mkdir()
        (root/'savegame').mkdir(); (root/'savegame'/'user').write_text('keep')
        (root/'gamedata').mkdir(); (root/'gamedata'/'user').write_text('keep')
        (root/'ZenkaiPower').write_text('old')
        (root/'setup').mkdir(); (root/'setup'/'en.json').write_text('{"welcome.title":"Custom"}')
        stage = root/'.update'; payload = stage/'extracted'/folder; payload.mkdir(parents=True)
        (payload/'ZenkaiPower').write_text('new')
        (payload/'setup').mkdir(); (payload/'setup'/'en.json').write_text('{"welcome.title":"Default"}')
        if success:
            setup = payload/'ZenkaiPower-setup'; setup.write_text('#!/bin/sh\nexit 0\n'); setup.chmod(0o755)
        r = subprocess.run([exe, '--apply-update', '2147483647', str(root), str(stage)], capture_output=True)
        assert r.returncode == (0 if success else 1), r.stderr
        assert (root/'ZenkaiPower').read_text() == ('new' if success else 'old')
        assert (root/'savegame'/'user').read_text() == 'keep'
        assert (root/'gamedata'/'user').read_text() == 'keep'
        assert (root/'setup'/'en.json').read_text() == '{"welcome.title":"Custom"}'
        assert stage.exists() != success
    # Complete download/stage/detached helper/restart flow using a local archive and the real SHA tool.
    root = base/'complete'; root.mkdir(); (root/'lib').mkdir()
    shutil.copy2(exe, root/'ZenkaiPower-setup')
    (root/'ZenkaiPower').write_text('old')
    archive_path = base/'complete.zip'
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr(folder + '/ZenkaiPower', b'new')
        z.writestr(folder + '/ZenkaiPower.dat', b'data')
        z.writestr(folder + '/game-info.pak', b'info')
        i = zipfile.ZipInfo(folder + '/ZenkaiPower-setup'); i.create_system = 3; i.external_attr = 0o100755 << 16
        z.writestr(i, '#!/bin/sh\ntouch "' + str(root/'restarted') + '"\n')
    env['UPDATE_FIXTURE'] = str(archive_path)
    digest = 'sha256:' + hashlib.sha256(archive_path.read_bytes()).hexdigest()
    subprocess.run([exe, 'prepare', str(root), 'sha256:'+'0'*64], env=env, check=False, capture_output=True)
    assert (root/'ZenkaiPower').read_text() == 'old'
    subprocess.run([exe, 'prepare', str(root), digest], env=env, check=True)
    for _ in range(100):
        if (root/'restarted').exists(): break
        time.sleep(.05)
    assert (root/'restarted').exists(), 'Detached helper did not restart setup'
    assert (root/'ZenkaiPower').read_text() == 'new'
    for _ in range(100):
        if not list(root.glob('.zenkai-update-*')): break
        time.sleep(.05)
    assert not list(root.glob('.zenkai-update-*')), 'Successful update left its staging folder'
print('PASS: archive validation, release responses, update installation, preserved data and rollback')
