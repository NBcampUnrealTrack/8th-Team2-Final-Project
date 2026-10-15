import unittest
from pathlib import Path
from pydantic import ValidationError
import buildings_mcp as m
from buildings_catalog import CATALOG,parts_for

class ContractTests(unittest.TestCase):
    def test_traversal(self):
        with self.assertRaises(ValueError):m.checked(str(m.EXCHANGE/'../outside.blend'),False)
    def test_overwrite(self):
        with self.assertRaises(ValueError):m.checked(str(m.EXCHANGE/'tests/fixture001.blend'),False)
    def test_wrong_file_type(self):
        with self.assertRaises(ValueError):m.checked(str(m.EXCHANGE/'bad.exe'),False)
    def test_unknown_job(self):
        with self.assertRaises(ValueError):m.status('UnknownNeverLaunchedJob')
    def test_names(self):
        for value in ['../file','code()','a b','']:
            with self.assertRaises(ValueError):m.name(value)
    def test_ref_rejects_extra_code(self):
        with self.assertRaises(ValidationError):m.Ref(object_name='test',expected_fingerprint='0'*64,code='pass')
    def test_catalog_part_names_unique(self):
        for spec in CATALOG:
            parts,doors=parts_for(spec);names=[p['object_name'] for p in parts];self.assertEqual(len(names),len(set(names)))
            for name in names:m.name(name)
            self.assertLessEqual(len(parts),96);self.assertEqual(len(doors),spec['doors'])
    def test_part_budgets(self):
        for spec in CATALOG:
            parts,_=parts_for(spec);verts=sum(6 if p['primitive']['type']=='cylinder' else 8 for p in parts);tris=sum(8 if p['primitive']['type']=='cylinder' else 12 for p in parts)
            self.assertLessEqual(verts,spec['budget_v']);self.assertLessEqual(tris,spec['budget_t'])

if __name__=='__main__':unittest.main()
