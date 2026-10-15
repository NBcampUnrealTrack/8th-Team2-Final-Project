"""Typed MCP orchestration: isolated assemble -> inspect -> UV -> animate -> QA."""
import argparse,hashlib,json,sys
from pathlib import Path
import blender_mcp_client as transport
ROOT=Path(__file__).resolve().parents[2]
DEST=ROOT/'Docs/Libregrove/DirectModel_Cottage_V5_20260908'
EXCHANGE=ROOT/'Saved/Revvy/BlenderExchange/LibregroveDoorV5'
transport.EVIDENCE=DEST/'evidence'

def result(v):
    v=v.get('result',v)
    return v.get('result',v) if v.get('state')=='finished' else v

def common(p):return {'blend_path':str(p),'expected_sha256':hashlib.sha256(p.read_bytes()).hexdigest()}

def main(stage,revision=1):
    global EXCHANGE
    suffix='' if revision==1 else '_r2'
    if revision==2:EXCHANGE=EXCHANGE/'revision02'
    client=transport.Client('door')
    original_call=client.call
    def versioned_call(tool,args,evidence):
        args=dict(args)
        if revision==2:args['job_id']+=suffix
        return original_call(tool,args,evidence+suffix)
    client.call=versioned_call
    try:
        if stage=='model':
            req=json.loads((DEST/f'assemble_{revision:03d}.json').read_text())
            assembled=result(client.call('blender_door_assembly_copy',req,'assembly_001'));print('ASSEMBLED',assembled['output'],flush=True)
            p=Path(assembled['output']['path'])
            client.call('blender_door_inspect',dict(job_id='DoorV5_InspectAssembly001',**common(p)),'inspect_assembly_001')
            uv=result(client.call('blender_door_uv_copy',dict(job_id='DoorV5_UV001',**common(p),output_blend=str(EXCHANGE/'03_Cottage_UV.blend')),'uv_001'));print('UV',uv['output'],flush=True)
            p=Path(uv['output']['path'])
            client.call('blender_door_inspect',dict(job_id='DoorV5_InspectUV001',**common(p)),'inspect_uv_001')
            animated=result(client.call('blender_door_keyframes_copy',dict(job_id='DoorV5_Animate001',**common(p),door_name='SM_LG_Cottage_Door_V5',open_angle_degrees=-100,output_blend=str(EXCHANGE/'04_Cottage_OpenClose.blend')),'animation_001'));print('ANIMATED',animated['output'],flush=True)
            p=Path(animated['output']['path'])
            final=result(client.call('blender_door_inspect',dict(job_id='DoorV5_FinalInspect001',**common(p)),'inspect_final_001'))
            assert final['motion']['passed'] and all(r['uv']['passed'] for r in final['objects'])
            with (DEST/f'verification{suffix}.json').open('x',encoding='utf-8') as f:json.dump(final,f,indent=2)
            print('FINAL',final['total_mesh_vertices'],final['total_triangles'],flush=True)
        elif stage=='preview':
            p=EXCHANGE/'04_Cottage_OpenClose.blend'
            for i,(frame,view,label) in enumerate([(1,'front_right','Closed'),(25,'front_right','Open'),(1,'front','Front_Closed'),(25,'front','Front_Open')],1):
                out=result(client.call('blender_door_preview',dict(job_id=f'DoorV5_Preview{i:03d}',**common(p),frame=frame,view=view,output_path=str(EXCHANGE/f'previews/{label}.png')),f'preview_{i:03d}'))
                print('PREVIEW',out['output_path'],flush=True)
        elif stage=='export':
            p=EXCHANGE/'04_Cottage_OpenClose.blend'
            for i,(selection,name) in enumerate([('assembly_animated','LG_Cottage_OpenClose'),('body_static','SM_LG_Cottage_Body'),('door_static_local','SM_LG_Cottage_Door')],1):
                for ext in ['glb','fbx']:
                    dest=EXCHANGE/f'exports/{name}.{ext}'
                    out=result(client.call('blender_door_export',dict(job_id=f'DoorV5_Export{i:03d}_{ext}',**common(p),selection=selection,output_path=str(dest)),f'export_{i:03d}_{ext}'))
                    print('EXPORTED',out,flush=True)
                    qa=result(client.call('blender_door_export_inspect',dict(job_id=f'DoorV5_Reimport{i:03d}_{ext}',export_path=str(dest),expected_sha256=out['sha256']),f'reimport_{i:03d}_{ext}'))
                    expected=2 if i==1 else 1
                    assert len(qa['meshes'])==expected and all(x['uv']['passed'] for x in qa['meshes'])
                    print('REIMPORT PASS',selection,ext,flush=True)
    finally:client.close()

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('stage',choices=['model','preview','export']);parser.add_argument('--revision',type=int,choices=[1,2],default=1);args=parser.parse_args();main(args.stage,args.revision)
