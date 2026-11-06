#!/usr/bin/env python3
"""Package the native 0.5.3 manager with the indexed F0 firmware.

Run builds and the documented tests first. GUI evidence is read from the
isolated test workspace; no fake adapter binary is ever copied to the release.
Older release directories and archives are never modified.
"""
from pathlib import Path
import difflib
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent
FW = ROOT / 'firmware-f0/firmware'
OUT = ROOT.parent / 'artifacts/wideband-manager-0.5.3'
BASELINE = ROOT.parent / 'build-tools/wb053-baseline'
ARCHIVE = OUT.parent / 'rusEFI-Wideband-Manager-0.5.3-English-Windows-F0.zip'
PREFIX = OUT.name + '/'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def copy(src, dest):
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)


def elf_layout(path):
    data = path.read_bytes()
    h = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    sections = [struct.unpack_from('<IIIIIIIIII', data, h[6] + i * h[11])
                for i in range(h[12])]
    strings = sections[h[13]]
    names = data[strings[4]:strings[4] + strings[5]]
    sizes = {names[s[0]:].split(b'\0', 1)[0].decode(): s[5] for s in sections}
    symbols = {}
    for s in sections:
        if s[1] != 2:
            continue
        names_section = sections[s[6]]
        table = data[names_section[4]:names_section[4] + names_section[5]]
        for offset in range(s[4], s[4] + s[5], s[9]):
            name, value, size, *_ = struct.unpack_from('<IIIBBH', data, offset)
            symbols[table[name:].split(b'\0', 1)[0].decode()] = (value, size)
    end = max(p + filesz for i in range(h[10])
              for typ, off, v, p, filesz, memsz, flags, align
              in [struct.unpack_from('<IIIIIIII', data, h[5] + i * h[9])]
              if typ == 1 and filesz and 0x08001800 <= p < 0x08007c00)
    return {'flash_used': end - 0x08001800,
            'free_before_crc': 0x08007bfc - end,
            'data_section': sizes['.data'], 'bss_section': sizes['.bss'],
            'heap_free': symbols['__heap_end__'][0] - symbols['__heap_base__'][0],
            'main_stack': symbols['__main_stack_size__'][0],
            'process_stack': symbols['__process_stack_size__'][0],
            'can_rx_working_area': symbols['_ZL13waCanRxThread'][1]}


exe = OUT / 'rusEFI-Wideband-Manager.exe'
assert exe.is_file() and exe.stat().st_size < 512 * 1024
assert not list(OUT.rglob('*.dll')), 'Do not ship a compiled fake PCAN DLL'
assert (ROOT / 'src/service_protocol.h').read_bytes() == (
    ROOT / 'firmware-f0/for_rusefi/manager_can.h').read_bytes()

image = json.loads(subprocess.check_output(['python3', str(ROOT / 'verify_release.py')], text=True))
assert image['bootloader_sha256'] == '900a987bf46bb1af098845a286ebbe8451d983d81441a93de34985d9976ee4e2'
assert image['app_sha256'] == '115fdf2327c34ee9c6c4a42efd5959679e707484614573c498f71ea976b8750c'
before, after = elf_layout(BASELINE / 'baseline.elf'), elf_layout(FW / 'build/wideband.elf')
assert after['flash_used'] - before['flash_used'] == 60
for resource in ['data_section', 'bss_section', 'heap_free', 'main_stack', 'process_stack', 'can_rx_working_area']:
    assert before[resource] == after[resource], resource

for name in ['README-DESKTOP.md', 'RELEASE-0.5.3.md', 'SETTINGS.md']:
    copy(ROOT / name, OUT / name)
copy(ROOT / 'RELEASE-0.5.3.md', OUT / 'README.md')
for name in ['FUNKTIONEN.md', 'RISIKEN.md', 'VERGLEICH.md']:
    copy(ROOT / name, OUT / 'historical' / name)
    historical = OUT / 'historical' / name
    historical.write_text(historical.read_text().replace('(RELEASE-0.5.3.md)', '(../RELEASE-0.5.3.md)'))
for name, target in [
    ('build/wideband_image.bin', 'app.bin'),
    ('deliver/f0_module/wideband_image_with_bl.bin', 'full.bin'),
    ('build/wideband.elf', 'app.elf'),
]:
    copy(FW / name, OUT / 'firmware' / ('wideband_f0_manager_indexed_' + target))
