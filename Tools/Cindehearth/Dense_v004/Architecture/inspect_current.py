from task import ROOT,read,write,repair
import sys
name,stage=sys.argv[1:3]
src=read(ROOT/'verification'/(name+'_source.json'))
d=repair(name,stage,'blender_topology_inspect',dict(blend_path=src['blend'],object_name=src['object']))
write(ROOT/'verification'/(name+'_current_topology.json'),d)
print(name,d['report']['geometry'])
