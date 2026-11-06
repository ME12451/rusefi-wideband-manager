#!/usr/bin/env python3
"""Report compiler-emitted frames/call paths. This is not a hardware high-water test.
Library/indirect/recursive calls and interrupt/context frames prevent a complete bound.
"""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parent
build=root/'firmware-f0/firmware/build'
frames={};names={};edges={}
for p in build.glob('*.ci'):
 for row in p.read_text().splitlines():
  m=re.search(r'node: \{ title: "([^"]+)" label: "((?:\\.|[^"\\])*)"',row)
  if m:
   key,label=m.groups();parts=label.split('\\n');names[key]=parts[0]
   size=re.search(r'\\n(\d+) bytes \(([^)]+)\)',label)
   if size and size[2]=='static':frames[key]=int(size[1])
  m=re.search(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"',row)
  if m:edges.setdefault(m[1],set()).add(m[2])
def inspect(start):
 unknown=set();cycles=set()
 def walk(k,seen):
  if k in seen:cycles.add(names.get(k,k));return 0,[]
  if k not in frames:unknown.add(names.get(k,k));return 0,[]
  candidates=[walk(c,seen|{k}) for c in edges.get(k,set())]
  best=max(candidates,key=lambda p:p[0]) if candidates else (0,[])
  return frames[k]+best[0],[names.get(k,k)]+best[1]
 total,path=walk(start,set())
 return {'local_frame_bytes':frames.get(start),'known_path_sum_bytes':total,'known_path':path,'unbounded_external_or_indirect':sorted(unknown),'recursive_paths':sorted(cycles)}
result={}
for label in ['CanRxThread','CanTxThread','main']:
 keys=[k for k,n in names.items() if n==label]
 if not keys:raise RuntimeError('Missing compiler stack/callgraph data for '+label)
 result[label]=inspect(keys[0])
result['limitations']='Known sums exclude stack inside external/indirect/recursive calls and hardware/RTOS context frames. Not a complete worst-case bound. No on-device stack/timing test.'
print(json.dumps(result,indent=2))
