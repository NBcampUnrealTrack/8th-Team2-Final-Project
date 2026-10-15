"""User-authorized, bounded Blender precision MCP extension (2026-09-08).

No script, expression, arbitrary operator, module or executable inputs. Immutable
input .blend; fingerprint preconditions; new objects/files; durable one-shot jobs.
"""
from __future__ import annotations
import json, math, os, re, subprocess
from pathlib import Path
from typing import Literal
from mcp.server.fastmcp import FastMCP
from pydantic import BaseModel, ConfigDict, Field

ROOT=Path(__file__).resolve().parents[2]
EXCHANGE=Path(os.environ['LOCALAPPDATA'])/'Revvy/BlenderExchange/Cindehearth/Libregrove_20260908'
OUTPUT=EXCHANGE/'precision_v2'
STATE=ROOT/'Saved/LibregrovePrecision'
BLENDER=Path('C:/Program Files/Blender Foundation/Blender 5.0/blender.exe')
VERSION='2.0.0'
mcp=FastMCP('Libregrove Blender Precision',instructions='Bounded inspected-ID topology/UV edits only. Preserve input files and source meshes. Each mutation requires an exact fingerprint and new output. Inspect before/after; poll stable job IDs. No generic script tool.')

def path_checked(value,output=False):
    p=Path(os.path.abspath(value))
    if '\\?\\' in str(p):raise ValueError('Extended paths not accepted')
    root=OUTPUT if output else EXCHANGE
    p.resolve().relative_to(root.resolve())
    if p.suffix.lower()!='.blend':raise ValueError('Expected .blend')
    if output and p.exists():raise ValueError('Refusing overwrite')
    if not output and not p.is_file():raise ValueError('Missing input')
    return p

def identifier(value):
    if not re.fullmatch(r'[A-Za-z0-9_-]{1,90}',value):raise ValueError('Invalid job ID')
    return value

def exact_name(value):
    if not value or len(value)>128 or any(c in value for c in '\\/*\n\r'):raise ValueError('Invalid object name')

def ids_checked(ids,allow_empty=False):
    if (not ids and not allow_empty) or len(ids)>100000 or len(set(ids))!=len(ids) or any(type(i)!=int or i<0 for i in ids):raise ValueError('Exact unique nonnegative inspected IDs required')

def finite_range(value,lo,hi):
    if not math.isfinite(value) or not lo<=value<=hi:raise ValueError('Numeric bound exceeded')

def status(job_id):
    job=STATE/identifier(job_id)
    if not (job/'request.json').exists():raise ValueError('Unknown job')
    if (job/'result.json').exists():return {'job_id':job_id,**json.loads((job/'result.json').read_text(encoding='utf-8'))}
    return {'job_id':job_id,'state':'running_or_interrupted','receipt':str(job)}

def start(job_id,command,payload):
    job=STATE/identifier(job_id);request={'command':command,'version':VERSION,**payload}
    if (job/'request.json').exists():
        if json.loads((job/'request.json').read_text(encoding='utf-8'))!=request:raise ValueError('Job ID reuse with different input')
        return status(job_id)
    if command not in ('self_test','assemble','inspect_fbx'):
        path_checked(payload['blend_path']);exact_name(payload['object_name'])
    if command not in ('inspect','compare','self_test','assemble','inspect_fbx'):
        if not re.fullmatch(r'[0-9a-f]{64}',payload['expected_fingerprint']):raise ValueError('Fingerprint required')
        exact_name(payload['output_object_name'])
        if payload['output_object_name']==payload['object_name']:raise ValueError('New object required')
    if payload.get('output_blend'):path_checked(payload['output_blend'],True)
    job.mkdir(parents=True,exist_ok=True);OUTPUT.mkdir(parents=True,exist_ok=True)
    with (job/'request.json').open('x',encoding='utf-8') as f:json.dump(request,f,indent=2)
    launch=[str(BLENDER),'--background','--factory-startup']
    if command not in ('self_test','assemble','inspect_fbx'):launch.append(payload['blend_path'])
    launch+=['--python',str(Path(__file__).with_name('precision_worker.py')),'--',str(job/'request.json')]
    with (job/'process.log').open('wb') as log:
        process=subprocess.Popen(launch,stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
    (job/'pid.json').write_text(json.dumps({'pid':process.pid}),encoding='utf-8')
    return {'job_id':job_id,'state':'running','version':VERSION}

def base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend):
    return dict(blend_path=blend_path,object_name=object_name,expected_fingerprint=expected_fingerprint,output_object_name=output_object_name,output_blend=output_blend)

