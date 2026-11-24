import os, pathlib, subprocess, sys, tempfile, time
exe = str(pathlib.Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as temp:
    root=pathlib.Path(temp)
    env=dict(os.environ,XDG_STATE_HOME=temp)
    path=root/'file with spaces.cpp'
    start=time.monotonic()
    r=subprocess.run([exe,str(path)],capture_output=True,env=env,timeout=5)
    assert r.returncode==0 and time.monotonic()-start<1.5
    log=root/'DoDevEditor'/'launcher.log'
    for _ in range(100):
        if log.exists() and 'GUI callback started' in log.read_text():break
        time.sleep(.05)
    assert str(path) in log.read_text()
    assert 'GUI callback started' in log.read_text()
    for flag in ('--foreground','--wait'):
        r=subprocess.run([exe,flag,str(path)],capture_output=True,env=env,timeout=5)
        assert r.returncode==17 and str(path) in r.stdout.decode()
    assert subprocess.run([exe,'--help'],capture_output=True).returncode==0
print('Direct executable detachment and foreground exit propagation OK')
