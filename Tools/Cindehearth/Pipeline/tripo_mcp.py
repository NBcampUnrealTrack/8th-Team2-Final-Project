"""Project MCP adapter using the official Tripo SDK; Blender edits use its typed bridge."""
from __future__ import annotations
import dataclasses
import json
import os
from pathlib import Path
import re
from typing import Literal
from urllib.parse import urlsplit
import winreg
from mcp.server.fastmcp import FastMCP
from tripo3d import TripoClient, TaskStatus

PROJECT=Path(__file__).resolve().parents[3]
EXCHANGE=Path(os.environ.get('LOCALAPPDATA',str(PROJECT/'Saved')))/'Revvy/BlenderExchange/Cindehearth'
STATE=PROJECT/'Saved/CindehearthTripo'
mcp=FastMCP('Cindehearth Tripo Pipeline',instructions='Cindehearth production assets MUST use tripo_generate_p2_image: P2 native quad, simple geometry, pixel texture detail. Legacy H-series tools are reference-only. Generation uses credits. Inspect status first. Preserve request_id after uncertain submission; never blindly regenerate. Use the separate typed Blender MCP to edit downloaded models.')

def api_key():
    value=os.environ.get('TRIPO_API_KEY','').strip()
    if not value:
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER,'Environment') as key:
                value=str(winreg.QueryValueEx(key,'TRIPO_API_KEY')[0]).strip()
        except OSError:pass
    return value

def client():
    key=api_key()
    if not key:raise ValueError('TRIPO_API_KEY is not configured. Set it locally; do not paste it into chat.')
    return TripoClient(api_key=key)

def safe_name(value):
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,96}',value):raise ValueError('Use 1-96 ASCII letters, numbers, underscores or hyphens.')
    return value

