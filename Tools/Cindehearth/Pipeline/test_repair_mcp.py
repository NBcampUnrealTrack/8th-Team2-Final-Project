"""Offline checks of repair file guards and durable job replay. No Blender launch."""
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch, Mock
import blender_repair_mcp as m

class RepairTransportTest(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.root=Path(self.temp.name)
        self.ex=self.root/'exchange';self.ex.mkdir()
        self.source=self.ex/'input.blend';self.source.write_bytes(b'fixture')
        self.state=self.root/'state'
        self.patches=[patch.object(m,'EXCHANGE',self.ex),patch.object(m,'STATE',self.state),
                      patch.object(m.subprocess,'Popen',return_value=Mock(pid=12345))]
        for p in self.patches:p.start()
        self.args=dict(blend_path=str(self.source),object_name='House',expected_fingerprint='a'*64,
                       output_path=str(self.ex/'house.glb'))
    def tearDown(self):
        for p in reversed(self.patches):p.stop()
        self.temp.cleanup()
    def test_completed_export_replay_with_existing_output(self):
        m.blender_repair_export(job_id='same',**self.args)
        Path(self.args['output_path']).write_bytes(b'result')
        (self.state/'same/result.json').write_text(json.dumps(dict(state='finished',success=True)))
        self.assertEqual(m.blender_repair_export(job_id='same',**self.args)['state'],'finished')
        self.assertEqual(m.subprocess.Popen.call_count,1)
    def test_job_id_cannot_change_request(self):
        m.blender_repair_export(job_id='same',**self.args)
        with self.assertRaises(ValueError):
            m.blender_repair_export(job_id='same',**{**self.args,'object_name':'Other'})
        self.assertEqual(m.subprocess.Popen.call_count,1)
    def test_output_path_cannot_escape_exchange(self):
        with self.assertRaises(ValueError):
            m.blender_repair_export(job_id='outside',**{**self.args,'output_path':str(self.root/'outside.glb')})
        m.subprocess.Popen.assert_not_called()
    def test_existing_output_not_overwritten(self):
        dest=Path(self.args['output_path']);dest.write_bytes(b'keep')
        with self.assertRaises(ValueError):m.blender_repair_export(job_id='new',**self.args)
        self.assertEqual(dest.read_bytes(),b'keep')
        m.subprocess.Popen.assert_not_called()
    def test_invalid_job_id(self):
        with self.assertRaises(ValueError):m.blender_repair_export(job_id='../escape',**self.args)
        m.subprocess.Popen.assert_not_called()

if __name__=='__main__':unittest.main()
