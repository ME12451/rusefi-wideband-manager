#!/usr/bin/env python3
"""Package the desktop-only 0.5.2 release; never rebuild or bundle firmware."""
from pathlib import Path
import hashlib
import json
import shutil
import zipfile

root = Path(__file__).resolve().parent
out = root.parent / 'artifacts/wideband-manager-0.5.2'
exe = out / 'rusEFI-Wideband-Manager.exe'
assert exe.is_file(), 'Run build.sh first'
assert not list(out.glob('*.dll')), 'Never ship the fake PCAN DLL'
assert exe.stat().st_size < 1024*1024, 'Keep the native desktop executable small'
shutil.copy2(root / 'README-DESKTOP.md', out / 'README.md')
source = out / 'source'
(source / 'tests').mkdir(parents=True, exist_ok=True)
(source / 'src').mkdir(exist_ok=True)
for p in (root / 'src').iterdir():
    if p.is_file(): shutil.copy2(p, source / 'src' / p.name)
for name in ['build.sh', 'README-DESKTOP.md', 'package_desktop.py']:
    shutil.copy2(root / name, source / name)
for name in ['run_desktop.sh', 'test_protocol.c', 'test_analytics.c', 'test_trace_math.c', 'gui_smoke.c', 'gui_regression_051.c', 'gui_layout_052.c', 'gui_can_052.c', 'fake_pcan.c', 'test_client_flow.py', 'test_service.py']:
    shutil.copy2(root / 'tests' / name, source / 'tests' / name)
shutil.copytree(root / 'tests/hal', source / 'tests/hal', dirs_exist_ok=True)
shutil.copy2(root / 'firmware-f0/LICENSE', out / 'rusEFI-protocol-LICENSE.txt')
manifest = {'version': '0.5.2', 'language': 'English', 'platform': 'Windows x64', 'firmware_included': False,
 'firmware_changed': False, 'exe_sha256': hashlib.sha256(exe.read_bytes()).hexdigest(),
 'existing_firmware_0_4_sha256': '9f03e1734c03e658f9715469c975fd756b23b80628ad8ae24b46cbee34855cd3',
 'validation': ['warning-clean Windows cross-build', 'ASAN/UBSAN desktop tests',
 'actual F0 service/client regression tests', 'Wine GUI smoke including pause, history and CSV',
 'normal disconnected startup', '1080p layout at 96/120/144 DPI without main scrollbars; normal half-area popup; console view changes, Escape and Back under fake CAN traffic', 'test-only fake-PCAN duplicate HW identities and Read F0 field update checks'], 'limitations': ['no physical CAN hardware tests', 'no native Windows multi-monitor DPI test']}
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
checks = out / 'SHA256SUMS'
checks.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(out).as_posix() + '\n' for p in sorted(out.rglob('*')) if p.is_file() and p != checks))
archive = out.parent / 'rusEFI-Wideband-Manager-0.5.2-English-Windows.zip'
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
    for p in sorted(out.rglob('*')):
        if p.is_file(): z.write(p, p.relative_to(out.parent))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    for line in checks.read_text().splitlines():
        expected, path = line.split('  ', 1)
        assert hashlib.sha256(z.read(out.name + '/' + path)).hexdigest() == expected
archive.with_suffix('.zip.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + '  ' + archive.name + '\n')
print(archive)
print(manifest['exe_sha256'])