objcopy = ROOT.parent / 'build-tools/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-objcopy'
full_bin = OUT / 'firmware/wideband_f0_manager_indexed_full.bin'
full_hex = full_bin.with_suffix('.hex')
subprocess.run([str(objcopy), '-I', 'binary', '-O', 'ihex', '--change-addresses', '0x08000000', str(full_bin), str(full_hex)], check=True)
# Verify every HEX data byte and that the config page is not part of the file.
address = 0
hex_bytes = {}
for line in full_hex.read_text().splitlines():
    record = bytes.fromhex(line[1:])
    assert line[0] == ':' and sum(record) % 256 == 0
    n, kind = record[0], record[3]
    offset = int.from_bytes(record[1:3], 'big')
    if kind == 2:
        address = int.from_bytes(record[4:4+n], 'big') << 4
    elif kind == 4:
        address = int.from_bytes(record[4:4+n], 'big') << 16
    elif kind == 0:
        for i, value in enumerate(record[4:4+n]):
            absolute = address + offset + i
            assert absolute not in hex_bytes and 0x08000000 <= absolute < 0x08007c00
            hex_bytes[absolute] = value
assert len(hex_bytes) == full_bin.stat().st_size
assert bytes(hex_bytes[a] for a in range(0x08000000, 0x08007c00)) == full_bin.read_bytes()

validation = OUT / 'validation'
validation.mkdir(exist_ok=True)
(validation / 'image-verification.json').write_text(json.dumps(image, indent=2) + '\n')
(validation / 'memory-comparison.json').write_text(json.dumps({'firmware_0_4': before, 'indexed': after}, indent=2) + '\n')
(validation / 'stack-analysis.json').write_text(subprocess.check_output(['python3', str(ROOT / 'analyze_stack.py')], text=True))
evidence = {
    '/tmp/wb053-fw-build.log': 'firmware-build.log',
    '/tmp/wb053-full-tests.log': 'firmware-client-host-tests.log',
    '/tmp/wb053-core-tests.log': 'portable-desktop-tests.log',
    '/tmp/wb-052-indexed053.log': 'gui-indexed-053.log',
    '/tmp/wb-053-dpi-96.log': 'gui-layout-96-dpi.log',
    '/tmp/wb-053-dpi-120.log': 'gui-layout-120-dpi.log',
    '/tmp/wb-053-dpi-144.log': 'gui-layout-144-dpi.log',
    '/tmp/wb-053-graph.log': 'gui-graph.log',
}
for src, target in evidence.items():
    text = Path(src).read_text()
    assert 'passed' in text or 'PASS' in text or target == 'firmware-build.log', src
    assert 'Assertion failed' not in text and 'FAILED' not in text, src
    copy(Path(src), validation / target)
for path in (FW / 'build').glob('*.su'):
    copy(path, validation / path.name)
for path in (FW / 'build').glob('*.ci'):
    copy(path, validation / path.name)
map_text = (FW / 'build/wideband.map').read_text()
(validation / 'memory-symbols.txt').write_text('\n'.join(
    line for line in map_text.splitlines() if any(word in line for word in
        ['__heap_base__', '__heap_end__', '__main_stack_size__', '__process_stack_size__', 'waCanRxThread'])) + '\n')
for name in ['target0', 'target1']:
    copy(Path('/tmp/wb053-' + name + '.png'), OUT / ('preview-fake-CAN-' + name + '.png'))

fw_changes = ['firmware/manager_service.cpp', 'firmware/boards/f0_module/port.cpp', 'for_rusefi/manager_can.h']
patch = ''
for name in fw_changes:
    old = (BASELINE / 'firmware-f0' / name).read_text()
    new = (ROOT / 'firmware-f0' / name).read_text()
    patch += ''.join(difflib.unified_diff(old.splitlines(keepends=True), new.splitlines(keepends=True), fromfile='a/' + name, tofile='b/' + name))
(OUT / 'firmware-indexed.patch').write_text(patch)
# Confirm the preserved 0.4 source archive differs in exactly these three files.
with zipfile.ZipFile(OUT.parent / 'rusEFI-Wideband-Manager-0.4-F0-CAN-Rtrim.zip') as z:
    source_prefix = 'rusEFI-Wideband-Manager/source/firmware-f0/'
    differences = []
    for name in z.namelist():
        if not name.startswith(source_prefix):
            continue
        rel = name[len(source_prefix):]
        if any(part in ['ChibiOS', 'libfirmware'] for part in Path(rel).parts):
            continue
        current = ROOT / 'firmware-f0' / rel
        assert current.is_file(), rel
        if z.read(name) != current.read_bytes():
            differences.append(rel)
    assert set(differences) == set(fw_changes), differences

# Collect the dependency subset used by application and bootloader, preserving licenses.
vendor = set()
for working in [FW, FW / 'boards/f0_module/bootloader']:
    for dep in (working / '.dep').glob('*.d'):
        for token in re.split(r'\s+', dep.read_text().replace('\\\n', ' ')):
            path = working / token
            if not path.is_file():
                continue
            resolved = path.resolve()
            for name in ['ChibiOS', 'libfirmware']:
                base = (FW / name).resolve()
                if resolved.is_relative_to(base):
                    vendor.add((name, str(resolved.relative_to(base))))
