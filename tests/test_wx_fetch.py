"""Offline regression of dependency selection and a real localhost archive fetch.
Synthetic wx targets test the CMake contract, not native Windows GUI compilation.
"""
import functools
import hashlib
import http.server
import pathlib
import subprocess
import sys
import tarfile
import tempfile
import threading

MODULES = pathlib.Path(__file__).resolve().parents[1] / 'cmake'
CMAKE = sys.argv[1] if len(sys.argv) > 1 else 'cmake'


def run(args, ok=True):
    p = subprocess.run([CMAKE, *map(str, args)], stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True, timeout=100)
    if (p.returncode == 0) != ok:
        raise AssertionError(p.stdout)
    return p.stdout


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *_):
        pass


with tempfile.TemporaryDirectory(prefix='dodev-wx-fetch-') as tmp:
    tmp = pathlib.Path(tmp)
    fixture = tmp / 'fixture'
    fixture.mkdir()
    (fixture / 'CMakeLists.txt').write_text('''cmake_minimum_required(VERSION 3.21)
project(FakeWx LANGUAGES CXX)
foreach(c IN ITEMS core base aui stc adv)
    add_library(wx_${c} STATIC dummy.cpp)
    add_library(wx::${c} ALIAS wx_${c})
endforeach()
''')
    (fixture / 'dummy.cpp').write_text('int fake_wx() { return 0; }\n')
    archive = tmp / 'fixture.tar.bz2'
    with tarfile.open(archive, 'w:bz2') as f:
        f.add(fixture, arcname='fixture')
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    project = tmp / 'project'
    project.mkdir()
    (project / 'main.cpp').write_text('int main() { return 0; }\n')
    (project / 'FindwxWidgets.cmake').write_text('''if(TEST_INSTALLED)
    set(wxWidgets_FOUND TRUE)
    set(wxWidgets_VERSION_STRING 3.2.6)
else()
    set(wxWidgets_FOUND FALSE)
endif()
''')
    (project / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.21)
project(FetchProbe LANGUAGES C CXX)
set(WIN32 TRUE)
set(CMAKE_MODULE_PATH "${{CMAKE_CURRENT_SOURCE_DIR}}")
set(CMAKE_DISABLE_FIND_PACKAGE_wxWidgets TRUE)
include("{MODULES.as_posix()}/BuildOptions.cmake")
if(TEST_DOWNLOAD)
    include("{MODULES.as_posix()}/WxFetch.cmake")
    dodev_fetch_wxwidgets("${{TEST_URL}}" "${{TEST_HASH}}")
    add_library(dodev_wx_dependencies INTERFACE)
    target_link_libraries(dodev_wx_dependencies INTERFACE wx::core wx::base wx::aui wx::stc wx::adv)
else()
    if(TEST_INSTALLED)
        set(CMAKE_DISABLE_FIND_PACKAGE_wxWidgets FALSE)
    endif()
    include("{MODULES.as_posix()}/WxDependency.cmake")
endif()
add_executable(probe main.cpp)
target_link_libraries(probe PRIVATE dodev_wx_dependencies)
''')
    out = run(['-S', project, '-B', tmp / 'installed', '-DTEST_INSTALLED=ON'])
    assert 'Provider: installed-module' in out, out
    assert 'using source fallback' not in out, out
    print('PASS: installed package wins; no fetch', flush=True)
    local = tmp / 'local'
    out = run(['-S', project, '-B', local, f'-DFETCHCONTENT_SOURCE_DIR_WXWIDGETS={fixture}'])
    assert 'Provider: fetched-static' in out and 'Using local extracted source' in out, out
    run(['--build', local, '--config', 'Release', '--parallel', '2'])
    print('PASS: offline source fallback and linked targets', flush=True)
    out = run(['-S', project, '-B', tmp / 'disabled', '-DDODEV_FETCH_WXWIDGETS=OFF'], ok=False)
    assert 'Enable source download' in out, out
    print('PASS: disabled fetch diagnostic', flush=True)
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), functools.partial(QuietHandler, directory=tmp))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        url = f'http://127.0.0.1:{server.server_port}/{archive.name}'
        download = tmp / 'download'
        args = ['-S', project, '-B', download, '-DTEST_DOWNLOAD=ON', f'-DTEST_URL={url}', f'-DTEST_HASH={digest}']
        out = run(args)
        assert '100% complete' in out and 'extract' in out.lower() and 'Sources ready' in out, out
        run(['--build', download, '--config', 'Release', '--parallel', '2'])
        print('PASS: HTTP transfer progress, hash verification, extraction and linked build', flush=True)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    out = run(args)
    assert 'Sources ready' in out, out
    print('PASS: reconfigure from cache with server offline', flush=True)
    out = run(['-S', project, '-B', tmp / 'bad-hash', '-DTEST_DOWNLOAD=ON',
               f'-DTEST_URL={archive}', '-DTEST_HASH=' + '0'*64], ok=False)
    assert 'hash' in out.lower(), out
    print('PASS: checksum mismatch rejected', flush=True)
