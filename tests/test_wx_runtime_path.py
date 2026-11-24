#!/usr/bin/env python3
"""Exercise the CMake helper using an installed ELF with a nonstandard library directory."""
import os, pathlib, subprocess, tempfile, sys
if not sys.platform.startswith('linux'):
    sys.exit(0)
root=pathlib.Path(__file__).resolve().parents[1]
env=os.environ.copy(); env.pop('LD_LIBRARY_PATH',None); env.pop('LD_PRELOAD',None)
def run(*args):
    return subprocess.check_output(args,env=env,stderr=subprocess.STDOUT,text=True)
with tempfile.TemporaryDirectory(prefix='dodev-rpath-') as td:
    p=pathlib.Path(td); lib=p/'custom wx lib'; lib.mkdir(); app=p/'app'; app.mkdir()
    (p/'lib.c').write_text('int wx_fixture(void){return 42;}\n')
    run(os.environ.get('CC','cc'),'-shared','-fPIC','-Wl,-soname,libdodev_wx_fixture.so.0','-o',str(lib/'libdodev_wx_fixture.so.0'),str(p/'lib.c'))
    (lib/'libdodev_wx_fixture.so').symlink_to('libdodev_wx_fixture.so.0')
    (app/'main.c').write_text('extern int wx_fixture(void); int main(void){return wx_fixture()!=42;}\n')
    (app/'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.21)
project(RuntimePathFixture C)
include("{root}/cmake/WxRuntimePath.cmake")
set(wxWidgets_LIBRARY_DIRS "")
set(wxWidgets_LIBRARIES "-L{lib}" -ldodev_wx_fixture)
add_executable(fixture main.c)
target_link_directories(fixture PRIVATE "{lib}")
target_link_libraries(fixture PRIVATE dodev_wx_fixture)
dodev_wx_runtime_path(fixture)
install(TARGETS fixture RUNTIME DESTINATION bin)
''')
    run('cmake','-S',str(app),'-B',str(p/'build'),'-DCMAKE_INSTALL_PREFIX='+str(p/'install'))
    run('cmake','--build',str(p/'build'))
    run('cmake','--install',str(p/'build'))
    run(str(p/'install/bin/fixture'))
    print('Installed executable resolves custom shared library without LD_LIBRARY_PATH')
