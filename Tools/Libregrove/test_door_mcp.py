"""Non-mutating contract tests; no Blender/user files are altered."""
import unittest
from pathlib import Path
import door_mcp as door

class ContractTests(unittest.TestCase):
    def test_path_traversal_rejected(self):
        with self.assertRaises(ValueError):door.checked(str(door.EXCHANGE/'../outside.blend'),existing=False)
    def test_wrong_export_suffix_rejected(self):
        with self.assertRaises(ValueError):door.checked(str(door.EXCHANGE/'bad.exe'),('.glb','.fbx'),False)
    def test_identifier_rejects_script_and_path(self):
        for value in ['../outside','x; code()', 'two words', '']:
            with self.assertRaises(ValueError):door.name(value)
    def test_existing_output_rejected(self):
        with self.assertRaises(ValueError):door.checked(str(door.EXCHANGE/'tests/fixture001.blend'),existing=False)
    def test_unknown_job_not_restarted(self):
        with self.assertRaises(ValueError):door.status('NeverSubmittedTestJob')
    def test_out_of_bound_rotation_rejected(self):
        with self.assertRaises(ValueError):door.blender_door_keyframes_copy('InvalidAngleTest','missing','0'*64,'Door',360,'missing')
    def test_unknown_ref_fields_rejected(self):
        with self.assertRaises(ValueError):door.Ref(object_name='Door',expected_fingerprint='0'*64,code='pass')

if __name__=='__main__':unittest.main()
