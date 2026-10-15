"""V6 reuse of approved bounded assembly/UV/rigid-door pipeline for simple buildings."""
import hashlib,json,subprocess
from pathlib import Path
from typing import Literal
from pydantic import BaseModel,ConfigDict,Field
from mcp.server.fastmcp import FastMCP
from door_mcp import name,Ref,BLENDER
ROOT=Path(__file__).resolve().parents[2]
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveBuildingsV6'
STATE=ROOT/'Saved/LibregroveBuildingJobs'
mcp=FastMCP('Libregrove Simple Buildings V6',instructions='Exact inspected mesh assembly, native UV and zero/one/two rigid hinged doors only. No geometry synthesis, scripts, booleans, GN or generic operators. Preserve hashed inputs; new output paths; stable asynchronous jobs. Runs in isolated Blender, no live scene mutation.')

def checked(p,exists=True,suffixes=('.blend',)):
    p=Path(p).resolve();p.relative_to(EXCHANGE.resolve())
    if p.suffix.lower() not in suffixes:raise ValueError('Unsupported file type')
    if exists and not p.is_file():raise ValueError('Input missing')
    if not exists and p.exists():raise ValueError('Overwrite refused')
    return p

def status(job_id):
    job=STATE/name(job_id)
    if not (job/'request.json').is_file():raise ValueError('Unknown job')
    if (job/'result.json').exists():return {'job_id':job_id,**json.loads((job/'result.json').read_text(encoding='utf-8'))}
    return {'job_id':job_id,'state':'running_or_interrupted'}

def start(job_id,command,args):
    job=STATE/name(job_id);req=dict(command=command,version='1.0.0',**args)
    if (job/'request.json').exists():
        if json.loads((job/'request.json').read_text(encoding='utf-8'))!=req:raise ValueError('Job ID conflict')
        return status(job_id)
    if 'blend_path' in args:
        src=checked(args['blend_path'])
        if hashlib.sha256(src.read_bytes()).hexdigest()!=args['expected_sha256']:raise ValueError('Stale input')
    if 'output_blend' in args:checked(args['output_blend'],False)
    if 'output_path' in args:checked(args['output_path'],False,('.png',) if command=='preview' else ('.fbx','.glb'))
    job.mkdir(parents=True,exist_ok=False)
    with (job/'request.json').open('x',encoding='utf-8') as f:json.dump(req,f,indent=2)
    cmd=[str(BLENDER),'--background','--factory-startup']
    if 'blend_path' in args:cmd.append(args['blend_path'])
    cmd+=['--python',str(Path(__file__).with_name('buildings_worker.py')),'--',str(job/'request.json')]
    with (job/'process.log').open('wb') as log:proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    (job/'pid.json').write_text(json.dumps({'pid':proc.pid}),encoding='utf-8')
    return {'job_id':job_id,'state':'running'}

class Door(BaseModel):
    model_config=ConfigDict(extra='forbid',allow_inf_nan=False)
    output_name:str
    parts:list[Ref]=Field(min_length=1,max_length=4)
    pivot_world_m:tuple[float,float,float]
    side:Literal['left','right']
    opening_y_min:float=Field(ge=-5,le=5)
    opening_y_max:float=Field(ge=-5,le=5)
    opening_z_min:float=Field(ge=0,le=3)
    opening_z_max:float=Field(ge=.1,le=4)
    wall_front_x:float=Field(ge=0,le=5)

@mcp.tool()
def blender_building_job_status(job_id:str)->dict:return status(job_id)

@mcp.tool()
def blender_building_self_test(job_id:str,output_blend:str)->dict:
    """Fixed synthetic double-door fixture plus native AABB UV and all73-frame collision/motion checks. No user file."""
    return start(job_id,'self_test',dict(output_blend=output_blend))

@mcp.tool()
def blender_building_assemble_copy(job_id:str,blend_path:str,expected_sha256:str,static_parts:list[Ref],doors:list[Door],body_name:str,output_blend:str)->dict:
    """Concatenate 1-96 exact inspected static parts and 0-2 door groups to a clean independent assembly. Bake world transforms, retain geometry/UV, preserve input. Door pivot must lie at inspected front/lower hinge corner. No geometry creation except copying existing vertices/faces."""
    if not 1<=len(static_parts)<=96 or len(doors)>2:raise ValueError('Part count bound')
    name(body_name);refs=static_parts+[r for d in doors for r in d.parts]
    if len({r.object_name for r in refs})!=len(refs):raise ValueError('Repeated source')
    for r in refs:name(r.object_name)
    for d in doors:
        name(d.output_name)
        if not all(abs(v)<=10 for v in d.pivot_world_m):raise ValueError('Pivot bound')
        if d.opening_y_min>=d.opening_y_max or d.opening_z_min>=d.opening_z_max:raise ValueError('Opening bound')
    return start(job_id,'assemble',dict(blend_path=blend_path,expected_sha256=expected_sha256,static_parts=[r.model_dump() for r in static_parts],doors=[d.model_dump() for d in doors],body_name=body_name,output_blend=output_blend))

@mcp.tool()
def blender_building_uv_copy(job_id:str,blend_path:str,expected_sha256:str,output_blend:str)->dict:
    """Native smart-project then AABB pack per mesh, margin .008. Preserve geometry/animation and input; reject overlapping/degenerate UVs."""
    return start(job_id,'uv',dict(blend_path=blend_path,expected_sha256=expected_sha256,output_blend=output_blend))

@mcp.tool()
def blender_building_animate_copy(job_id:str,blend_path:str,expected_sha256:str,output_blend:str)->dict:
    """On a copied assembly create missing door Z actions, mirrored outward100 degrees at1/25/49/73,24fps. Reject existing action. Check both doors vs body and each other over all73 frames."""
    return start(job_id,'animate',dict(blend_path=blend_path,expected_sha256=expected_sha256,output_blend=output_blend))

@mcp.tool()
def blender_building_inspect(job_id:str,blend_path:str,expected_sha256:str)->dict:
    """Read geometry, per-mesh UV, world bounds, symmetric structure and rigid motion. Does not save."""
    return start(job_id,'inspect',dict(blend_path=blend_path,expected_sha256=expected_sha256))

@mcp.tool()
def blender_building_preview(job_id:str,blend_path:str,expected_sha256:str,frame:Literal[1,25],view:Literal['front','front_right','back'],output_path:str)->dict:
    """Read-only 960-square clay camera render, fit to exact bounds, no materials/lights saved."""
    return start(job_id,'preview',dict(blend_path=blend_path,expected_sha256=expected_sha256,frame=frame,view=view,output_path=output_path))

@mcp.tool()
def blender_building_export(job_id:str,blend_path:str,expected_sha256:str,selection:Literal['assembly','body','door_left','door_right'],output_path:str)->dict:
    """Export exact assembly (animation if present), static body or hinge-local door as NEW GLB/FBX. Scene unchanged on disk. No Unreal import."""
    return start(job_id,'export',dict(blend_path=blend_path,expected_sha256=expected_sha256,selection=selection,output_path=output_path))

@mcp.tool()
def blender_building_export_inspect(job_id:str,export_path:str,expected_sha256:str)->dict:
    """Hash checked read-only FBX/GLB reimport with animation offset0; inspect UV/size and five representative pose matrices."""
    p=checked(export_path,True,('.fbx','.glb'))
    if hashlib.sha256(p.read_bytes()).hexdigest()!=expected_sha256:raise ValueError('Stale export')
    return start(job_id,'inspect_export',dict(export_path=str(p),expected_sha256=expected_sha256))

if __name__=='__main__':mcp.run(transport='stdio')
