"""Shared packaged entry point, with a startup log for Windows GUI launches."""
import os
from pathlib import Path
import sys
import traceback

def run():
    if sys.stdout is None or sys.stderr is None:
        base=Path(os.environ.get('LOCALAPPDATA') or os.environ.get('XDG_STATE_HOME') or Path.home()/'.local/state')
        folder=base/'rusEFI'/'WidebandManager06';folder.mkdir(parents=True,exist_ok=True)
        stream=open(folder/'session.log','w',encoding='utf-8',buffering=1)
        if sys.stdout is None:sys.stdout=stream
        if sys.stderr is None:sys.stderr=stream
    try:
        from main import main
        return main()
    except Exception:
        detail=traceback.format_exc();print(detail,file=sys.stderr,flush=True)
        if os.name=='nt' and '--smoke-test' not in sys.argv:
            import ctypes
            ctypes.windll.user32.MessageBoxW(None,'Unable to start rusEFI Wideband Manager 0.6.\n\n'+detail,'rusEFI Wideband Manager 0.6',0x10)
        return 1
if __name__=='__main__':raise SystemExit(run())
