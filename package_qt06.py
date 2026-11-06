#!/usr/bin/env python3
"""Create checked Windows/Linux 0.6 archives and one common-source archive."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tarfile
import zipfile

ROOT=Path(__file__).resolve().parent
WORK=ROOT.parent
ART=WORK/'artifacts'
QT=ROOT/'qt'
APP_FILES=('launch.py','main.py','actions.py','backend.py','transports.py','graph.py','i18n.py','smoke.py')
FIRMWARE_SHA='9f03e1734c03e658f9715469c975fd756b23b80628ad8ae24b46cbee34855cd3'

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(ROOT/'firmware-f0/firmware/build/wideband_image.bin')==FIRMWARE_SHA
source_hashes={str(p.relative_to(ROOT)):sha(p) for p in [*(QT/n for n in APP_FILES),QT/'core.c',*(ROOT/'src'/n for n in ['protocol.h','service_protocol.h','analytics.h','trace_math.h'])]}
source_id=hashlib.sha256(json.dumps(source_hashes,sort_keys=True).encode()).hexdigest()

linux=ART/'wideband-manager-0.6-linux-x86_64'
linux.mkdir(exist_ok=True)
shutil.copytree(ART/'qt06-linux-build/rusEFI-Wideband-Manager',linux,dirs_exist_ok=True)
windows=ART/'wideband-manager-0.6-windows-x64'
for name in APP_FILES:assert sha(windows/'app'/name)==sha(QT/name),name
assert sha(linux/'_internal/libwideband.so')==sha(QT/'libwideband.so')
assert sha(windows/'app/libwideband.dll')==sha(QT/'libwideband.dll')

source=ART/'wideband-manager-0.6-common-source'
source.mkdir(exist_ok=True)
for out,platform,evidence in [(windows,'Windows x64','/tmp/wb-qt06-win-demo'),(linux,'Linux x86_64','/tmp/wb-qt06-native-demo'),(source,'Common source',None)]:
    for name in ['README-DE.md','README-EN.md']:shutil.copy2(QT/name,out/name)
    shutil.copytree(QT/'licenses',out/'licenses',dirs_exist_ok=True)
    shutil.copy2(ROOT/'firmware-f0/LICENSE',out/'licenses/rusEFI-LICENSE.txt')
    src=out/'source';(src/'qt').mkdir(parents=True,exist_ok=True);(src/'src').mkdir(exist_ok=True);(src/'tests').mkdir(exist_ok=True)
    for path in QT.iterdir():
        if path.is_file() and path.suffix not in ('.dll','.so'):shutil.copy2(path,src/'qt'/path.name)
    shutil.copytree(QT/'licenses',src/'qt/licenses',dirs_exist_ok=True)
    for path in (ROOT/'src').glob('*.h'):shutil.copy2(path,src/'src'/path.name)
    for name in ['test_qt_backend.py','test_qt_i18n.py','test_protocol.c','test_analytics.c','test_trace_math.c','run_desktop.sh']:shutil.copy2(ROOT/'tests'/name,src/'tests'/name)
    shutil.copytree(ROOT/'tests/hal',src/'tests/hal',dirs_exist_ok=True)
    shutil.copy2(ROOT/'package_qt06.py',src/'package_qt06.py')
    validation=out/'validation';validation.mkdir(exist_ok=True)
    for name in ['wb-qt06-backend-tests.log','wb-qt06-i18n-tests.log','wb-qt06-core-tests.log']:
        text=Path('/tmp',name).read_text();assert 'FAILED' not in text and 'Traceback' not in text;shutil.copy2(Path('/tmp',name),validation/name)
    for prefix in ['win','native']:
        for mode in ['demo','offline']:
            folder=Path('/tmp')/f'wb-qt06-{prefix}-{mode}';report=folder/f'gui-{mode}.json';data=json.loads(report.read_text());assert data['passed'];shutil.copy2(report,validation/f'{prefix}-{report.name}')
    shutil.copy2(Path('/tmp/wb-qt06-linux-abi.json'),validation/'linux-abi.json')
    if evidence:
        for name in ['demo-de.png','demo-en.png','tools-de.png','small-de.png']:shutil.copy2(Path(evidence)/name,out/('preview-'+name))
    # Retain third-party license files from installed pinned dependency wheels.
    libs=WORK/'build-tools/wideband-qt06/linux312-libs'
    for folder in libs.glob('*.dist-info'):
        if folder.name.startswith(('setuptools','altgraph')):continue
        for path in folder.rglob('*'):
            if path.is_file() and any(word in path.name.lower() for word in ('license','copying','notice')):
                target=out/'licenses/python-packages'/folder.name/path.relative_to(folder);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,target)
    shutil.copy2(WORK/'build-tools/wideband-qt06/python312/python/lib/python3.12/LICENSE.txt',out/'licenses/CPython-LICENSE.txt')
    metadata={'version':'0.6','platform':platform,'languages':['English','Deutsch'],'shared_source_id':source_id,'shared_source_sha256':source_hashes,'firmware_changed':False,'firmware_included':False,'existing_firmware_sha256':FIRMWARE_SHA,'hardware_tested':False,'dependencies':{'PySide6-Essentials':'6.10.2','python-can':'4.6.1','pyserial':'3.5'},'validation':{'Windows':'Actual EXE under WineHQ 11.21/Xvfb','Linux':'Packaged Qt executable on Ubuntu 26.04.1/X11/Xvfb','gui_checks_per_platform':{'demo':28,'offline':18},'module':'Actual F0 dispatcher with fake HAL; no physical CAN hardware'},'linux_binary_minimum_glibc':'2.43','windows_runtime':'CPython 3.12.10 embeddable','linux_runtime':'CPython 3.12.16 python-build-standalone 20261026'}
    (out/'manifest.json').write_text(json.dumps(metadata,indent=2)+'\n')
    checks=out/'SHA256SUMS'
    checks.write_text(''.join(sha(path)+'  '+path.relative_to(out).as_posix()+'\n' for path in sorted(out.rglob('*')) if path.is_file() and path!=checks and '__pycache__' not in path.parts))

def zip_package(folder,name):
    path=ART/name
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for file in sorted(folder.rglob('*')):
            if file.is_file() and '__pycache__' not in file.parts:archive.write(file,file.relative_to(ART))
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        for row in (folder/'SHA256SUMS').read_text().splitlines():
            expected,name=row.split('  ',1);assert hashlib.sha256(archive.read(folder.name+'/'+name)).hexdigest()==expected
    path.with_suffix(path.suffix+'.sha256').write_text(sha(path)+'  '+path.name+'\n');print(path)

zip_package(windows,'rusEFI-Wideband-Manager-0.6-Windows-x64.zip')
zip_package(source,'rusEFI-Wideband-Manager-0.6-Common-Source.zip')
path=ART/'rusEFI-Wideband-Manager-0.6-Linux-x86_64.tar.gz'
with tarfile.open(path,'w:gz',compresslevel=6) as archive:archive.add(linux,arcname=linux.name,filter=lambda info:None if '/__pycache__/' in info.name else info)
with tarfile.open(path,'r:gz') as archive:
    for row in (linux/'SHA256SUMS').read_text().splitlines():
        expected,name=row.split('  ',1);assert hashlib.sha256(archive.extractfile(linux.name+'/'+name).read()).hexdigest()==expected
path.with_suffix('.gz.sha256').write_text(sha(path)+'  '+path.name+'\n');print(path)
print('Shared source:',source_id)
