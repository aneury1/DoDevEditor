#!/usr/bin/env python3
"""Smoke-test CLI detachment without linking or running Raven/Vulkan."""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

launcher=pathlib.Path(sys.argv[1])
desktop=pathlib.Path(sys.argv[2])
text=desktop.read_text(encoding='utf-8')
for fragment in ('Exec=dodeveditor %F','Terminal=false','inode/directory','text/plain'):
    assert fragment in text, fragment

with tempfile.TemporaryDirectory() as tmp:
    root=pathlib.Path(tmp)
    test_launcher=root/'dodeveditor'
    shutil.copy2(launcher,test_launcher)
    test_launcher.chmod(0o755)
    # Stub GUI stays alive for two seconds; launcher must return immediately.
    target=root/'DoDevEditor'
    target.write_text('#!/usr/bin/env python3\n'
                      'import os,sys,time\n'
                      'with open(os.environ["DODEV_LAUNCH_TEST_OUTPUT"],"w") as f:\n'
                      ' f.write("\\n".join(sys.argv[1:]))\n'
                      'print("stub process started", flush=True)\n'
                      'time.sleep(2)\n',encoding='utf-8')
    target.chmod(0o755)
    path=root/'filename with spaces.cpp'
    path.write_text('int main() { return 0; }',encoding='utf-8')
    folder=root/'workspace'; folder.mkdir()
    output=root/'received.txt'
    env=os.environ.copy()
    env['DODEV_LAUNCH_TEST_OUTPUT']=str(output)
    env['XDG_STATE_HOME']=str(root/'state')
    started=time.monotonic()
    run=subprocess.run([str(test_launcher),str(path),str(folder)],
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                       timeout=5,env=env)
    elapsed=time.monotonic()-started
    assert run.returncode==0,(run.returncode,run.stderr)
    assert elapsed<1.5,f'CLI did not detach: {elapsed}'
    for _ in range(40):
        if output.exists():break
        time.sleep(.05)
    assert output.exists(),'Stub GUI never started'
    assert output.read_text().splitlines()==["--foreground","--",str(path),str(folder)]
    log=root/'state'/'DoDevEditor'/'launcher.log'
    for _ in range(40):
        if log.exists(): break
        time.sleep(.05)
    assert log.exists()
    assert 'stub process started' in log.read_text()

    # --foreground waits and reports the GUI process status.
    start=time.monotonic()
    run=subprocess.run([str(test_launcher),'--foreground',str(path)],
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                       timeout=5,env=env)
    assert run.returncode==0
    assert time.monotonic()-start>=1.8
    assert 'stub process started' in run.stdout.decode()
    assert subprocess.run([str(test_launcher),'--help'],capture_output=True).returncode==0
    assert subprocess.run([str(test_launcher),'--bad-option'],capture_output=True).returncode==2
    target.unlink()
    assert subprocess.run([str(test_launcher),str(path)],capture_output=True).returncode==127
print('detached CLI, argv, process logs and .desktop entry OK')
