#!/usr/bin/env python3
"""Package the native desktop app, shared code and test evidence; no firmware."""
from pathlib import Path
import hashlib
import json
import shutil
import tarfile

root=Path(__file__).resolve().parent
out=root.parent/'artifacts/wideband-manager-0.5-linux-x86_64'
out.mkdir(parents=True,exist_ok=True)
for sub in ['linux','src','tests']:(out/sub).mkdir(exist_ok=True)
for p in (root/'linux').iterdir():
    if p.is_file():shutil.copy2(p,out/'linux'/p.name)
for p in (root/'src').iterdir():
    if p.is_file():shutil.copy2(p,out/'src'/p.name)
for name in ['test_linux.py','test_protocol.c','test_analytics.c','test_trace_math.c','run_desktop.sh']:
    shutil.copy2(root/'tests'/name,out/'tests'/name)
shutil.copytree(root/'tests/hal',out/'tests/hal',dirs_exist_ok=True)
shutil.copy2(root/'linux/README.md',out/'README.md')
shutil.copy2(root/'firmware-f0/LICENSE',out/'rusEFI-protocol-LICENSE.txt')
launcher=out/'wideband-manager'
launcher.write_text('#!/usr/bin/env sh\nset -eu\napp_dir=$(CDPATH=\'\' cd -- "$(dirname -- "$0")" && pwd)\nexec "$app_dir/linux/wideband-manager" "$@"\n');launcher.chmod(0o755)
for name in ['preview-demo.png','preview-disconnected.png','preview-tools.png','preview-small.png']:
    shutil.copy2(Path('/tmp/wb-linux-evidence')/name,out/name)
logs=''.join(Path('/tmp/'+n).read_text() for n in ['wb-linux-tests.log','wb-linux-core-tests.log','wb-linux-gui.log'])
assert '\nFAILED' not in logs and 'SMOKE FAILED' not in logs and 'Traceback' not in logs and 'TypeError' not in logs
(out/'validation.txt').write_text(logs)
(out/'manifest.json').write_text(json.dumps({'version':'0.5','platform':'Linux x86_64','language':'English','runtime':'system Python 3 + GTK3/PyGObject/Cairo','transports':['SocketCAN','direct Lawicel SLCAN 115200 8N1'],'firmware_changed':False,'firmware_included':False,'hardware_tested':False,'real_socketcan_interface_tested':False,'existing_firmware_sha256':'9f03e1734c03e658f9715469c975fd756b23b80628ad8ae24b46cbee34855cd3'},indent=2)+'\n')
checks=out/'SHA256SUMS'
checks.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(out).as_posix()+'\n' for p in sorted(out.rglob('*')) if p.is_file() and p!=checks))
archive=out.parent/'rusEFI-Wideband-Manager-0.5-English-Linux-x86_64.tar.gz'
with tarfile.open(archive,'w:gz') as tar:tar.add(out,arcname=out.name)
with tarfile.open(archive,'r:gz') as tar:
    for line in checks.read_text().splitlines():
        expected,path=line.split('  ',1)
        assert hashlib.sha256(tar.extractfile(out.name+'/'+path).read()).hexdigest()==expected
archive.with_suffix('.gz.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
print(archive)
