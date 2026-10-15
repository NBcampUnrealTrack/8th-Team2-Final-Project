"""User-approved rigid-door MCP, 2026-09-08. Fixed operations, no code inputs.

Workers run in isolated Blender processes, preserve hashed inputs and require
new output paths. No live add-on patching, arbitrary operators, GN or booleans.
"""
from __future__ import annotations
import hashlib, json, os, re, subprocess
from pathlib import Path
from typing import Literal
from mcp.server.fastmcp import FastMCP
from pydantic import BaseModel, ConfigDict, Field

ROOT=Path(__file__).resolve().parents[2]
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveDoorV5'
STATE=ROOT/'Saved/LibregroveDoorJobs'
BLENDER=Path('C:/Program Files/Blender Foundation/Blender 5.0/blender.exe')
mcp=FastMCP('Libregrove Door V5',instructions='Bounded rigid door assembly, UV, Z-rotation animation, readback and export only. Immutable hashed input; new outputs; one logical operation per call. Poll stable job IDs. No scripts, expressions, generic operators or live scene changes.')

def name(s):
    if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]{0,100}',s):raise ValueError('Exact ASCII identifier required')
    return s

def checked(path,suffixes=('.blend',),existing=True):
    p=Path(path).resolve();p.relative_to(EXCHANGE.resolve())
    if p.suffix.lower() not in suffixes:raise ValueError('Wrong suffix')
    if existing and not p.is_file():raise ValueError('Input missing')
    if not existing and p.exists():raise ValueError('Overwrite forbidden')
    return p

def status(job_id):
    job=STATE/name(job_id)
    if not (job/'request.json').is_file():raise ValueError('Unknown job')
    if (job/'result.json').exists():return {'job_id':job_id,**json.loads((job/'result.json').read_text(encoding='utf-8'))}
    return {'job_id':job_id,'state':'running_or_interrupted','receipt':str(job)}

