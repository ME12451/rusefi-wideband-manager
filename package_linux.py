#!/usr/bin/env python3
"""Package the lightweight native GTK application; system runtime, no firmware."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import tarfile

root=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=root.parent/'artifacts')
parser.add_argument('--evidence',type=Path)
args=parser.parse_args()
if not (root/'linux/libwideband.so').is_file():raise SystemExit('Build linux/libwideband.so with bash linux/build.sh first')
folder=args.output/'wideband-manager-0.5.3-linux-x86_64'
if folder.exists():raise SystemExit('Output folder already exists; choose a clean output directory')
folder.mkdir(parents=True)
for name in ('linux','src'):
    shutil.copytree(root/name,folder/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc','*.exe','*.dll'))
(folder/'tests').mkdir()
for name in ('test_linux.py','test_protocol.c','test_analytics.c','test_trace_math.c','run_desktop.sh'):
    shutil.copy2(root/'tests'/name,folder/'tests'/name)
shutil.copytree(root/'tests/hal',folder/'tests/hal')
shutil.copy2(root/'linux/README.md',folder/'README.md')
shutil.copy2(root/'LICENSE',folder/'LICENSE')
shutil.copy2(root/'RELEASE-NATIVE-0.5.3.md',folder/'RELEASE-NATIVE-0.5.3.md')
if args.evidence:shutil.copytree(args.evidence,folder/'validation')
launcher=folder/'wideband-manager'
launcher.write_text('#!/usr/bin/env sh\nset -eu\napp_dir=$(CDPATH=\'\' cd -- "$(dirname -- "$0")" && pwd)\nexec "$app_dir/linux/wideband-manager" "$@"\n')
launcher.chmod(0o755)
(folder/'manifest.json').write_text(json.dumps({'version':'0.5.3','platform':'Linux x86_64','ui':'native GTK3','runtime':'system Python 3 + GTK3/PyGObject/Cairo','bundled_runtime':False,'transports':['SocketCAN','direct SLCAN'],'indexed_configuration':True,'firmware_changed':False,'firmware_included':False,'hardware_tested':False,'firmware_app_sha256':'115fdf2327c34ee9c6c4a42efd5959679e707484614573c498f71ea976b8750c'},indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
checks=folder/'SHA256SUMS'
checks.write_text(''.join(sha(p)+'  '+p.relative_to(folder).as_posix()+'\n' for p in sorted(folder.rglob('*')) if p.is_file() and p!=checks))
archive=args.output/'rusEFI-Wideband-Manager-0.5.3-English-Linux-x86_64.tar.gz'
with tarfile.open(archive,'w:gz') as t:t.add(folder,arcname=folder.name)
with tarfile.open(archive) as t:
    for row in checks.read_text().splitlines():
        expected,name=row.split('  ',1)
        assert hashlib.sha256(t.extractfile(folder.name+'/'+name).read()).hexdigest()==expected
archive.with_name(archive.name+'.sha256').write_text(sha(archive)+'  '+archive.name+'\n')
print(archive)
