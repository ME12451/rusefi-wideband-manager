#!/usr/bin/env python3
"""Build a portable Windows folder from an official Python ZIP and pinned wheels.
Runs on Windows with MinGW-w64, or on Linux with a MinGW-w64 cross compiler.
No Python or Qt installation is required on the target PC.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import zipfile

APP_FILES=('launch.py','main.py','actions.py','backend.py','transports.py','graph.py','i18n.py','smoke.py')

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--python-zip',required=True,type=Path)
    parser.add_argument('--wheels',required=True,type=Path)
    parser.add_argument('--compiler',default='x86_64-w64-mingw32-gcc')
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args();qt=Path(__file__).resolve().parent;out=args.output.resolve()
    (out/'app').mkdir(parents=True,exist_ok=True);site=out/'runtime/Lib/site-packages';site.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.python_zip) as archive:archive.extractall(out/'runtime')
    for wheel in sorted(args.wheels.glob('*.whl')):
        with zipfile.ZipFile(wheel) as archive:archive.extractall(site)
    pth=list((out/'runtime').glob('python*._pth'))
    if len(pth)!=1:raise RuntimeError('Expected one embedded Python path file; use a clean output folder')
    python_zip=next(p for p in (out/'runtime').glob('python*.zip'))
    pth[0].write_text(python_zip.name+'\n.\nLib/site-packages\n../app\nimport site\n')
    for name in APP_FILES:shutil.copy2(qt/name,out/'app'/name)
    subprocess.run([args.compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-shared','-static-libgcc',str(qt/'core.c'),'-lm','-o',str(out/'app/libwideband.dll')],check=True)
    subprocess.run([args.compiler,'-std=c11','-O2','-Wall','-Wextra','-Werror','-static','-mwindows','-municode',str(qt/'windows_launcher.c'),'-o',str(out/'rusEFI-Wideband-Manager.exe')],check=True)
    print(out)
if __name__=='__main__':main()
