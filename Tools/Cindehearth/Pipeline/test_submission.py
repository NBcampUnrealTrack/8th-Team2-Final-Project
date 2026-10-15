"""Offline checks for paid-submission deduplication; never contacts Tripo."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import tripo_mcp as adapter

class FakeClient:
    calls=0
    fail=False
    async def __aenter__(self):return self
    async def __aexit__(self,*args):return False
    async def text_to_model(self,**kwargs):
        type(self).calls+=1
        if type(self).fail:raise TimeoutError('Acknowledgement lost')
        return 'task_example'
    async def _v3_upload_file(self,path):return 'file_example'
    async def _v3_request(self,method,path,json_data):
        type(self).calls+=1
        type(self).request=(method,path,json_data)
        if type(self).fail:raise TimeoutError('Acknowledgement lost')
        return {'data':{'task_id':'task_p2_example'}}

class SubmissionChecks(unittest.IsolatedAsyncioTestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory()
        self.state=patch.object(adapter,'STATE',Path(self.temp.name));self.state.start()
        FakeClient.calls=0;FakeClient.fail=False
    def tearDown(self):self.state.stop();self.temp.cleanup()
    async def test_duplicate_does_not_generate_twice(self):
        with patch.object(adapter,'client',FakeClient):
            first=await adapter.submit('House','request_1','text',{'prompt':'house'})
            second=await adapter.submit('House','request_1','text',{'prompt':'house'})
        self.assertEqual(first,second);self.assertEqual(FakeClient.calls,1)
    async def test_uncertain_submission_is_not_retried(self):
        FakeClient.fail=True
        with patch.object(adapter,'client',FakeClient):
            first=await adapter.submit('House','request_2','text',{'prompt':'house'})
            second=await adapter.submit('House','request_2','text',{'prompt':'house'})
        self.assertEqual(first['status'],'submission_uncertain')
        self.assertEqual(first,second);self.assertEqual(FakeClient.calls,1)
    async def test_missing_credentials_never_creates_journal(self):
        with patch.object(adapter,'client',side_effect=ValueError('Missing key')):
            with self.assertRaises(ValueError):await adapter.submit('House','request_3','text',{})
        self.assertFalse(any(Path(self.temp.name).iterdir()))
    async def test_changed_payload_is_rejected_for_existing_id(self):
        with patch.object(adapter,'client',FakeClient):
            await adapter.submit('House','request_4','text',{'prompt':'house'})
            with self.assertRaises(ValueError):await adapter.submit('House','request_4','text',{'prompt':'different'})
        self.assertEqual(FakeClient.calls,1)
    async def test_status_reports_submitted_and_uncertain_requests(self):
        with patch.object(adapter,'client',FakeClient):
            await adapter.submit('House','accepted','text',{'prompt':'house'})
            FakeClient.fail=True
            await adapter.submit('House','uncertain','text',{'prompt':'house'})
        result=await adapter.tripo_status()
        self.assertTrue(result['generation_submitted'])
        self.assertEqual(result['request_count'],2)
        self.assertEqual(result['unresolved_request_count'],1)
    async def test_p2_uses_native_quad_v3_once(self):
        image=Path(self.temp.name)/'ref.png';image.write_bytes(b'fixture')
        with patch.object(adapter,'PROJECT',Path(self.temp.name)),patch.object(adapter,'client',FakeClient):
            first=await adapter.tripo_generate_p2_image('HouseP2','p2_request',str(image),2000)
            again=await adapter.tripo_generate_p2_image('HouseP2','p2_request',str(image),2000)
        self.assertEqual(first,again);self.assertEqual(FakeClient.calls,1)
        method,path,body=FakeClient.request
        self.assertEqual((method,path),('POST','/v3/generation/image-to-model'))
        self.assertEqual(body['model'],'P2-20260801');self.assertIs(body['quad'],True)
        self.assertEqual(body['face_limit'],2000);self.assertNotIn('smart_low_poly',body)
    async def test_p2_uncertain_submission_is_not_retried(self):
        image=Path(self.temp.name)/'ref.png';image.write_bytes(b'fixture');FakeClient.fail=True
        with patch.object(adapter,'PROJECT',Path(self.temp.name)),patch.object(adapter,'client',FakeClient):
            first=await adapter.tripo_generate_p2_image('HouseP2','p2_uncertain',str(image),2000)
            again=await adapter.tripo_generate_p2_image('HouseP2','p2_uncertain',str(image),2000)
        self.assertEqual(first['status'],'submission_uncertain')
        self.assertEqual(first,again);self.assertEqual(FakeClient.calls,1)

if __name__=='__main__':unittest.main()
