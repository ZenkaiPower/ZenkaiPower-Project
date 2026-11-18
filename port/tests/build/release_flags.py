#!/usr/bin/env python3
"""Release launch gates and Docker flag forwarding without a full build."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[3]
mem=(root/'port/src/plat_mem.c').read_text()
a=mem.index('#if defined(PORT_DISTRIBUTION_BUILD)')
b=mem.index('\n#define HEAP_BASE',a)
with tempfile.TemporaryDirectory() as tmp:
    tmp=Path(tmp)
    src=tmp/'flags.c'
    src.write_text('#include <assert.h>\n#include <string.h>\n'+mem[a:b]+r'''
int main(void) {
    char *argv[]={"game","--cheats","--settings"};
    assert(!Port_SettingsEnabled());
    Port_ParseLaunchArgs(3,argv);
#if PORT_DISTRIBUTION_BUILD
    assert(!Port_CheatsEnabled());
    assert(!Port_SettingsEnabled());
#else
    assert(Port_CheatsEnabled());
    assert(Port_SettingsEnabled());
#endif
}
''')
    for release in (0,1):
        exe=tmp/f'flags{release}'
        subprocess.run(['cc',f'-DPORT_DISTRIBUTION_BUILD={release}',str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    docker=tmp/'docker';log=tmp/'docker.json'
    docker.write_text('#!/usr/bin/env python3\nimport os,sys,json\nwith open(os.environ["DOCKER_TEST_LOG"],"a") as f: f.write(json.dumps(sys.argv[1:])+"\\n")\n')
    docker.chmod(0o755)
    env=dict(os.environ,PATH=str(tmp)+os.pathsep+os.environ['PATH'],ZENKAIPOWER_SKIP_IMAGE_BUILD='1',DOCKER_TEST_LOG=str(log))
    for args in (['linux','--release'],['--release','win'],['linux']):
        log.write_text('')
        subprocess.run(['sh','port/release/build.sh',*args],cwd=root,env=env,check=True,stdout=subprocess.DEVNULL)
        calls=[json.loads(line) for line in log.read_text().splitlines()]
        run=next(call for call in calls if call[0]=='run')
        forwarded=run[run.index('port/release/inside.sh')+1:]
        assert ('--release' in forwarded)==('--release' in args)
        assert forwarded[0]==('win' if 'win' in args else 'linux')
    result=subprocess.run(['sh','port/release/build.sh','--invalid'],cwd=root,env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    assert result.returncode==2
print('Release launch gates and script forwarding checks passed')

launch=json.loads((root/'.vscode/launch.json').read_text())['configurations']
tasks=json.loads((root/'.vscode/tasks.json').read_text())['tasks']
by_label={task['label']:task for task in tasks}
assert launch[0]['name']=='Build and run Windows debug'
for platform in ('Windows','Linux'):
    config=next(c for c in launch if c['name']==f'Build and run {platform} debug')
    assert config['args']==['--settings','--cheats']
    task=by_label[config['preLaunchTask']]
    assert '-Release' not in task['args']
    assert 'port/release/build.ps1' in task['args'][-2]
    assert config['program'].endswith('ZenkaiPower.exe' if platform=='Windows' else 'ZenkaiPower')
assert by_label['Build Windows debug']['group']['isDefault']
print('Desktop F5 checks passed: debug tasks, settings/cheats arguments and Windows default.')
