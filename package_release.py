#!/usr/bin/env python3
from pathlib import Path
import difflib,hashlib,json,re,shutil,subprocess,zipfile
root=Path(__file__).resolve().parent
out=root.parent/'artifacts/wideband-manager-0.4'
fw=root/'firmware-f0/firmware'
out.mkdir(exist_ok=True);(out/'firmware').mkdir(exist_ok=True);(out/'validation').mkdir(exist_ok=True);(out/'licenses').mkdir(exist_ok=True)
def copy(src,dest):shutil.copy2(src,dest)
for name in ['README.md','SETTINGS.md','VERGLEICH.md','FUNKTIONEN.md','RISIKEN.md','rtrim-source-manifest.json']:copy(root/name,out/name)
for src,name in [(fw/'build/wideband_image.bin','app.bin'),(fw/'deliver/f0_module/wideband_image_with_bl.bin','full.bin'),(fw/'build/wideband.elf','app.elf'),(fw/'boards/f0_module/bootloader/build/wideband_bootloader.padded.bin','bootloader.bin'),(fw/'boards/f0_module/bootloader/build/wideband_bootloader.elf','bootloader.elf')]:copy(src,out/'firmware'/('wideband_f0_manager_rtrim_'+name))
objcopy=root.parent/'build-tools/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin/arm-none-eabi-objcopy'
subprocess.run([str(objcopy),'-I','binary','-O','ihex','--change-addresses','0x08000000',str(out/'firmware/wideband_f0_manager_rtrim_full.bin'),str(out/'firmware/wideband_f0_manager_rtrim_full.hex')],check=True)
for src,name in [('/tmp/wb-manager-04-firmware-build.log','firmware-build.log'),('/tmp/wb-manager-04-tests.log','host-tests.log')]:copy(src,out/'validation'/name)
verification=subprocess.check_output(['python3',str(root/'verify_release.py')],text=True)
(out/'validation/image-verification.json').write_text(verification)
(out/'validation/stack-analysis.json').write_text(subprocess.check_output(['python3',str(root/'analyze_stack.py')],text=True))
for path in (fw/'build').glob('*.su'):copy(path,out/'validation'/path.name)
for path in (fw/'build').glob('*.ci'):copy(path,out/'validation'/path.name)
for src,name in [('/tmp/wb-manager-04-o2-check.log','comparison-O2-original-alignment.log'),('/tmp/wb-manager-04-o2-align1-check.log','comparison-O2-small-alignment.log')]:copy(src,out/'validation'/name)
maptext=(fw/'build/wideband.map').read_text();(out/'validation/memory-report.txt').write_text('\n'.join(l for l in maptext.splitlines() if any(k in l for k in ['__heap_base__','__heap_end__','__main_stack_size__','__process_stack_size__','waCanRxThread']))+'\n')
copy(root/'firmware-f0/LICENSE',out/'licenses/wideband-MIT.txt');copy(fw/'ChibiOS/license.txt',out/'licenses/ChibiOS-GPLv3.txt')
# Exact source patch against the selectable baseline, excluding generated firmware.
baseline=root.parent/'epicefi/firmware/controllers/can/wideband_firmware'
changed=['firmware/Makefile','firmware/boards/f0_module/build_wideband.sh','firmware/can.cpp','firmware/main.cpp','firmware/manager_service.cpp','firmware/manager_service.h','firmware/boards/port.h','firmware/boards/f0_module/port.cpp','firmware/sampling.cpp','firmware/lambda_conversion.cpp','firmware/heater_thread.cpp','for_rusefi/wideband_can.h','for_rusefi/wideband_calibration.h','for_rusefi/manager_can.h']
patch=''
for name in changed:
 old=(baseline/name).read_text() if (baseline/name).is_file() else ''
 new=(root/'firmware-f0'/name).read_text()
 patch+=''.join(difflib.unified_diff(old.splitlines(keepends=True),new.splitlines(keepends=True),fromfile='a/'+name if old else '/dev/null',tofile='b/'+name))
(out/'firmware-source.patch').write_text(patch)
manifest={'version':'0.4','target':'f0_module / STM32F042K6','baseline_selectable_commit':'64cdf9f97ff9a1110ce3e7a47eb22ebdebe4bf86','baseline_before_selectable':'27e115b7faa136d7d1b0554e1694322daca983b9','hardware_tested':False,'gui_runtime_tested':False,'image':json.loads(verification),'source_sha256':{name:hashlib.sha256((root/'firmware-f0'/name).read_bytes()).hexdigest() for name in changed}}
(root/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');copy(root/'source-manifest.json',out/'source-manifest.json')
# Dependencies actually used by app and bootloader (headers/sources from .d files),
# plus platform build rules and linker scripts. Preserve original licences.
vendor=set()
for working in [fw,fw/'boards/f0_module/bootloader']:
 for dep in (working/'.dep').glob('*.d'):
  for token in re.split(r'\s+',dep.read_text().replace('\\\n',' ')):
   p=working/token
   if not p.is_file():continue
   resolved=p.resolve()
   for name in ['ChibiOS','libfirmware']:
    base=(fw/name).resolve()
    if resolved.is_relative_to(base):vendor.add((name,str(resolved.relative_to(base))))
for p in (fw/'ChibiOS').rglob('*'):
 if p.is_file() and p.suffix in ['.mk','.ld']:vendor.add(('ChibiOS',str(p.relative_to(fw/'ChibiOS'))))
for p in (fw/'libfirmware').rglob('*'):
 if p.is_file() and not any(x in p.parts for x in ['.git','build','.dep']):vendor.add(('libfirmware',str(p.relative_to(fw/'libfirmware'))))
for name in ['license.txt','readme.txt']:vendor.add(('ChibiOS',name))
(out/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+str(p.relative_to(out))+'\n' for p in sorted(out.rglob('*')) if p.is_file() and p.name!='SHA256SUMS'))
archive=root.parent/'artifacts/rusEFI-Wideband-Manager-0.4-F0-CAN-Rtrim.zip'
skip={'build','.dep','deliver','.git','__pycache__','ChibiOS','libfirmware'}
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
 for p in out.rglob('*'):
  if p.is_file():z.write(p,'rusEFI-Wideband-Manager/'+str(p.relative_to(out)))
 for p in root.rglob('*'):
  rel=p.relative_to(root)
  if not p.is_file() or any(part in skip for part in rel.parts):continue
  if p.name.startswith('wideband_image') or p.suffix in ['.pyc','.bin','.elf','.hex']:continue
  z.write(p,'rusEFI-Wideband-Manager/source/'+str(rel))
 for name,relative in sorted(vendor):z.write(fw/name/relative,'rusEFI-Wideband-Manager/source/firmware-f0/firmware/'+name+'/'+relative)
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
Path(str(archive)+'.sha256').write_text(hashlib.sha256(archive.read_bytes()).hexdigest()+'  '+archive.name+'\n')
print(archive)
print('Vendor source files:',len(vendor),'ZIP bytes:',archive.stat().st_size)
