import json,sys
from concurrent.futures import ThreadPoolExecutor
from precision_stage import stage
from precision_finish import repair_uv,render

def repair(name):
    if name=='LG_04_Workshop':
        source='UV29';args={'tool':'blender_backing_panels_copy','panels':[{'center':[.99,.01,.85],'dimensions':[.03,3.10,1.50],'rotation_degrees':[0,0,0]}],'max_bounds_growth':0}
    elif name=='LG_05_WatermillHouse':
        source='UV28';args={'tool':'blender_backing_panels_copy','panels':[
            {'center':[-1.067,-.008,.957],'dimensions':[.03,2.86,.084],'rotation_degrees':[0,0,0]},
            {'center':[-1.067,-.008,1.694],'dimensions':[.03,2.86,.10],'rotation_degrees':[0,0,0]},
            {'center':[-.025,-1.449,2.078],'dimensions':[.26,.016,.16],'rotation_degrees':[0,0,0]},
            {'center':[-.025,1.433,2.078],'dimensions':[.26,.016,.16],'rotation_degrees':[0,0,0]}],'max_bounds_growth':0}
    elif name=='LG_07_Sanctuary':
        source='UV28';args={'tool':'blender_backing_discs_copy','discs':[{'center':[0,0,.680],'radius':1.08,'depth':.035,'segments':64},{'center':[0,0,.862],'radius':.895,'depth':.04,'segments':64}]}
    else:raise ValueError(name)
    stage(name,'Body30',source,args)
    stage(name,'UVTri31','Body30',{'tool':'blender_triangulate_copy'})
    repair_uv(name,'UVTri31','UV32')
    for view in ('front_right','back_left'):render(name,'UV32',view)

if __name__=='__main__':
    names={4:'LG_04_Workshop',5:'LG_05_WatermillHouse',7:'LG_07_Sanctuary'}
    with ThreadPoolExecutor(max_workers=2) as pool:list(pool.map(repair,[names[int(n)] for n in sys.argv[1:]]))
