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
subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir','--name','rusEFI-Wideband-Manager','--distpath',str(out),'--workpath',str(work),'--specpath',str(work.parent),'--paths',str(qt),'--add-binary',str(qt/'libwideband.so')+':.','--hidden-import','can.interfaces.pcan','--hidden-import','can.interfaces.socketcan','--hidden-import','can.interfaces.virtual','--copy-metadata','python-can',str(qt/'launch.py')],env=env,check=True)