@mcp.tool()
def blender_precision_job_status(job_id:str)->dict:
    """Poll an existing durable job, never restart it."""
    return status(job_id)

@mcp.tool()
def blender_precision_self_test(job_id:str,output_blend:str)->dict:
    """Run fixed synthetic fixtures for planar dissolve, nonmanifold split, seams, UV repair/pack, symmetry and sharp normals. Does not load any user model. Save a NEW test fixture .blend."""
    return start(job_id,'self_test',dict(output_blend=output_blend))

@mcp.tool()
def blender_components_offset_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,component_ids:list[int],offset_m:tuple[float,float,float])->dict:
    """Translate 1-100 complete inspected connected face components by a vector of at most 5mm to separate evidenced coplanar contacts. No deformation, boolean or remeshing. Exact untouched-component/source checks required."""
    ids_checked(component_ids)
    if len(component_ids)>100:raise ValueError('At most 100 exact components')
    for value in offset_m:finite_range(value,-.005,.005)
    finite_range(math.sqrt(sum(v*v for v in offset_m)),.00001,.005)
    return start(job_id,'offset_components',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),component_ids=component_ids,offset_m=offset_m))

@mcp.tool()
def blender_delete_wire_edges_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,edge_ids:list[int])->dict:
    """Remove only 1-100 exact inspected edges with ZERO incident faces, and their vertices only if fully isolated afterward. Reject any face-count change. No surface deletion."""
    ids_checked(edge_ids)
    if len(edge_ids)>100:raise ValueError('At most 100 exact wire edges')
    return start(job_id,'delete_wire_edges',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),edge_ids=edge_ids))

@mcp.tool()
def blender_surface_guarded_decimate_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,ratio:float,max_surface_error_m:float=0.015,max_bounds_drift_m:float=0.005)->dict:
    """Apply exactly one native COLLAPSE decimation to a source-preserving copy. Enforce bidirectional all-vertex/triangle-centroid BVH distance and AABB drift bounds before saving; no symmetry or remeshing. UVs are retained by Blender but require new overlap inspection. Candidate requires visual review."""
    finite_range(ratio,.15,.95);finite_range(max_surface_error_m,.0001,.05);finite_range(max_bounds_drift_m,.00001,.015)
    return start(job_id,'guarded_decimate',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),ratio=ratio,max_surface_error_m=max_surface_error_m,max_bounds_drift_m=max_bounds_drift_m))

@mcp.tool()
def blender_uv_sliver_guard_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,face_ids:list[int])->dict:
    """Fix only inspected tiny UV triangles that lose altitude in float32 V-flip. Move one UV corner at most 1e-5 units; do not repack other islands. Reject any resulting UV overlap/out-of-bounds. Geometry unchanged."""
    ids_checked(face_ids)
    return start(job_id,'uv_sliver_guard',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),face_ids=face_ids))

class DuplicatePair(BaseModel):
    model_config=ConfigDict(extra='forbid')
    keep_face:int=Field(ge=0)
    remove_face:int=Field(ge=0)

@mcp.tool()
def blender_duplicate_faces_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,pairs:list[DuplicatePair])->dict:
    """Remove 1-100 explicitly inspected coincident triangular duplicates only. Exact coordinate equality and equal material required. Opposite-winding duplicates allowed only on already two-sided materials. Preserve retained face, all UV coordinates and surviving geometry."""
    if not 1<=len(pairs)<=100:raise ValueError('1-100 exact pairs required')
    removed={p.remove_face for p in pairs}
    if len(removed)!=len(pairs) or removed & {p.keep_face for p in pairs}:raise ValueError('Retained face may not also be deleted')
    return start(job_id,'deduplicate',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),pairs=[p.model_dump() for p in pairs]))

@mcp.tool()
def blender_precision_inspect_fbx(job_id:str,fbx_path:str,expected_sha256:str)->dict:
    """Read-only verification of a hashed FBX beneath this task's exports directory. Import into a fresh isolated Blender process, inspect dimensions/geometry/UV overlap, then exit without saving. Never modifies a live Blender scene or the input file."""
    p=Path(fbx_path).resolve();p.relative_to((OUTPUT/'exports').resolve())
    if not p.is_file() or p.suffix.lower()!='.fbx':raise ValueError('Existing task-export FBX required')
    if not re.fullmatch(r'[0-9a-f]{64}',expected_sha256):raise ValueError('SHA256 required')
    return start(job_id,'inspect_fbx',dict(fbx_path=str(p),expected_sha256=expected_sha256))

