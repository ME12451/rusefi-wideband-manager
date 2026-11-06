#!/usr/bin/env python3
"""Build a self-contained Linux folder using the current Python environment."""
from pathlib import Path
import os
import subprocess
import sys

qt=Path(__file__).resolve().parent
root=qt.parent.parent
out=Path(os.environ.get('WB_QT_DIST',root/'artifacts/qt06-linux-build'))
work=Path(os.environ.get('WB_QT_BUILD',root/'build-tools/wideband-qt06/pyinstaller-work'))
cache=Path(os.environ.get('PYINSTALLER_CONFIG_DIR',root/'build-tools/wideband-qt06/pyinstaller-cache'))
subprocess.run([str(qt/'build_core.sh')],check=True)
env=dict(os.environ,PYINSTALLER_CONFIG_DIR=str(cache))
# Some Linux environments keep optional XCB libraries outside the system paths.
# Bundle those explicitly when building a portable X11 release there.
extra=[]
if os.environ.get('WB_QT_XCB_LIB_DIR'):
    folder=Path(os.environ['WB_QT_XCB_LIB_DIR'])
    for name in ('libxcb-image.so.0','libxcb-keysyms.so.1','libxcb-icccm.so.4','libxcb-cursor.so.0','libxcb-render-util.so.0'):
        path=folder/name
        if not path.is_file():raise FileNotFoundError(path)
        extra.extend(['--add-binary',str(path)+':.'])
subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir','--name','rusEFI-Wideband-Manager','--distpath',str(out),'--workpath',str(work),'--specpath',str(work.parent),'--paths',str(qt),'--add-binary',str(qt/'libwideband.so')+':.','--hidden-import','can.interfaces.pcan','--hidden-import','can.interfaces.socketcan','--hidden-import','can.interfaces.virtual','--copy-metadata','python-can',*extra,str(qt/'launch.py')],env=env,check=True)
