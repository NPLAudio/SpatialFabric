"""Build a portable Windows app with bundled Python/Tk and retained runtime notices.
Run using Python 3.13 after installing requirements-build.txt in a virtual environment.
"""
import argparse
import hashlib
import json
from importlib.metadata import distribution
from pathlib import Path
import shutil
import subprocess
import struct
import sys
import tempfile
import tkinter
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent
NAME = 'SpatialFabricOSCController'
TOOLS = [('SpatialFabricDebugger', 'debugger_launch.py'), ('SpatialFabricEngineSimulator', 'simulator_launch.py')]  # console tools, no Tk


def main():
    if sys.platform != 'win32': raise SystemExit('Build the Windows executable on Windows; source runs separately with Python/Tk.')
    if struct.calcsize('P') != 8: raise SystemExit('Use a 64-bit Python interpreter for the Windows-x64 package.')
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'dist', help='Separate versioned output avoids replacing a running older executable')
    output_dir = parser.parse_args().output_dir.resolve(); output_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=NAME + '-build-') as temporary:
        work = Path(temporary)
        notices = work / 'runtime-licenses'; notices.mkdir()
        shutil.copy2(Path(sys.base_prefix) / 'LICENSE.txt', notices / 'Python-LICENSE.txt')
        tcl = Path(tkinter.Tcl().eval('info library'))
        shutil.copy2(tcl.parent / 'tk8.6/license.terms', notices / 'Tk-license.terms')
        tcl_license = tcl / 'license.terms'
        if tcl_license.is_file(): shutil.copy2(tcl_license, notices / 'Tcl-license.terms')
        else:
            # Some Windows Python distributions omit this file. Retain the upstream notice.
            url = 'https://raw.githubusercontent.com/tcltk/tcl/core-8-6-branch/license.terms'
            with urllib.request.urlopen(url, timeout=30) as response:
                (notices / 'Tcl-license.terms').write_bytes(response.read(65536))
        dist = distribution('pyinstaller')
        copying = next(f for f in dist.files if str(f).endswith('/licenses/COPYING.txt'))
        shutil.copy2(dist.locate_file(copying), notices / 'PyInstaller-COPYING.txt')
        subprocess.run([sys.executable, '-m', 'unittest', 'discover', '-s', 'tests', '-v'], cwd=ROOT, check=True)
        subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--windowed',
                        '--name', NAME, '--icon', str(ROOT / 'assets/controller.ico'), '--add-data', str(ROOT / 'assets') + ';assets',
                        '--add-data', str(notices) + ';runtime-licenses', '--add-data', str(ROOT / 'LICENSE') + ';.',
                        '--add-data', str(ROOT / 'THIRD_PARTY_NOTICES.md') + ';.',
                        '--distpath', str(output_dir), '--workpath', str(work / 'work'), '--specpath', str(work),
                        str(ROOT / 'launch.py')], cwd=ROOT, check=True)
        for tool, script in TOOLS:
            subprocess.run([sys.executable, '-m', 'PyInstaller', '--noconfirm', '--clean', '--onefile', '--console', '--name', tool, '--icon', str(ROOT / 'assets/controller.ico'),
                            '--exclude-module', 'tkinter', '--add-data', str(ROOT / 'LICENSE') + ';.', '--distpath', str(output_dir), '--workpath', str(work / ('work-' + tool)),
                            '--specpath', str(work), str(ROOT / script)], cwd=ROOT, check=True)
        portable = output_dir / NAME; portable.mkdir(parents=True, exist_ok=True)
        shutil.copy2(output_dir / (NAME + '.exe'), portable)
        for tool, _ in TOOLS: shutil.copy2(output_dir / (tool + '.exe'), portable)
        for name in ('README.md', 'LICENSE', 'THIRD_PARTY_NOTICES.md'): shutil.copy2(ROOT / name, portable)
        shutil.copytree(ROOT / 'profiles', portable / 'profiles', dirs_exist_ok=True)
        shutil.copytree(notices, portable / 'runtime-licenses', dirs_exist_ok=True)
        archive = output_dir / (NAME + '-Windows-x64.zip')
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as output:
            for path in sorted(portable.rglob('*')):
                if path.is_file(): output.write(path, path.relative_to(portable.parent))
        artifacts = [output_dir / (NAME + '.exe'), *(output_dir / (tool + '.exe') for tool, _ in TOOLS), archive]
        sums = [{'name': a.name, 'bytes': a.stat().st_size, 'sha256': hashlib.sha256(a.read_bytes()).hexdigest()} for a in artifacts]
        version = next(line for line in (ROOT / 'osc_controller/__init__.py').read_text().splitlines() if line.startswith('__version__')).split("'")[1]
        (output_dir / 'SHA256SUMS.txt').write_text(''.join(f"{s['sha256']}  {s['name']}\n" for s in sums), encoding='utf-8')
        (output_dir / 'BUILD_MANIFEST.json').write_text(json.dumps({'version': version, 'platform': 'Windows x64', 'signed': False, 'artifacts': sums}, indent=2) + '\n', encoding='utf-8')
        print('\nExecutable:', output_dir / (NAME + '.exe'))
        for tool, _ in TOOLS: print('Console tool:', output_dir / (tool + '.exe'))
        print('Portable ZIP:', archive)


if __name__ == '__main__': main()
