"""Bounded mesh-repair MCP. No script, expression, console, or operator input.

Each job uses a separate Blender process and an immutable input .blend. Edits
require a matching fingerprint, a new object, and a new .blend. Persistent job
receipts survive stdio disconnects. The user's interactive Blender is untouched.
"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from typing import Literal
from mcp.server.fastmcp import FastMCP

ROOT=Path(__file__).resolve().parents[3]
EXCHANGE=Path(os.environ['LOCALAPPDATA'])/'Revvy/BlenderExchange/Cindehearth'
STATE=ROOT/'Saved/CindehearthRepair'
BLENDER=Path('C:/Program Files/Blender Foundation/Blender 5.0/blender.exe')
mcp=FastMCP('Cindehearth Blender Mesh Repair',instructions='Only bounded topology inspection, selected component deletion, selected boundary filling, welding, export and review renders. No arbitrary code execution. Inspect exact IDs and fingerprint before editing. New object and .blend required. Poll existing job_id; do not resubmit uncertain jobs.')

def safe_path(value:str,suffixes:tuple[str,...],exists:bool)->Path:
    p=Path(value).resolve()
    p.relative_to(EXCHANGE.resolve())
    if p.suffix.lower() not in suffixes:raise ValueError('Unsupported file extension')
    if exists and not p.is_file():raise ValueError('Input file not found')
    if not exists and p.exists():raise ValueError('Refusing to overwrite output')
    return p

def start(job_id:str,command:str,payload:dict)->dict:
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,90}',job_id):raise ValueError('Invalid job_id')
    safe_path(payload['blend_path'],('.blend',),True)
    if len(payload['object_name'])>128 or any(c in payload['object_name'] for c in '\\/*\n\r'):raise ValueError('Invalid exact object name')
    request=dict(command=command,**payload)
    job=STATE/job_id;job.mkdir(parents=True,exist_ok=True)
    req=job/'request.json'
    if req.exists():
        if json.loads(req.read_text())!=request:raise ValueError('Job ID already belongs to a different request')
        return status(job_id)
    if payload.get('output_blend'):safe_path(payload['output_blend'],('.blend',),False)
    if payload.get('output_path'):safe_path(payload['output_path'],('.png',) if command=='render' else ('.glb','.fbx'),False)
    if payload.get('texture_path'):safe_path(payload['texture_path'],('.png',),False)
    if payload.get('output_object_name')==payload['object_name']:raise ValueError('Output must be a new object')
    with req.open('x',encoding='utf-8') as f:json.dump(request,f,indent=2)
    with (job/'process.log').open('wb') as log:
        process=subprocess.Popen([str(BLENDER),'--background','--factory-startup',payload['blend_path'],'--python',str(Path(__file__).with_name('blender_repair_worker.py')),'--',str(req)],stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    (job/'pid.json').write_text(json.dumps({'pid':process.pid}),encoding='utf-8')
    return {'job_id':job_id,'state':'running'}

def status(job_id:str)->dict:
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,90}',job_id):raise ValueError('Invalid job_id')
    job=STATE/job_id
    if not (job/'request.json').exists():raise ValueError('Unknown job')
    result=job/'result.json'
    if result.exists():return {'job_id':job_id,**json.loads(result.read_text(encoding='utf-8'))}
    # A missing result is never interpreted as safe-to-retry.
    return {'job_id':job_id,'state':'running_or_interrupted','receipt':str(job)}

@mcp.tool()
def blender_repair_job_status(job_id:str)->dict:
    """Read the durable result of an already submitted job without restarting it."""
    return status(job_id)

@mcp.tool()
def blender_topology_inspect(job_id:str,blend_path:str,object_name:str)->dict:
    """Read mesh counts, connected face components and boundary loops with stable IDs and bounds. Does not save or modify the input file."""
    return start(job_id,'inspect',dict(blend_path=blend_path,object_name=object_name))

@mcp.tool()
def blender_repair_mesh_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,operation:Literal['weld','delete_components','delete_faces','fill_loops','fill_networks','cap_polygon'],selection_ids:list[int],distance:float=0.00001,max_faces:int=100,max_diameter:float=0.5)->dict:
    """ONE bounded repair on a new mesh copy: weld; delete exact inspected component or face IDs; fill exact simple closed loops; or fill cycles within exact inspected boundary networks. Face-count/diameter guards apply. Preserve source, material and existing UV loops. Planar-project cap UVs into adjacent material regions. No implicit selection, remeshing, or arbitrary code."""
    if not re.fullmatch(r'[0-9a-f]{64}',expected_fingerprint):raise ValueError('Exact inspected fingerprint required')
    if not 0<distance<=.02 or not 1<=max_faces<=5000 or not 0<max_diameter<=5:raise ValueError('Repair bounds exceeded')
    if len(selection_ids)>2048 or len(set(selection_ids))!=len(selection_ids) or any(type(i)!=int or i<0 for i in selection_ids):raise ValueError('Invalid exact IDs')
    if operation!='weld' and not selection_ids:raise ValueError('Explicit selection required')
    if operation=='weld' and selection_ids:raise ValueError('Weld uses the whole copied mesh; IDs must be empty')
    if operation=='cap_polygon' and not 3<=len(selection_ids)<=4:raise ValueError('cap_polygon requires 3 or 4 ordered inspected vertex IDs')
    return start(job_id,'repair',dict(blend_path=blend_path,object_name=object_name,expected_fingerprint=expected_fingerprint,output_object_name=output_object_name,output_blend=output_blend,operation=operation,selection_ids=selection_ids,distance=distance,max_faces=max_faces,max_diameter=max_diameter))

@mcp.tool()
def blender_rebuild_gable_roof(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,roof_component_id:int,delete_component_ids:list[int],center_x:float,half_width:float,front_y:float,back_y:float,eave_z:float,ridge_z:float,thickness:float,texture_path:str,texture_size:Literal[512,1024]=1024)->dict:
    """Replace ONE inspected roof component with a closed gable roof of ten quads. Bake the old roof's base color onto new UVs (no lighting bake). Optionally remove exact inspected stray components, capped at 20 faces total. Keeps house, original source mesh and textures. No generic geometry or script input."""
    if not re.fullmatch(r'[0-9a-f]{64}',expected_fingerprint):raise ValueError('Exact fingerprint required')
    if not (0<half_width<=10 and 0<back_y-front_y<=20 and 0<ridge_z-eave_z<=10 and .01<=thickness<=.3):raise ValueError('Roof dimensions outside limits')
    if roof_component_id<0 or any(i<0 for i in delete_component_ids) or len(delete_component_ids)>20:raise ValueError('Invalid component IDs')
    return start(job_id,'gable_roof',dict(blend_path=blend_path,object_name=object_name,expected_fingerprint=expected_fingerprint,output_object_name=output_object_name,output_blend=output_blend,roof_component_id=roof_component_id,delete_component_ids=delete_component_ids,center_x=center_x,half_width=half_width,front_y=front_y,back_y=back_y,eave_z=eave_z,ridge_z=ridge_z,thickness=thickness,texture_path=texture_path,texture_size=texture_size))

@mcp.tool()
def blender_repair_export(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_path:str)->dict:
    """Export only the inspected object as a new FBX or GLB, preserving the .blend and source object."""
    return start(job_id,'export',dict(blend_path=blend_path,object_name=object_name,expected_fingerprint=expected_fingerprint,output_path=output_path))

@mcp.tool()
def blender_mesh_review_render(job_id:str,blend_path:str,object_name:str,output_path:str,view:Literal['front_left','front_right','back_left','back_right','top'],mode:Literal['textured','clay']='textured')->dict:
    """Render only the exact model from a fixed review angle in an isolated Blender process. Saves a new PNG; never saves preview lights or changes to the .blend."""
    return start(job_id,'render',dict(blend_path=blend_path,object_name=object_name,output_path=output_path,view=view,mode=mode))

if __name__=='__main__':mcp.run(transport='stdio')