for path in (FW / 'ChibiOS').rglob('*'):
    if path.is_file() and path.suffix in ['.mk', '.ld']:
        vendor.add(('ChibiOS', str(path.relative_to(FW / 'ChibiOS'))))
for path in (FW / 'libfirmware').rglob('*'):
    if path.is_file() and not any(part in ['.git', 'build', '.dep'] for part in path.parts):
        vendor.add(('libfirmware', str(path.relative_to(FW / 'libfirmware'))))
for name in ['license.txt', 'readme.txt']:
    vendor.add(('ChibiOS', name))
copy(ROOT / 'firmware-f0/LICENSE', OUT / 'licenses/wideband-MIT.txt')
copy(FW / 'ChibiOS/license.txt', OUT / 'licenses/ChibiOS-GPLv3.txt')

source = OUT / 'source'
for directory in ['src', 'tests', 'firmware-f0']:
    for path in (ROOT / directory).rglob('*'):
        rel = path.relative_to(ROOT)
        if not path.is_file() or any(part in ['.git', 'build', '.dep', 'deliver', '__pycache__', 'ChibiOS', 'libfirmware'] for part in rel.parts):
            continue
        if path.name.startswith('wideband_image') or path.suffix in ['.pyc', '.exe', '.dll', '.bin', '.elf', '.hex', '.dfu']:
            continue
        if directory == 'tests' and path.name.startswith('gui_') and path.name not in ['gui_indexed_053.c', 'gui_layout_053.c', 'gui_smoke.c']:
            continue
        copy(path, source / rel)
for name, relative in sorted(vendor):
    copy(FW / name / relative, source / 'firmware-f0/firmware' / name / relative)
for name in ['README.md', 'README-DESKTOP.md', 'RELEASE-0.5.3.md', 'SETTINGS.md', 'build.sh', 'build-firmware.sh', 'prepare-firmware-deps.sh', 'verify_release.py', 'analyze_stack.py', 'package_indexed.py', 'rtrim-source-manifest.json']:
    copy(ROOT / name, source / name)
copy(ROOT / 'tools/crc32', source / 'tools/crc32')
for name in ['FUNKTIONEN.md', 'RISIKEN.md', 'VERGLEICH.md']:
    copy(ROOT / name, source / name)
copy(ROOT / 'docs/historical/README-0.4.md', source / 'docs/historical/README-0.4.md')

manifest = {
    'version': '0.5.3', 'ui_language': 'English', 'platform': 'native Windows x64',
    'target': 'STM32F042K6 / f0_module / CAN only',
    'service_version': 2, 'legacy_service_retained_in_firmware': True,
    'manager_legacy_configuration_fallback': False,
    'exe_size': exe.stat().st_size, 'exe_sha256': sha(exe),
    'app_image': image, 'memory_before': before, 'memory_after': after,
    'hardware_tested': False, 'hardware_flashed': False, 'gui_simulation_tested': True,
    'changed_firmware_files': fw_changes,
    'dependencies': {'ChibiOS': 'a8865dc84b114e74f62d8e470458839da71d333b', 'libfirmware': '39b3a04dab617e7ff24577e99d0d7fac2ce44c9f'},
    'source_sha256': {str(p.relative_to(source)): sha(p) for p in sorted(source.rglob('*')) if p.is_file()},
}
(OUT / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
copy(OUT / 'source-manifest.json', ROOT / 'source-manifest.json')
checks = OUT / 'SHA256SUMS'
checks.write_text(''.join(sha(p) + '  ' + str(p.relative_to(OUT)) + '\n' for p in sorted(OUT.rglob('*')) if p.is_file() and p != checks))
with zipfile.ZipFile(ARCHIVE, 'w', zipfile.ZIP_DEFLATED) as z:
    for path in sorted(OUT.rglob('*')):
        if path.is_file():
            z.write(path, PREFIX + str(path.relative_to(OUT)))
with zipfile.ZipFile(ARCHIVE) as z:
    assert z.testzip() is None
    assert not any(n.lower().endswith('.dll') for n in z.namelist())
    for line in checks.read_text().splitlines():
        expected, name = line.split('  ', 1)
        assert hashlib.sha256(z.read(PREFIX + name)).hexdigest() == expected, name
ARCHIVE.with_suffix('.zip.sha256').write_text(sha(ARCHIVE) + '  ' + ARCHIVE.name + '\n')
print(json.dumps({'archive': str(ARCHIVE), 'archive_bytes': ARCHIVE.stat().st_size,
                  'exe_bytes': exe.stat().st_size, 'vendor_source_files': len(vendor),
                  'memory_before': before, 'memory_after': after}, indent=2))