def start(job_id,command,payload):
    job=STATE/name(job_id);request={'version':'1.0.0','command':command,**payload}
    if (job/'request.json').exists():
        if json.loads((job/'request.json').read_text(encoding='utf-8'))!=request:raise ValueError('Job ID conflict')
        return status(job_id)
    if 'blend_path' in payload:
        p=checked(payload['blend_path'])
        if hashlib.sha256(p.read_bytes()).hexdigest()!=payload['expected_sha256']:raise ValueError('Stale input SHA256')
    for field in ('output_blend','output_path'):
        if field in payload:checked(payload[field],('.blend','.png','.glb','.fbx'),False)
    job.mkdir(parents=True,exist_ok=False)
    with (job/'request.json').open('x',encoding='utf-8') as f:json.dump(request,f,indent=2)
    cmd=[str(BLENDER),'--background','--factory-startup']
    if 'blend_path' in payload:cmd.append(str(checked(payload['blend_path'])))
    cmd+=['--python',str(Path(__file__).with_name('door_worker.py')),'--',str(job/'request.json')]
    with (job/'process.log').open('wb') as log:
        proc=subprocess.Popen(cmd,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    (job/'pid.json').write_text(json.dumps({'pid':proc.pid}),encoding='utf-8')
    return {'job_id':job_id,'state':'running','version':'1.0.0'}

class Ref(BaseModel):
    model_config=ConfigDict(extra='forbid')
    object_name:str
    expected_fingerprint:str=Field(pattern=r'^[0-9a-f]{64}$')

def common(blend_path,expected_sha256):return dict(blend_path=blend_path,expected_sha256=expected_sha256)

@mcp.tool()
def blender_door_job_status(job_id:str)->dict:
    """Poll one durable job; never resubmit it."""
    return status(job_id)

@mcp.tool()
def blender_door_self_test(job_id:str,output_blend:str)->dict:
    """Synthetic 3-cube test: independent assembly, hinge invariance, open/close and UV checks. No user data loaded."""
    return start(job_id,'self_test',dict(output_blend=output_blend))

@mcp.tool()
def blender_door_assembly_copy(job_id:str,blend_path:str,expected_sha256:str,static_parts:list[Ref],moving_parts:list[Ref],pivot_world_m:tuple[float,float,float],body_name:str,door_name:str,output_blend:str)->dict:
    """Combine 1-48 inspected static meshes and 1-4 rigid door/handle meshes into TWO independent meshes in a NEW scene. Bake source transforms without changing world geometry; door origin at explicit hinge. No new topology except concatenation; preserves source objects and input file. Source names/fingerprints must be inspected. No booleans or generated geometry."""
    if not 1<=len(static_parts)<=48 or not 1<=len(moving_parts)<=4:raise ValueError('Assembly part bound')
    refs=static_parts+moving_parts
    if len({r.object_name for r in refs})!=len(refs):raise ValueError('Duplicate source')
    for r in refs:name(r.object_name)
    name(body_name);name(door_name)
    if body_name==door_name:raise ValueError('Distinct new names required')
    if not all(abs(v)<=10 for v in pivot_world_m):raise ValueError('Pivot outside task bounds')
    return start(job_id,'assemble',dict(**common(blend_path,expected_sha256),static_parts=[r.model_dump() for r in static_parts],moving_parts=[r.model_dump() for r in moving_parts],pivot_world_m=pivot_world_m,body_name=body_name,door_name=door_name,output_blend=output_blend))

@mcp.tool()
def blender_door_uv_copy(job_id:str,blend_path:str,expected_sha256:str,output_blend:str)->dict:
    """On a saved TWO-mesh door assembly copy, native smart-project and pack one unique 0-1 atlas per mesh (0.008 fractional island margin). Geometry and input preserved. Reject overlapping/degenerate UVs."""
    return start(job_id,'uv',dict(**common(blend_path,expected_sha256),output_blend=output_blend))

@mcp.tool()
def blender_door_keyframes_copy(job_id:str,blend_path:str,expected_sha256:str,door_name:str,open_angle_degrees:float,output_blend:str)->dict:
    """Add one new rigid-door Z rotation action to a copied TWO-mesh assembly: frame1 closed,25 open,49 open,73 closed,24fps. Angle -110..-85 degrees, no armature. Reject preexisting animation. Validate every frame and fixed hinge before new save."""
    name(door_name)
    if not -110<=open_angle_degrees<=-85:raise ValueError('Door outward swing angle bound')
    return start(job_id,'animate',dict(**common(blend_path,expected_sha256),door_name=door_name,open_angle_degrees=open_angle_degrees,output_blend=output_blend))

@mcp.tool()
def blender_door_inspect(job_id:str,blend_path:str,expected_sha256:str)->dict:
    """Read topology/UV and motion at all73frames, inspect doorway rays and door/frame collision samples. No save. Exact diagnostics, not engine collision validation."""
    return start(job_id,'inspect',common(blend_path,expected_sha256))

@mcp.tool()
def blender_door_preview(job_id:str,blend_path:str,expected_sha256:str,frame:Literal[1,13,25,49,73],view:Literal['front','front_right','back'],output_path:str)->dict:
    """Read-only isolated clay render of both assembly meshes at one exact animation frame, 960-square PNG. Preview camera/lights are never saved to input."""
    return start(job_id,'preview',dict(**common(blend_path,expected_sha256),frame=frame,view=view,output_path=output_path))

@mcp.tool()
def blender_door_export(job_id:str,blend_path:str,expected_sha256:str,selection:Literal['assembly_animated','body_static','door_static_local'],output_path:str)->dict:
    """Export new GLB/FBX with exact two-mesh assembly, body only or hinge-local door only. Static door at origin; assembly animation baked 24fps. No input modification, textures, rigs or Unreal import."""
    return start(job_id,'export',dict(**common(blend_path,expected_sha256),selection=selection,output_path=output_path))

@mcp.tool()
def blender_door_export_inspect(job_id:str,export_path:str,expected_sha256:str)->dict:
    """Read-only GLB/FBX reimport into an isolated empty Blender. Report meshes, transforms, UV and animated trajectory. Never save."""
    p=checked(export_path,('.glb','.fbx'))
    if hashlib.sha256(p.read_bytes()).hexdigest()!=expected_sha256:raise ValueError('Export hash mismatch')
    return start(job_id,'inspect_export',dict(export_path=str(p),expected_sha256=expected_sha256))

if __name__=='__main__':mcp.run(transport='stdio')