@mcp.tool()
def blender_precision_inspect(job_id:str,blend_path:str,object_name:str,include_uv_overlap:bool=True)->dict:
    """Read stable vertex/edge/face IDs, face normals, components, boundary networks, UV triangles and exact triangle-overlap pairs with face IDs. No model changes."""
    return start(job_id,'inspect',dict(blend_path=blend_path,object_name=object_name,include_uv_overlap=include_uv_overlap))

@mcp.tool()
def blender_precision_surface_compare(job_id:str,blend_path:str,object_name:str,reference_object_name:str)->dict:
    """Read-only bidirectional BVH nearest-surface distance at EVERY vertex and triangle centroid of two exact meshes in one saved file. Reports finite bounds, counts and fingerprints. A finite sample test, not a mathematical Hausdorff guarantee."""
    exact_name(reference_object_name)
    return start(job_id,'compare',dict(blend_path=blend_path,object_name=object_name,reference_object_name=reference_object_name))

@mcp.tool()
def blender_planar_dissolve_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,face_ids:list[int],angle_degrees:float=0.2)->dict:
    """Dissolve only coplanar interior edges whose BOTH adjacent faces are in the exact inspected selection. Preserve boundaries, material seams, UV seams, vertex positions and the original object."""
    ids_checked(face_ids);finite_range(angle_degrees,0,3)
    return start(job_id,'dissolve',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),face_ids=face_ids,angle_degrees=angle_degrees))

@mcp.tool()
def blender_topology_split_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,edge_ids:list[int])->dict:
    """Split only exact inspected edges incident to more than two faces. Keeps every face and its coordinates; separates ambiguous sheets, does NOT fill resulting intentional boundaries."""
    ids_checked(edge_ids)
    return start(job_id,'split_nonmanifold',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),edge_ids=edge_ids))

@mcp.tool()
def blender_sharp_normals_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,angle_degrees:float=45)->dict:
    """Set smooth polygon shading and explicit per-edge sharp flags from face angle on a copy. Preserves face winding and positions; not a Geometry Nodes modifier."""
    finite_range(angle_degrees,1,89)
    return start(job_id,'sharp_normals',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),angle_degrees=angle_degrees))

@mcp.tool()
def blender_partial_symmetry_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,component_ids:list[int],direction:Literal['-X','X','-Y','Y'],tolerance:float=0.0001,max_reflection_error:float=0.005)->dict:
    """Symmetrize only exact complete connected components around local origin. Reject if reflected nearest-vertex error exceeds bounded tolerance. Preserve all unselected components. Does not claim to halve exported triangles."""
    ids_checked(component_ids);finite_range(tolerance,0.000001,.005);finite_range(max_reflection_error,.000001,.02)
    return start(job_id,'symmetry',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),component_ids=component_ids,direction=direction,tolerance=tolerance,max_reflection_error=max_reflection_error))

@mcp.tool()
def blender_uv_seams_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,edge_ids:list[int],mark:bool=True)->dict:
    """Set/clear UV seam flags on exact inspected edges; preserve all other seam flags and UV coordinates."""
    ids_checked(edge_ids)
    return start(job_id,'seams',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),edge_ids=edge_ids,mark=mark))

@mcp.tool()
def blender_uv_native_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,method:Literal['smart_project','unwrap','pack'],angle_degrees:float=60,margin_fraction:float=0.001)->dict:
    """Blender-native UV projection or seam unwrap, then true fractional-margin island packing (CONCAVE). Returns a candidate with full diagnostics; overlap repair is a separate explicit selected-face command."""
    finite_range(angle_degrees,10,89);finite_range(margin_fraction,.0001,.01)
    return start(job_id,'uv_native',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),method=method,angle_degrees=angle_degrees,margin_fraction=margin_fraction))

@mcp.tool()
def blender_uv_faces_reproject_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,face_ids:list[int],margin_fraction:float=0.001,minimum_uv_area:float=0.000000001)->dict:
    """Planar reproject exact inspected TRIANGULAR faces as separate seam islands, preserving their metric aspect; repack all existing UV islands natively. No topology or position changes. Used only for evidenced overlapping/degenerate UV triangles."""
    ids_checked(face_ids);finite_range(margin_fraction,.0001,.01);finite_range(minimum_uv_area,0,.0000001)
    return start(job_id,'uv_faces',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),face_ids=face_ids,margin_fraction=margin_fraction,minimum_uv_area=minimum_uv_area))