def write_record(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_suffix('.tmp')
    temp.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    temp.replace(path)

@mcp.tool()
async def tripo_status(check_balance:bool=False)->dict:
    """Check local authentication configuration; optionally validate key and query credit balance without generation."""
    requests=[]
    for record in (STATE/'requests').glob('*.json'):
        try:requests.append(json.loads(record.read_text(encoding='utf-8')))
        except (OSError,ValueError):requests.append({'status':'unreadable'})
    result={'api_key_configured':bool(api_key()),'sdk':'tripo3d==0.4.2','exchange_root':str(EXCHANGE),
            'generation_submitted':any(bool(r.get('task_id')) for r in requests),
            'request_count':len(requests),
            'unresolved_request_count':sum(r.get('status') in ('submitting','submission_uncertain','unreadable') for r in requests)}
    if check_balance and result['api_key_configured']:
        async with client() as c:result['credits']=dataclasses.asdict(await c.get_balance())
    return result

async def submit(asset_name,request_id,kind,payload):
    safe_name(asset_name);safe_name(request_id)
    record=STATE/'requests'/(request_id+'.json')
    if record.exists():
        previous=json.loads(record.read_text(encoding='utf-8'))
        if previous['asset_name']!=asset_name or previous['kind']!=kind or previous['parameters']!=payload:
            raise ValueError('request_id belongs to different parameters; inspect the existing request before deciding on a new paid generation.')
        return previous
    c=client()  # Missing credentials must not create a submission journal.
    data={'asset_name':asset_name,'request_id':request_id,'kind':kind,'parameters':payload,'status':'submitting'}
    record.parent.mkdir(parents=True,exist_ok=True)
    with record.open('x',encoding='utf-8') as stream:json.dump(data,stream,ensure_ascii=False,indent=2)
    try:
        async with c:
            method=c.text_to_model if kind=='text' else c.image_to_model
            data['task_id']=await method(**payload)
        data['status']='submitted'
    except Exception as error:
        # An interrupted POST may have been accepted. Retain the request journal.
        data.update(status='submission_uncertain',error_type=type(error).__name__,next_step='Inspect provider task history. Do not repeat generation with a new request ID until resolved.')
    write_record(record,data)
    return data

@mcp.tool()
async def tripo_generate_text(asset_name:str,request_id:str,prompt:str,negative_prompt:str='',face_limit:int=20000,model_version:Literal['v3.1-20260211','v3.0-20250812','v2.5-20250123']='v3.1-20260211')->dict:
    """Submit one paid text-to-model task. Warn the user about credits first. Reuse request_id for readback; duplicates do not resubmit."""
    if not 1<=len(prompt)<=1024 or len(negative_prompt)>255:raise ValueError('Prompt must be 1-1024 characters; negative prompt at most 255.')
    if not 1000<=face_limit<=100000:raise ValueError('face_limit must be 1000-100000.')
    return await submit(asset_name,request_id,'text',dict(prompt=prompt,negative_prompt=negative_prompt,face_limit=face_limit,model_version=model_version,texture=True,pbr=True,texture_quality='standard',auto_size=False,quad=False,export_uv=True))

@mcp.tool()
async def tripo_generate_image(asset_name:str,request_id:str,image_path:str,face_limit:int=20000,model_version:Literal['v3.1-20260211','v3.0-20250812','v2.5-20250123']='v3.1-20260211')->dict:
    """Upload an authorized local reference image and submit one paid 3D task. Allowed roots: this project and Cindehearth Blender exchange."""
    source=Path(image_path).resolve()
    if not any(source.is_relative_to(p.resolve()) for p in (PROJECT,EXCHANGE)):raise ValueError('Image must be within the project or Cindehearth exchange root.')
    if not source.is_file() or source.suffix.lower() not in ('.jpg','.jpeg','.png','.webp'):raise ValueError('Expected an existing image file.')
    if not 1000<=face_limit<=100000:raise ValueError('face_limit must be 1000-100000.')
    return await submit(asset_name,request_id,'image',dict(image=str(source),face_limit=face_limit,model_version=model_version,texture=True,pbr=True,texture_quality='standard',auto_size=False,quad=False,export_uv=True))

@mcp.tool()
async def tripo_generate_p2_image(asset_name:str,request_id:str,image_path:str,face_limit:int=2000)->dict:
    """Paid P2 Smart Mesh generation with native quad topology. Uses official v3 P2-20260801, not H-series smart_low_poly. Reuse request ID after uncertain submission."""
    safe_name(asset_name);safe_name(request_id)
    source=Path(image_path).resolve()
    if not any(source.is_relative_to(p.resolve()) for p in (PROJECT,EXCHANGE)):raise ValueError('Image must be in project or exchange.')
    if not source.is_file() or source.suffix.lower() not in ('.png','.jpg','.jpeg','.webp'):raise ValueError('Existing image required.')
    if not 48<=face_limit<=25000:raise ValueError('P2 quad face_limit must be 48-25000.')
    params=dict(image_path=str(source),model='P2-20260801',face_limit=face_limit,quad=True,texture=True,pbr=True,texture_quality='standard',auto_size=False,export_uv=True)
    record=STATE/'requests'/(request_id+'.json')
    if record.exists():
        old=json.loads(record.read_text(encoding='utf-8'))
        if old['asset_name']!=asset_name or old['parameters']!=params:raise ValueError('Request ID conflicts with previous parameters.')
        return old
    c=client()
    data=dict(asset_name=asset_name,request_id=request_id,kind='p2_image',api_version='v3',parameters=params,status='preparing_upload')
    record.parent.mkdir(parents=True,exist_ok=True)
    with record.open('x',encoding='utf-8') as f:json.dump(data,f,indent=2)
    try:
        async with c:
            token=await c._v3_upload_file(str(source))
            body={k:v for k,v in params.items() if k!='image_path'}
            body['input']=token
            data.update(status='submitting',submitted_parameters=body);write_record(record,data)
            response=await c._v3_request('POST','/v3/generation/image-to-model',json_data=body)
            data.update(task_id=response['data']['task_id'],status='submitted')
    except Exception as error:
        data.update(status='submission_uncertain' if data['status']=='submitting' else 'upload_failed',error_type=type(error).__name__)
        data['error_code']=getattr(error,'code',None)
        data['error_message']=str(getattr(error,'message',''))[:400].replace(api_key(),'[REDACTED]')
        data['next_step']='Inspect provider history and this journal before any new paid request.'
    write_record(record,data)
    return data

@mcp.tool()
async def tripo_task_status(task_id:str,api_version:Literal['v2','v3']='v2')->dict:
    """Read status of an existing task; never submit another generation while polling."""
    safe_name(task_id)
    if api_version=='v3':
        async with client() as c:result=await c._v3_get_task(task_id)
        write_record(STATE/'tasks'/(task_id+'.json'),result)
        return {**{k:v for k,v in result.items() if k!='output'},'output_files':{k:urlsplit(v).path.rsplit('/',1)[-1] if isinstance(v,str) and v.startswith('https://') else v for k,v in (result.get('output') or {}).items()}}
    async with client() as c:task=await c.get_task(task_id)
    result=dataclasses.asdict(task)
    write_record(STATE/'tasks'/(task_id+'.json'),result)
    return result

@mcp.tool()
async def tripo_download_result(task_id:str,asset_name:str,api_version:Literal['v2','v3']='v2')->dict:
    """Download a successful task into a new exchange folder for typed Blender import. Existing output files are never overwritten."""
    safe_name(task_id);safe_name(asset_name)
    dest=EXCHANGE/'Tripo'/asset_name/task_id
    receipt=STATE/'downloads'/(task_id+'.json')
    if receipt.exists():
        result=json.loads(receipt.read_text(encoding='utf-8'))
        if all(Path(p).is_file() for p in result['files'].values() if p):return result
        raise ValueError('Receipt exists but files changed. Inspect output instead of overwriting.')
    if dest.exists():raise ValueError('Output folder already exists without receipt; inspect partial download.')
    async with client() as c:
        task=await c.get_task(task_id,use_v3=api_version=='v3')
        if task.status!=TaskStatus.SUCCESS:return {'downloaded':False,'status':task.status,'progress':task.progress}
        dest.mkdir(parents=True,exist_ok=False)
        files=await c.download_task_models(task,str(dest))
    result={'downloaded':True,'task_id':task_id,'asset_name':asset_name,'files':files,'output_directory':str(dest)}
    if not any(p and Path(p).is_file() for p in files.values()):raise ValueError('No model files downloaded; inspect output folder.')
    write_record(receipt,result)
    return result

if __name__=='__main__':mcp.run(transport='stdio')
