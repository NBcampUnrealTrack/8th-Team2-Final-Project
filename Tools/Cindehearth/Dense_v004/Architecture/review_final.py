"""Render explicit inspected source via existing bounded review MCP."""
from task import ROOT,EX,read,repair
import sys
name,stage=sys.argv[1:3]
src=read(ROOT/'verification'/(name+'_source.json'))
for view in ['front_right','back_left','top']:
    result=repair(name,stage+'_'+view,'blender_mesh_review_render',dict(blend_path=src['blend'],object_name=src['object'],output_path=str(EX/'Review'/(name+'_'+stage+'_'+view+'.png')),view=view,mode='textured'))
    print(name,view,result.get('output_path',result),flush=True)
