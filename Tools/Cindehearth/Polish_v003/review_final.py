"""New, immutable review renders of the current repaired source."""
import sys
from task import ROOT,EX,read,repair
for name in sys.argv[1:]:
 d=read(ROOT/'verification'/(name+'_source.json'));stage=d['object'].rsplit('_',1)[-1]
 for view in ['front_right','back_left']:
  out=EX/'Review'/(name+'_'+stage+'_'+view+'.png')
  repair(name,'Final_'+stage+'_'+view,'blender_mesh_review_render',dict(blend_path=d['blend'],object_name=d['object'],output_path=str(out),view=view,mode='textured'))
 print(name,stage,'FINAL_REVIEW',flush=True)
