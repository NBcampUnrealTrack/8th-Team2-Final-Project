# BLEND stream: re-import every FBX into a fresh scene, print stats, render Workbench preview.
# usage: blender -b --python blend_verify.py -- <meshdir> <previewdir>
import bpy, os, sys, math, glob
from mathutils import Vector

argv = sys.argv[sys.argv.index('--') + 1:]
MESHDIR, PREVDIR = argv[0], argv[1]
os.makedirs(PREVDIR, exist_ok=True)

MAT_COLORS = {
    'MAT_Wall': (0.85, 0.78, 0.62), 'MAT_Timber': (0.30, 0.18, 0.10), 'MAT_Roof': (0.38, 0.44, 0.26),
    'MAT_Stone': (0.52, 0.52, 0.50), 'MAT_Wood': (0.58, 0.40, 0.22), 'MAT_Leaf': (0.30, 0.56, 0.20),
    'MAT_Bark': (0.36, 0.25, 0.15), 'MAT_Cloth': (0.20, 0.30, 0.72), 'MAT_Metal': (0.25, 0.26, 0.30),
    'MAT_Soil': (0.30, 0.20, 0.12), 'MAT_Glow': (1.00, 0.82, 0.40), 'MAT_Cliff': (0.46, 0.42, 0.37),
}
VALID = set(MAT_COLORS)


def helper_bar(name, color, size, loc):
    me = bpy.data.meshes.new(name)
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts[:])
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    ob.location = loc
    m = bpy.data.materials.new(name + '_mat')
    m.diffuse_color = (*color, 1.0)
    me.materials.append(m)
    bpy.context.scene.collection.objects.link(ob)
    return ob


files = sorted(glob.glob(os.path.join(MESHDIR, 'SM_*.fbx')))
report = []
for f in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    bpy.ops.import_scene.fbx(filepath=f)
    meshes = [o for o in sc.objects if o.type == 'MESH']
    name = os.path.splitext(os.path.basename(f))[0]
    if len(meshes) != 1:
        print("VERIFY %s ERROR: %d mesh objects imported" % (name, len(meshes)))
        continue
    ob = meshes[0]
    me = ob.data
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    d = ob.dimensions
    slots = [m.name if m else None for m in me.materials]
    bad = [s for s in slots if s not in VALID]
    has_uv = len(me.uv_layers) > 0
    uv_ok = False
    if has_uv:
        uvd = me.uv_layers[0].data
        us = [l.uv.x for l in uvd]
        vs = [l.uv.y for l in uvd]
        uv_ok = (max(us) - min(us)) > 0.01 or (max(vs) - min(vs)) > 0.01
    # bbox min z, and xy center
    bb = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    minz = min(v.z for v in bb)
    cx = (min(v.x for v in bb) + max(v.x for v in bb)) / 2
    cy = (min(v.y for v in bb) + max(v.y for v in bb)) / 2
    unassigned = sum(1 for p in me.polygons if p.material_index >= len(slots))
    smooth_faces = sum(1 for p in me.polygons if p.use_smooth)
    line = ("VERIFY %s obj=%s dims=(%.2f, %.2f, %.2f) tris=%d verts=%d slots=%s uv=%s uv_nonzero=%s "
            "minz=%.3f xy_center=(%.2f, %.2f) scale=(%.2f,%.2f,%.2f) rot=(%.1f,%.1f,%.1f) smooth_faces=%d bad_slots=%s unassigned=%d") % (
        name, ob.name, d.x, d.y, d.z, tris, len(me.vertices), slots, has_uv, uv_ok, minz, cx, cy,
        ob.scale.x, ob.scale.y, ob.scale.z, math.degrees(ob.rotation_euler.x), math.degrees(ob.rotation_euler.y),
        math.degrees(ob.rotation_euler.z), smooth_faces, bad, unassigned)
    print(line)
    report.append(line)
    # ---- preview render
    for m in me.materials:
        if m and m.name in MAT_COLORS:
            m.diffuse_color = (*MAT_COLORS[m.name], 1.0)
    maxdim = max(d.x, d.y, d.z)
    L = max(1.0, maxdim * 0.35)
    helper_bar('AXIS_X', (0.9, 0.1, 0.1), (L, 0.05 * L, 0.05 * L), Vector((cx + L / 2, cy - d.y / 2 - 0.3 * L, 0.02)))
    helper_bar('AXIS_Y', (0.1, 0.8, 0.1), (0.05 * L, L, 0.05 * L), Vector((cx - d.x / 2 - 0.3 * L, cy + L / 2, 0.02)))
    helper_bar('GROUND', (0.22, 0.22, 0.22), (maxdim * 3, maxdim * 3, 0.01), Vector((cx, cy, -0.01)))
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading
    sh.light = 'STUDIO'
    sh.color_type = 'MATERIAL'
    sh.show_cavity = True
    sh.show_shadows = True
    sh.show_object_outline = True
    sh.background_type = 'VIEWPORT'
    sh.background_color = (0.16, 0.17, 0.20)
    sc.display.render_aa = '8'
    cam_d = bpy.data.cameras.new('Cam')
    cam_d.angle = math.radians(38)
    cam = bpy.data.objects.new('Cam', cam_d)
    sc.collection.objects.link(cam)
    sc.camera = cam
    center = Vector((cx, cy, minz + d.z / 2))
    radius = math.sqrt(d.x ** 2 + d.y ** 2 + d.z ** 2) / 2
    dirv = Vector((0.75, 1.0, 0.62)).normalized()
    dist = radius / math.sin(cam_d.angle / 2) * 1.15
    cam.location = center + dirv * dist
    cam.rotation_euler = (center - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam_d.clip_end = dist * 4
    sc.render.resolution_x = 640
    sc.render.resolution_y = 640
    sc.render.resolution_percentage = 100
    sc.render.filepath = os.path.join(PREVDIR, name + '.png')
    bpy.ops.render.render(write_still=True)
    print("RENDERED", sc.render.filepath)

print("VERIFY_DONE %d files" % len(files))
