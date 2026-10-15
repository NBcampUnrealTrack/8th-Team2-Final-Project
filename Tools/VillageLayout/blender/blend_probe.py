import bpy, bmesh, sys, os
print("BLENDER", bpy.app.version_string)
out = sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.read_factory_settings(use_empty=True)
print("has import_scene.fbx:", hasattr(bpy.ops.import_scene, 'fbx'))
print("has wm.fbx_import:", hasattr(bpy.ops.wm, 'fbx_import'))
print("has export_scene.fbx:", hasattr(bpy.ops.export_scene, 'fbx'))
print("has wm.fbx_export:", hasattr(bpy.ops.wm, 'fbx_export'))
# bmesh ops signature checks
bm = bmesh.new()
r = bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
print("ico verts", len(bm.verts), "faces", len(bm.faces))
bm2 = bmesh.new()
bmesh.ops.create_cone(bm2, cap_ends=True, cap_tris=False, segments=8, radius1=1.0, radius2=0.5, depth=2.0)
print("cone verts", len(bm2.verts), "faces", len(bm2.faces))
bm3 = bmesh.new()
bmesh.ops.create_cube(bm3, size=1.0)
bmesh.ops.bevel(bm3, geom=bm3.edges[:], offset=0.1, offset_type='OFFSET', segments=2, profile=0.5, affect='EDGES')
print("bevel cube faces", len(bm3.faces))
# test cube_project op in edit mode
me = bpy.data.meshes.new("T"); bm3.to_mesh(me); ob = bpy.data.objects.new("T", me)
bpy.context.scene.collection.objects.link(ob)
bpy.context.view_layer.objects.active = ob; ob.select_set(True)
try:
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    res = bpy.ops.uv.cube_project(cube_size=4.0)
    bpy.ops.object.mode_set(mode='OBJECT')
    print("cube_project result", res, "uv layers", [u.name for u in me.uv_layers])
    uv = me.uv_layers[0].data
    print("sample uvs", [tuple(round(c,3) for c in uv[i].uv) for i in range(4)], "verts", [tuple(round(c,3) for c in me.vertices[me.loops[i].vertex_index].co) for i in range(4)])
except Exception as e:
    print("cube_project FAILED", e)
# Workbench render test
sc = bpy.context.scene
sc.render.engine = 'BLENDER_WORKBENCH'
sh = sc.display.shading
print("shading attrs", [a for a in dir(sh) if not a.startswith('_')][:60])
sh.light = 'STUDIO'; sh.color_type = 'MATERIAL'; sh.show_cavity = True; sh.show_shadow = True
sh.background_type = 'VIEWPORT'; sh.background_color = (0.15,0.15,0.18)
cam_d = bpy.data.cameras.new('Cam'); cam = bpy.data.objects.new('Cam', cam_d); sc.collection.objects.link(cam); sc.camera = cam
from mathutils import Vector
cam.location = Vector((3,3,2.5))
cam.rotation_euler = (Vector((0,0,0)) - cam.location).to_track_quat('-Z','Y').to_euler()
mat = bpy.data.materials.new("MAT_Test"); mat.diffuse_color = (0.8,0.3,0.2,1); me.materials.append(mat)
sc.render.resolution_x = 256; sc.render.resolution_y = 256
sc.render.filepath = os.path.join(out, "probe.png")
bpy.ops.render.render(write_still=True)
print("rendered", os.path.exists(sc.render.filepath))
