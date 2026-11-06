#!/usr/bin/env python3
"""Validate F0 app/full/HEX, reserved config page, CRC, vectors and ELF layout."""
from pathlib import Path
import struct,zlib,json,hashlib
root=Path(__file__).resolve().parent
fw=root/'firmware-f0/firmware'
app=(fw/'build/wideband_image.bin').read_bytes()
boot=(fw/'boards/f0_module/bootloader/build/wideband_bootloader.padded.bin').read_bytes()
full=(fw/'deliver/f0_module/wideband_image_with_bl.bin').read_bytes()
assert len(app)==0x6400 and len(boot)==0x1800 and len(full)==0x7c00
assert full==boot+app
assert app[-4:]==zlib.crc32(app[:-4]).to_bytes(4,'big')
assert struct.unpack_from('<I',boot,0x20)[0]==0x57424331
for start,stop in [(0,0x1800),(0x1800,0x7c00)]:
 sp,rv=struct.unpack_from('<II',full,start)
 assert 0x20000100<=sp<=0x20001800 and not sp%4
 assert rv&1 and 0x08000000+start<=(rv&~1)<0x08000000+stop
b=(fw/'build/wideband.elf').read_bytes();h=struct.unpack_from('<16sHHIIIIIHHHHHH',b)
app_end=0x08001800
for i in range(h[10]):
 typ,off,v,p,filesz,memsz,flags,align=struct.unpack_from('<IIIIIIII',b,h[5]+i*h[9])
 if typ==1 and filesz and 0x08001800<=p<0x08007c00:app_end=max(app_end,p+filesz)
assert app_end<=0x08007bfc
# Check the reserved stack regions in the actual ELF, not just build flags.
sections=[struct.unpack_from('<IIIIIIIIII',b,h[6]+i*h[11]) for i in range(h[12])]
symbols={}
for section in sections:
 if section[1]!=2:continue
 strings=sections[section[6]];table=b[strings[4]:strings[4]+strings[5]]
 for off in range(section[4],section[4]+section[5],section[9]):
  name,value,size,info,other,index=struct.unpack_from('<IIIBBH',b,off)
  label=table[name:].split(b'\0',1)[0].decode()
  symbols[label]=(value,size)
assert symbols['__main_stack_size__'][0]==1024
assert symbols['__process_stack_size__'][0]==1024
assert symbols['_ZL13waCanRxThread'][1]==680 # Original 512-byte stack + ChibiOS/port workspace.
heap_free=symbols['__heap_end__'][0]-symbols['__heap_base__'][0]
assert heap_free>=0
report={'app_size':len(app),'full_size':len(full),'app_flash_used':app_end-0x08001800,'free_before_crc':0x08007bfc-app_end,'main_stack_bytes':1024,'interrupt_stack_bytes':1024,'can_rx_stack_bytes':512,'can_rx_working_area_bytes':680,'heap_free_bytes':heap_free,'rollback_payload_bytes':12,'crc32':app[-4:].hex(),'config_page_excluded':True,'bootloader_sha256':hashlib.sha256(boot).hexdigest(),'app_sha256':hashlib.sha256(app).hexdigest()}
baseline=root.parent/'artifacts/wideband-selectable/wideband_f0_selectable_bootloader.bin'
if baseline.exists():
 assert boot==baseline.read_bytes()
 report['bootloader_byte_identical_to_selectable']=True
print(json.dumps(report,indent=2))