class Panel(BaseModel):
    model_config=ConfigDict(extra='forbid',allow_inf_nan=False)
    center:tuple[float,float,float]
    dimensions:tuple[float,float,float]
    rotation_degrees:tuple[float,float,float]=(0,0,0)

class Cylinder(BaseModel):
    model_config=ConfigDict(extra='forbid',allow_inf_nan=False)
    center:tuple[float,float,float]
    radius:float=Field(gt=0,le=10)
    depth:float=Field(gt=0,le=2)
    segments:int=Field(default=64,ge=16,le=96)

@mcp.tool()
def blender_backing_discs_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,discs:list[Cylinder])->dict:
    """Add 1-4 closed Z-axis circular backing slabs within the inspected mesh bounds, below broken circular ledges. Preserves existing detailed faces. No boolean or remeshing."""
    if not 1<=len(discs)<=4:raise ValueError('1-4 backing discs only')
    return start(job_id,'discs',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),discs=[d.model_dump() for d in discs]))

class ReviewAsset(BaseModel):
    model_config=ConfigDict(extra='forbid')
    blend_path:str
    object_name:str
    expected_fingerprint:str
    display_name:str

@mcp.tool()
def blender_assemble_clean_review(job_id:str,assets:list[ReviewAsset],output_blend:str,layout:Literal['grid','origin']='grid')->dict:
    """Create a NEW clean review .blend from 1-20 exact fingerprint-checked meshes in immutable input files. Fixed grid spacing 8m or origin; independent meshes; no old trials or user scenes are modified."""
    if not 1<=len(assets)<=20:raise ValueError('1-20 exact meshes only')
    if len({a.display_name for a in assets})!=len(assets):raise ValueError('Display names must be unique')
    for a in assets:
        path_checked(a.blend_path);exact_name(a.object_name);exact_name(a.display_name)
        if not re.fullmatch(r'[0-9a-f]{64}',a.expected_fingerprint):raise ValueError('Fingerprint required')
    return start(job_id,'assemble',dict(assets=[a.model_dump() for a in assets],output_blend=output_blend,layout=layout))

@mcp.tool()
def blender_backing_panels_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,panels:list[Panel],max_bounds_growth:float=0.002)->dict:
    """Add up to 12 closed rectangular solid backing panels beneath inspected roof/beam gaps. Each exact local center, dimensions and XYZ Euler rotation is bounded to source bounds. Keeps original surface/detail; not boolean/remesh. One patch set on a new object."""
    if not 1<=len(panels)<=12:raise ValueError('1-12 panels only')
    for p in panels:
        for x in p.dimensions:finite_range(x,.001,20)
        for x in p.center:finite_range(x,-50,50)
        for x in p.rotation_degrees:finite_range(x,-180,180)
    finite_range(max_bounds_growth,0,.02)
    return start(job_id,'panels',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),panels=[p.model_dump() for p in panels],max_bounds_growth=max_bounds_growth))

@mcp.tool()
def blender_triangulate_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str)->dict:
    """Triangulate the exact copied mesh with fixed BEAUTY settings for deterministic UV/export face mapping. No decimation or coordinate changes."""
    return start(job_id,'triangulate',base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend))

@mcp.tool()
def blender_triangulate_repair_copy(job_id:str,blend_path:str,object_name:str,expected_fingerprint:str,output_object_name:str,output_blend:str,degenerate_source_face_ids:list[int],max_removed_triangle_area:float=0.0000000001)->dict:
    """Triangulate with fixed settings; remove only tiny triangles arising inside exact inspected source face IDs. IDs must come from triangulation_preview. Deletes at most 100 triangles, each under a bounded area in local square meters. No ordinary visible face deletion."""
    ids_checked(degenerate_source_face_ids);finite_range(max_removed_triangle_area,0,.00000001)
    return start(job_id,'triangulate_repair',dict(**base(blend_path,object_name,expected_fingerprint,output_object_name,output_blend),degenerate_source_face_ids=degenerate_source_face_ids,max_removed_triangle_area=max_removed_triangle_area))

if __name__=='__main__':mcp.run(transport='stdio')
