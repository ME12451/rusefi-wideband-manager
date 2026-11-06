#!/usr/bin/env python3
import ast
from pathlib import Path
import string
import unittest
ROOT=Path(__file__).resolve().parents[1]/'qt'
tree=ast.parse((ROOT/'i18n.py').read_text())
DE=ast.literal_eval(next(n.value for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='DE' for t in n.targets)))
class CatalogTests(unittest.TestCase):
 def test_placeholders_match(self):
  formatter=string.Formatter()
  for english,german in DE.items():
   self.assertTrue(german.strip(),english)
   self.assertEqual(sorted((name,spec) for _,name,spec,_ in formatter.parse(english) if name is not None),sorted((name,spec) for _,name,spec,_ in formatter.parse(german) if name is not None),english)
 def test_all_static_ui_and_backend_errors_translated(self):
  missing=[]
  for path in ROOT.glob('*.py'):
   if path.name in ('i18n.py','smoke.py','launch.py'):continue
   for node in ast.walk(ast.parse(path.read_text())):
    if not isinstance(node,ast.Call) or not node.args or not isinstance(node.args[0],ast.Constant) or not isinstance(node.args[0].value,str):continue
    name=node.func.attr if isinstance(node.func,ast.Attribute) else node.func.id if isinstance(node.func,ast.Name) else ''
    if name in ('tr','note','confirm','error','choose','label','button','add_tab','ValueError','OSError','TimeoutError'):
     if node.args[0].value not in DE:missing.append((path.name,node.lineno,node.args[0].value))
  self.assertEqual(missing,[])
if __name__=='__main__':unittest.main(verbosity=2)
