# BLEND stream: low-poly stylized medieval village prop set (Blender 5.0 headless)
# usage: blender -b --python blend_build.py -- <outdir> [only_name1,only_name2]
import bpy, bmesh, math, os, sys, random
from mathutils import Vector, Matrix, Euler, noise

argv = sys.argv[sys.argv.index('--') + 1:]
OUTDIR = argv[0]
ONLY = set(argv[1].split(',')) if len(argv) > 1 else None
os.makedirs(OUTDIR, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)

MAT_COLORS = {
    'MAT_Wall':   (0.85, 0.78, 0.62),
    'MAT_Timber': (0.30, 0.18, 0.10),
    'MAT_Roof':   (0.38, 0.44, 0.26),
    'MAT_Stone':  (0.52, 0.52, 0.50),
    'MAT_Wood':   (0.58, 0.40, 0.22),
    'MAT_Leaf':   (0.30, 0.56, 0.20),
    'MAT_Bark':   (0.36, 0.25, 0.15),
    'MAT_Cloth':  (0.20, 0.30, 0.72),
    'MAT_Metal':  (0.25, 0.26, 0.30),
    'MAT_Soil':   (0.30, 0.20, 0.12),
    'MAT_Glow':   (1.00, 0.82, 0.40),
    'MAT_Cliff':  (0.46, 0.42, 0.37),
}


def get_mat(name):
    m = bpy.data.materials.get(name)
    if m is None:
        m = bpy.data.materials.new(name)
        m.diffuse_color = (*MAT_COLORS[name], 1.0)
    return m


def tri_count(me):
    return sum(len(p.vertices) - 2 for p in me.polygons)


# ---------------------------------------------------------------- builder
class Builder:
    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.slots = []

    def slot(self, mat):
        if mat not in self.slots:
            self.slots.append(mat)
        return self.slots.index(mat)

    def add_geom(self, verts, faces, mat, smooth=False, M=None):
        idx = self.slot(mat)
        bm = self.bm
        bverts = []
        for c in verts:
            v = Vector(c)
            if M is not None:
                v = M @ v
            bverts.append(bm.verts.new(v))
        for f in faces:
            if isinstance(f[0], (list, tuple)):
                idxs, sm = f
            else:
                idxs, sm = f, smooth
            try:
                bf = bm.faces.new([bverts[i] for i in idxs])
            except ValueError:
                continue
            bf.material_index = idx
            bf.smooth = sm

    def add_bm(self, tmp, mat, smooth=False, M=None):
        tmp.verts.ensure_lookup_table()
        tmp.faces.ensure_lookup_table()
        verts = [v.co.copy() for v in tmp.verts]
        faces = [tuple(v.index for v in f.verts) for f in tmp.faces]
        tmp.free()
        self.add_geom(verts, faces, mat, smooth, M)

    # --- primitives -------------------------------------------------
    def box(self, center, size, mat, rot=None, M=None, smooth=False):
        hx, hy, hz = size[0] / 2, size[1] / 2, size[2] / 2
        verts = [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
                 (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]
        faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        T = Matrix.Translation(Vector(center))
        if rot is not None:
            T = T @ rot.to_matrix().to_4x4()
        if M is not None:
            T = M @ T
        self.add_geom(verts, faces, mat, smooth, T)

    def box_z(self, cx, cy, z0, z1, sx, sy, mat, **kw):
        self.box((cx, cy, (z0 + z1) / 2), (sx, sy, z1 - z0), mat, **kw)

    def lathe(self, profile, segs, mat, smooth=True, caps=True, close_ring=False, base=(0, 0, 0),
              rot=None, M=None, angle_offset=0.0):
        """profile: list of (r, z). Rings at each profile point; r==0 -> apex point."""
        verts = []
        rings = []
        for (r, z) in profile:
            if r < 1e-6:
                rings.append([len(verts)])
                verts.append((0.0, 0.0, z))
            else:
                ring = []
                for i in range(segs):
                    a = angle_offset + 2 * math.pi * i / segs
                    ring.append(len(verts))
                    verts.append((r * math.cos(a), r * math.sin(a), z))
                rings.append(ring)
        faces = []

        def connect(A, B):
            if len(A) == 1 and len(B) == 1:
                return
            if len(A) == 1:
                for i in range(segs):
                    faces.append(((A[0], B[i], B[(i + 1) % segs]), smooth))
            elif len(B) == 1:
                for i in range(segs):
                    faces.append(((A[i], A[(i + 1) % segs], B[0]), smooth))
            else:
                for i in range(segs):
                    j = (i + 1) % segs
                    faces.append(((A[i], A[j], B[j], B[i]), smooth))

        for k in range(len(rings) - 1):
            connect(rings[k], rings[k + 1])
        if close_ring:
            connect(rings[-1], rings[0])
        elif caps:
            if len(rings[0]) > 1:
                faces.append((tuple(reversed(rings[0])), False))
            if len(rings[-1]) > 1:
                faces.append((tuple(rings[-1]), False))
        T = Matrix.Translation(Vector(base))
        if rot is not None:
            T = T @ rot.to_matrix().to_4x4()
        if M is not None:
            T = M @ T
        self.add_geom(verts, faces, mat, smooth, T)

    def cyl(self, base, r1, r2, h, segs, mat, smooth=True, caps=True, rot=None, M=None, angle_offset=0.0):
        self.lathe([(r1, 0.0), (r2, h)], segs, mat, smooth=smooth, caps=caps, base=base, rot=rot, M=M,
                   angle_offset=angle_offset)

    def prism(self, pts, c0, c1, mat, plane='XY', M=None, smooth=False):
        """Extrude a 2D outline. plane 'XY': pts=(x,y), extruded along z from c0..c1.
        'XZ': pts=(x,z), extruded along y. 'YZ': pts=(y,z) extruded along x."""
        n = len(pts)

        def P(a, b, c):
            if plane == 'XY':
                return (a, b, c)
            if plane == 'XZ':
                return (a, c, b)
            return (c, a, b)

        tmp = bmesh.new()
        bv = [tmp.verts.new(P(a, b, c0)) for a, b in pts] + [tmp.verts.new(P(a, b, c1)) for a, b in pts]
        caps = [tmp.faces.new([bv[i] for i in reversed(range(n))]), tmp.faces.new([bv[i] for i in range(n, 2 * n)])]
        for i in range(n):
            j = (i + 1) % n
            tmp.faces.new([bv[i], bv[j], bv[n + j], bv[n + i]])
        if n > 4:
            bmesh.ops.triangulate(tmp, faces=caps)
        self.add_bm(tmp, mat, smooth, M)

    def pyramid(self, base_center, base_size, h, mat, M=None):
        hx, hy = base_size[0] / 2, base_size[1] / 2
        verts = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0), (0, 0, h)]
        faces = [(0, 3, 2, 1), (0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]
        T = Matrix.Translation(Vector(base_center))
        if M is not None:
            T = M @ T
        self.add_geom(verts, faces, mat, False, T)

    def icosphere(self, center, r, mat, subdiv=2, scale=(1, 1, 1), noise_amp=0.0, seed=0, smooth=True,
                  clamp_z0=None, freq=1.4):
        tmp = bmesh.new()
        bmesh.ops.create_icosphere(tmp, subdivisions=subdiv, radius=r)
        off = Vector((seed * 3.1, seed * 1.7, seed * 0.9))
        for v in tmp.verts:
            v.co = Vector((v.co.x * scale[0], v.co.y * scale[1], v.co.z * scale[2]))
            if noise_amp > 0:
                d = noise.noise(v.co * freq / max(r, 0.01) + off)
                v.co += v.co.normalized() * d * noise_amp
        if clamp_z0 is not None:
            for v in tmp.verts:
                z = v.co.z + center[2]
                if z < clamp_z0:
                    v.co.z = clamp_z0 - center[2]
        self.add_bm(tmp, mat, smooth, Matrix.Translation(Vector(center)))

    def bevel_box(self, center, size, mat, offset, segments=1, top_only=False, smooth=False, noise_amp=0.0, seed=0,
                  clamp_z0=None):
        tmp = bmesh.new()
        bmesh.ops.create_cube(tmp, size=1.0)
        bmesh.ops.scale(tmp, vec=Vector(size), verts=tmp.verts[:])
        if top_only:
            edges = [e for e in tmp.edges if all(v.co.z > 0 for v in e.verts)]
        else:
            edges = tmp.edges[:]
        bmesh.ops.bevel(tmp, geom=edges, offset=offset, offset_type='OFFSET', segments=segments, profile=0.5,
                        affect='EDGES', clamp_overlap=True)
        if noise_amp > 0:
            off = Vector((seed * 2.3, seed * 1.1, seed * 0.7))
            for v in tmp.verts:
                d = noise.noise(v.co * 1.6 + off)
                v.co += v.co.normalized() * d * noise_amp
        if clamp_z0 is not None:
            for v in tmp.verts:
                if v.co.z + center[2] < clamp_z0:
                    v.co.z = clamp_z0 - center[2]
        self.add_bm(tmp, mat, smooth, Matrix.Translation(Vector(center)))

    def voxel_block(self, cells, nz, cell, mat, noise_amp=0.25, seed=0):
        vid = {}
        verts = []
        faces = []

        def V(i, j, k):
            key = (i, j, k)
            if key not in vid:
                vid[key] = len(verts)
                verts.append(Vector((i * cell, j * cell, k * cell)))
            return vid[key]

        for (i, j) in cells:
            faces.append((V(i, j, nz), V(i + 1, j, nz), V(i + 1, j + 1, nz), V(i, j + 1, nz)))
            faces.append((V(i, j, 0), V(i, j + 1, 0), V(i + 1, j + 1, 0), V(i + 1, j, 0)))
            for (di, dj) in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                if (i + di, j + dj) in cells:
                    continue
                for k in range(nz):
                    if (di, dj) == (1, 0):
                        q = (V(i + 1, j, k), V(i + 1, j + 1, k), V(i + 1, j + 1, k + 1), V(i + 1, j, k + 1))
                    elif (di, dj) == (-1, 0):
                        q = (V(i, j + 1, k), V(i, j, k), V(i, j, k + 1), V(i, j + 1, k + 1))
                    elif (di, dj) == (0, 1):
                        q = (V(i + 1, j + 1, k), V(i, j + 1, k), V(i, j + 1, k + 1), V(i + 1, j + 1, k + 1))
                    else:
                        q = (V(i, j, k), V(i + 1, j, k), V(i + 1, j, k + 1), V(i, j, k + 1))
                    faces.append(q)
        xs = [v.x for v in verts]
        ys = [v.y for v in verts]
        cx = (min(xs) + max(xs)) / 2
        cy = (min(ys) + max(ys)) / 2
        out = []
        for v in verts:
            p = v - Vector((cx, cy, 0))
            k = int(round(v.z / cell))
            n = Vector((noise.noise(p * 0.9 + Vector((seed, 0, 0))),
                        noise.noise(p * 0.9 + Vector((0, seed + 7.3, 0))),
                        noise.noise(p * 0.9 + Vector((0, 0, seed + 13.1)))))
            layer = 0.75 + 0.5 * math.sin(k * 1.9 + seed)
            d = n * noise_amp * layer
            if k == 0 or k == nz:
                d.z = 0.0
            else:
                d.z *= 0.4
            out.append(p + d)
        self.add_geom(out, faces, mat, smooth=False)

    def finalize(self, uv_size=4.0, sharp_angle=35.0):
        bm = self.bm
        bm.normal_update()
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        bm.normal_update()
        ang = math.radians(sharp_angle)
        for e in bm.edges:
            lf = e.link_faces
            if len(lf) == 2:
                if (not lf[0].smooth) or (not lf[1].smooth) or lf[0].normal.angle(lf[1].normal, 0.0) > ang:
                    e.smooth = False
            else:
                e.smooth = False
        me = bpy.data.meshes.new(self.name)
        bm.to_mesh(me)
        bm.free()
        ob = bpy.data.objects.new(self.name, me)
        bpy.context.scene.collection.objects.link(ob)
        for s in self.slots:
            me.materials.append(get_mat(s))
        bpy.ops.object.select_all(action='DESELECT')
        ob.select_set(True)
        bpy.context.view_layer.objects.active = ob
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.cube_project(cube_size=uv_size, correct_aspect=False, clip_to_bounds=False, scale_to_bounds=False)
        bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        me.update()
        d = ob.dimensions
        print("BUILT %s tris=%d verts=%d dims=(%.2f, %.2f, %.2f) slots=%s" % (
            self.name, tri_count(me), len(me.vertices), d.x, d.y, d.z, self.slots))
        return ob


# ---------------------------------------------------------------- shared part helpers
def fpos(face, W, D, u, off, z):
    if face == '+Y':
        return (u, D / 2 + off, z)
    if face == '-Y':
        return (u, -D / 2 - off, z)
    if face == '+X':
        return (W / 2 + off, u, z)
    return (-W / 2 - off, u, z)


def fsize(face, w, t, h):
    return (w, t, h) if face in ('+Y', '-Y') else (t, w, h)


def wall_slab(b, face, W, D, z0, z1, t, holes, mat, out_off=0.0):
    """Tile a wall slab (thickness t) around rectangular holes (uc, zc, w, h)."""
    if face in ('+Y', '-Y'):
        u0, u1 = -W / 2, W / 2
        s = 1 if face == '+Y' else -1
        v_out = s * (D / 2 + out_off)
    else:
        u0, u1 = -D / 2 + t, D / 2 - t
        s = 1 if face == '+X' else -1
        v_out = s * (W / 2 + out_off)
    v_in = v_out - s * t
    zc_list = [z0, z1]
    for (uc, zc, w, h) in holes:
        zc_list += [zc - h / 2, zc + h / 2]
    zcuts = sorted(set(round(z, 5) for z in zc_list if z0 - 1e-6 <= z <= z1 + 1e-6))
    for za, zb in zip(zcuts[:-1], zcuts[1:]):
        if zb - za < 1e-4:
            continue
        segs = [(u0, u1)]
        for (uc, zc, w, h) in holes:
            if zc - h / 2 <= za + 1e-5 and zc + h / 2 >= zb - 1e-5:
                ha, hb = uc - w / 2, uc + w / 2
                new = []
                for (a, c) in segs:
                    if hb <= a + 1e-6 or ha >= c - 1e-6:
                        new.append((a, c))
                        continue
                    if ha > a + 1e-6:
                        new.append((a, ha))
                    if hb < c - 1e-6:
                        new.append((hb, c))
                segs = new
        for (a, c) in segs:
            if c - a < 1e-4:
                continue
            if face in ('+Y', '-Y'):
                b.box(((a + c) / 2, (v_out + v_in) / 2, (za + zb) / 2), (c - a, t, zb - za), mat)
            else:
                b.box(((v_out + v_in) / 2, (a + c) / 2, (za + zb) / 2), (t, c - a, zb - za), mat)


def window(b, face, W, D, u, zc, w, h, muntins=True, frame_mat='MAT_Wood'):
    fw = 0.1
    # frame: embedded 0.05 into wall, protruding 0.06
    off, th = 0.005, 0.11
    b.box(fpos(face, W, D, u - w / 2 - fw / 2, off, zc), fsize(face, fw, th, h + 2 * fw), frame_mat)
    b.box(fpos(face, W, D, u + w / 2 + fw / 2, off, zc), fsize(face, fw, th, h + 2 * fw), frame_mat)
    b.box(fpos(face, W, D, u, off, zc - h / 2 - fw / 2), fsize(face, w, th, fw), frame_mat)
    b.box(fpos(face, W, D, u, off + 0.02, zc + h / 2 + fw / 2), fsize(face, w + 0.1, th + 0.04, fw + 0.02), frame_mat)
    # glow pane inset 0.10
    b.box(fpos(face, W, D, u, -0.10, zc), fsize(face, w, 0.04, h), 'MAT_Glow')
    if muntins:
        b.box(fpos(face, W, D, u, -0.07, zc), fsize(face, 0.05, 0.03, h), frame_mat)
        b.box(fpos(face, W, D, u, -0.07, zc), fsize(face, w, 0.03, 0.05), frame_mat)


def door(b, face, W, D, u, z0, w, h, frame_mat='MAT_Timber', double=False):
    zc = z0 + h / 2
    fw = 0.12
    off, th = 0.005, 0.11
    b.box(fpos(face, W, D, u - w / 2 - fw / 2, off, zc + 0.06), fsize(face, fw, th, h + 0.12), frame_mat)
    b.box(fpos(face, W, D, u + w / 2 + fw / 2, off, zc + 0.06), fsize(face, fw, th, h + 0.12), frame_mat)
    b.box(fpos(face, W, D, u, off + 0.02, z0 + h + fw / 2), fsize(face, w + 2 * fw + 0.1, th + 0.04, fw), frame_mat)
    # panel inset 0.06 from wall face, thickness 0.06
    if double:
        b.box(fpos(face, W, D, u - w / 4 - 0.01, -0.09, zc), fsize(face, w / 2 - 0.02, 0.06, h), 'MAT_Wood')
        b.box(fpos(face, W, D, u + w / 4 + 0.01, -0.09, zc), fsize(face, w / 2 - 0.02, 0.06, h), 'MAT_Wood')
    else:
        b.box(fpos(face, W, D, u, -0.09, zc), fsize(face, w, 0.06, h), 'MAT_Wood')
    # cross braces on the door
    b.box(fpos(face, W, D, u, -0.045, z0 + 0.35), fsize(face, w - 0.04, 0.03, 0.12), frame_mat)
    b.box(fpos(face, W, D, u, -0.045, z0 + h - 0.35), fsize(face, w - 0.04, 0.03, 0.12), frame_mat)
    # handle
    b.box(fpos(face, W, D, u + w / 2 - 0.18, -0.03, z0 + h * 0.48), fsize(face, 0.06, 0.06, 0.16), 'MAT_Metal')


def stud(b, face, W, D, u, z0, z1, t=0.15):
    b.box(fpos(face, W, D, u, 0.0, (z0 + z1) / 2), fsize(face, t, 0.16, z1 - z0), 'MAT_Timber')


def brace(b, face, W, D, u0, z0, u1, z1, t=0.14):
    du, dz = u1 - u0, z1 - z0
    L = math.hypot(du, dz)
    if face in ('+Y', '-Y'):
        rot = Euler((0, -math.atan2(dz, du), 0))
        size = (L, t, t)
    else:
        rot = Euler((math.atan2(dz, du), 0, 0))
        size = (t, L, t)
    b.box(fpos(face, W, D, (u0 + u1) / 2, 0.0, (z0 + z1) / 2), size, 'MAT_Timber', rot=rot)


def timber_frame(b, W, D, z0, z1, bands, thick=0.2):
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box((sx * W / 2, sy * D / 2, (z0 + z1) / 2), (thick, thick, z1 - z0), 'MAT_Timber')
    for zb in bands:
        b.box((0, D / 2, zb), (W + thick, thick, thick), 'MAT_Timber')
        b.box((0, -D / 2, zb), (W + thick, thick, thick), 'MAT_Timber')
        b.box((W / 2, 0, zb), (thick, D + thick, thick), 'MAT_Timber')
        b.box((-W / 2, 0, zb), (thick, D + thick, thick), 'MAT_Timber')


def gable_roof(b, W, D, wall_h, rise, ov, thick, ridge='X', gable_mat='MAT_Wall', gable_walls=True,
               roof_mat='MAT_Roof', ridge_cap=True, chimneys=(), gable_ov=None):
    """Gable roof. ridge 'X': ridge along X, slopes face +-Y. ridge 'Y': ridge along Y, gable ends face +-Y."""
    if ridge == 'X':
        L, S, M = W, D, None
    else:
        L, S = D, W
        M = Matrix.Rotation(math.radians(90), 4, 'Z')  # local X -> world Y, local Y -> world -X
    if gable_ov is None:
        gable_ov = ov
    half = S / 2
    theta = math.atan2(rise, half)
    drop = ov * math.tan(theta)
    slope_len = math.hypot(half + ov, rise + drop)
    for s in (-1, 1):
        ridge_pt = Vector((0, 0, wall_h + rise))
        eave_pt = Vector((0, s * (half + ov), wall_h - drop))
        mid = (ridge_pt + eave_pt) / 2
        n = Vector((0, s * math.sin(theta), math.cos(theta)))
        center = mid + n * (thick / 2)
        rot = Euler((-s * theta, 0, 0))
        b.box(tuple(center), (L + 2 * gable_ov, slope_len, thick), roof_mat, rot=rot, M=M)
    top_ridge = wall_h + rise + thick / math.cos(theta)
    if ridge_cap:
        b.box((0, 0, top_ridge - 0.04), (L + 2 * gable_ov + 0.04, 0.42, 0.22), roof_mat, M=M)
    if gable_walls:
        r2 = rise - 0.03
        verts = [(-L / 2, -half, wall_h), (-L / 2, half, wall_h), (-L / 2, 0, wall_h + r2),
                 (L / 2, -half, wall_h), (L / 2, half, wall_h), (L / 2, 0, wall_h + r2)]
        faces = [(0, 2, 1), (3, 4, 5), (0, 1, 4, 3), (1, 2, 5, 4), (2, 0, 3, 5)]
        b.add_geom(verts, faces, gable_mat, False, M)
    for (cx, cy) in chimneys:
        zs = wall_h + rise * (1 - abs(cy) / half) + thick / math.cos(theta)
        b.box((cx, cy, zs - 0.5 + (1.9) / 2), (0.6, 0.6, 1.9), 'MAT_Stone', M=M)
        b.box((cx, cy, zs + 1.4 + 0.06), (0.76, 0.76, 0.12), 'MAT_Stone', M=M)
    return top_ridge


# ---------------------------------------------------------------- meshes
def build_house_a():
    b = Builder('SM_House_A')
    W, D, H, t, fh = 6.0, 5.0, 3.0, 0.3, 0.4
    front = [(-1.2, fh + 1.05, 1.0, 2.1), (0.6, 1.75, 0.8, 0.9), (2.0, 1.75, 0.8, 0.9)]
    sides = [(0.0, 1.75, 0.8, 0.9)]
    wall_slab(b, '+Y', W, D, 0, H, t, front, 'MAT_Wall')
    wall_slab(b, '-Y', W, D, 0, H, t, [], 'MAT_Wall')
    wall_slab(b, '+X', W, D, 0, H, t, sides, 'MAT_Wall')
    wall_slab(b, '-X', W, D, 0, H, t, sides, 'MAT_Wall')
    b.box_z(0, 0, 0, fh, W + 0.2, D + 0.2, 'MAT_Stone')
    timber_frame(b, W, D, fh, H, bands=[fh + 0.1, H - 0.1])
    stud(b, '+Y', W, D, -0.25, fh + 0.2, H - 0.2)
    stud(b, '+Y', W, D, 1.3, fh + 0.2, H - 0.2)
    stud(b, '-Y', W, D, 0.0, fh + 0.2, H - 0.2)
    for f in ('+X', '-X'):
        stud(b, f, W, D, -1.3, fh + 0.2, H - 0.2)
        stud(b, f, W, D, 1.3, fh + 0.2, H - 0.2)
    door(b, '+Y', W, D, -1.2, fh, 1.0, 2.1)
    b.box((-1.2, D / 2 + 0.35, 0.1), (1.4, 0.5, 0.2), 'MAT_Stone')
    window(b, '+Y', W, D, 0.6, 1.75, 0.8, 0.9)
    window(b, '+Y', W, D, 2.0, 1.75, 0.8, 0.9)
    window(b, '+X', W, D, 0.0, 1.75, 0.8, 0.9)
    window(b, '-X', W, D, 0.0, 1.75, 0.8, 0.9)
    gable_roof(b, W, D, H, 2.2, 0.45, 0.25, ridge='X', chimneys=[(1.8, -0.9)])
    return b.finalize()


def build_house_b():
    b = Builder('SM_House_B')
    W, D, H, t, fh = 7.0, 6.0, 5.2, 0.3, 0.4
    front = [(-1.5, fh + 1.05, 1.0, 2.1), (1.5, 1.75, 0.8, 0.9), (-1.5, 3.9, 0.8, 0.9), (1.5, 3.9, 0.8, 0.9)]
    side = [(-1.2, 1.75, 0.8, 0.9), (1.2, 3.9, 0.8, 0.9)]
    back = [(0.0, 3.9, 0.8, 0.9)]
    wall_slab(b, '+Y', W, D, 0, H, t, front, 'MAT_Wall')
    wall_slab(b, '-Y', W, D, 0, H, t, back, 'MAT_Wall')
    wall_slab(b, '+X', W, D, 0, H, t, side, 'MAT_Wall')
    wall_slab(b, '-X', W, D, 0, H, t, side, 'MAT_Wall')
    b.box_z(0, 0, 0, fh, W + 0.2, D + 0.2, 'MAT_Stone')
    timber_frame(b, W, D, fh, H, bands=[fh + 0.1, 2.7, H - 0.1])
    stud(b, '+Y', W, D, 0.0, fh + 0.2, H - 0.2)
    stud(b, '+Y', W, D, -2.6, 2.8, H - 0.2)
    stud(b, '+Y', W, D, 2.6, 2.8, H - 0.2)
    stud(b, '+Y', W, D, 2.6, fh + 0.2, 2.6)
    brace(b, '+Y', W, D, -3.4, 2.1, -2.75, 2.6)
    brace(b, '+Y', W, D, 3.4, 2.1, 2.75, 2.6)
    brace(b, '+Y', W, D, -3.4, 4.6, -2.75, 5.1)
    brace(b, '+Y', W, D, 3.4, 4.6, 2.75, 5.1)
    for f in ('+X', '-X'):
        stud(b, f, W, D, 0.0, fh + 0.2, H - 0.2)
        stud(b, f, W, D, -2.4, 2.8, H - 0.2)
        stud(b, f, W, D, 2.4, 2.8, H - 0.2)
    stud(b, '-Y', W, D, -1.6, fh + 0.2, H - 0.2)
    stud(b, '-Y', W, D, 1.6, fh + 0.2, H - 0.2)
    door(b, '+Y', W, D, -1.5, fh, 1.0, 2.1)
    b.box((-1.5, D / 2 + 0.35, 0.1), (1.4, 0.5, 0.2), 'MAT_Stone')
    # awning over the door
    zt = fh + 2.1 + 0.12
    b.box((-1.5, D / 2 + 0.45, zt + 0.42), (1.8, 0.95, 0.1), 'MAT_Roof', rot=Euler((math.radians(-22), 0, 0)))
    for dx in (-0.65, 0.65):
        p0 = Vector((-1.5 + dx, D / 2, zt - 0.25))
        p1 = Vector((-1.5 + dx, D / 2 + 0.7, zt + 0.32))
        dvec = p1 - p0
        rot = Euler((math.atan2(dvec.z, dvec.y), 0, 0))
        b.box(tuple((p0 + p1) / 2), (0.1, dvec.length, 0.1), 'MAT_Timber', rot=rot)
    for w in front[1:]:
        window(b, '+Y', W, D, w[0], w[1], w[2], w[3])
    for f in ('+X', '-X'):
        for w in side:
            window(b, f, W, D, w[0], w[1], w[2], w[3], muntins=False)
    window(b, '-Y', W, D, 0.0, 3.9, 0.8, 0.9, muntins=False)
    gable_roof(b, W, D, H, 2.6, 0.45, 0.25, ridge='Y', chimneys=[(1.5, 1.3)])
    # attic window on front gable (surface-mounted)
    for dx in (-0.3, 0.3):
        b.box((dx, D / 2 + 0.03, 6.1), (0.08, 0.1, 0.8), 'MAT_Wood')
    for dz in (-0.35, 0.35):
        b.box((0, D / 2 + 0.03, 6.1 + dz), (0.68, 0.1, 0.08), 'MAT_Wood')
    b.box((0, D / 2 + 0.005, 6.1), (0.6, 0.03, 0.7), 'MAT_Glow')
    return b.finalize()


def build_house_c():
    b = Builder('SM_House_C')
    W, D, H, t, fh = 12.0, 8.0, 5.5, 0.3, 0.4
    front = [(0.0, fh + 1.2, 1.6, 2.4), (-3.0, 1.75, 0.8, 0.9), (3.0, 1.75, 0.8, 0.9),
             (-1.5, 4.1, 0.8, 0.9), (1.5, 4.1, 0.8, 0.9), (-4.2, 4.1, 0.8, 0.9), (4.2, 4.1, 0.8, 0.9)]
    side = [(-2.2, 1.75, 0.8, 0.9), (2.2, 4.1, 0.8, 0.9)]
    back = [(-3.0, 4.1, 0.8, 0.9), (3.0, 4.1, 0.8, 0.9)]
    wall_slab(b, '+Y', W, D, 0, H, t, front, 'MAT_Wall')
    wall_slab(b, '-Y', W, D, 0, H, t, back, 'MAT_Wall')
    wall_slab(b, '+X', W, D, 0, H, t, side, 'MAT_Wall')
    wall_slab(b, '-X', W, D, 0, H, t, side, 'MAT_Wall')
    b.box_z(0, 0, 0, fh, W + 0.2, D + 0.2, 'MAT_Stone')
    timber_frame(b, W, D, fh, H, bands=[fh + 0.1, 2.9, H - 0.1])
    for u in (-1.7, 1.7, -4.4, 4.4):
        stud(b, '+Y', W, D, u, fh + 0.2, 2.8)
    for u in (0.0, -2.9, 2.9, -5.3, 5.3):
        stud(b, '+Y', W, D, u, 3.0, H - 0.2)
    brace(b, '+Y', W, D, -5.9, 3.1, -5.35, 3.7)
    brace(b, '+Y', W, D, 5.9, 3.1, 5.35, 3.7)
    brace(b, '+Y', W, D, -5.9, 4.7, -5.35, 5.3)
    brace(b, '+Y', W, D, 5.9, 4.7, 5.35, 5.3)
    for f in ('+X', '-X'):
        stud(b, f, W, D, 0.0, fh + 0.2, H - 0.2)
        stud(b, f, W, D, -2.2, 3.0, H - 0.2)
        stud(b, f, W, D, 2.2, fh + 0.2, 2.8)
    for u in (-4.5, -1.5, 1.5, 4.5):
        stud(b, '-Y', W, D, u, fh + 0.2, H - 0.2)
    door(b, '+Y', W, D, 0.0, fh, 1.6, 2.4, double=True)
    # porch steps
    b.box((0, D / 2 + 0.3, 0.2), (2.6, 0.4, 0.4), 'MAT_Stone')
    b.box((0, D / 2 + 0.7, 0.1), (3.0, 0.4, 0.2), 'MAT_Stone')
    for w in front[1:]:
        window(b, '+Y', W, D, w[0], w[1], w[2], w[3])
    for f in ('+X', '-X'):
        for w in side:
            window(b, f, W, D, w[0], w[1], w[2], w[3], muntins=False)
    for w in back:
        window(b, '-Y', W, D, w[0], w[1], w[2], w[3], muntins=False)
    # hanging sign over the door
    zs = fh + 2.4 + 0.75
    b.box((0, D / 2 + 0.45, zs), (1.7, 0.1, 0.1), 'MAT_Timber')
    for dx in (-0.7, 0.7):
        b.box((dx, D / 2 + 0.22, zs), (0.1, 0.46, 0.1), 'MAT_Timber')
        b.box((dx, D / 2 + 0.22, zs - 0.2), (0.08, 0.44, 0.08), 'MAT_Timber', rot=Euler((math.radians(-40), 0, 0)))
    zb = zs - 0.05 - 0.14 - 0.45
    b.box((0, D / 2 + 0.45, zb), (1.2, 0.06, 0.9), 'MAT_Wood')
    for dx in (-0.6, 0.6):
        b.box((dx, D / 2 + 0.45, zb), (0.06, 0.08, 0.96), 'MAT_Timber')
    for dz in (-0.45, 0.45):
        b.box((0, D / 2 + 0.45, zb + dz), (1.26, 0.08, 0.06), 'MAT_Timber')
    for dx in (-0.45, 0.45):
        b.box((dx, D / 2 + 0.45, zs - 0.05 - 0.07), (0.03, 0.03, 0.15), 'MAT_Metal')
    gable_roof(b, W, D, H, 3.0, 0.5, 0.3, ridge='X', chimneys=[(-4.0, -1.5), (4.0, -1.5)])
    return b.finalize()


def build_house_d():
    b = Builder('SM_House_D')
    W, D, H, t, fh = 5.0, 4.0, 2.6, 0.22, 0.3
    front = [(0.0, fh + 0.95, 1.4, 1.9)]
    ph = 0.29
    nb = int(round((H - fh) / ph))
    for i in range(nb):
        z0 = fh + i * ph
        z1 = H if i == nb - 1 else z0 + ph
        oo = 0.03 if i % 2 else 0.0
        wall_slab(b, '+Y', W, D, z0, z1, t, front, 'MAT_Wood', out_off=oo)
        wall_slab(b, '-Y', W, D, z0, z1, t, [], 'MAT_Wood', out_off=oo)
        wall_slab(b, '+X', W, D, z0, z1, t, [], 'MAT_Wood', out_off=oo)
        wall_slab(b, '-X', W, D, z0, z1, t, [], 'MAT_Wood', out_off=oo)
    # inner core so the wall reads solid behind planks
    b.box_z(0, 0, fh, H, W - 0.1, D - 0.1, 'MAT_Wood')
    b.box_z(0, 0, 0, fh, W + 0.2, D + 0.2, 'MAT_Stone')
    timber_frame(b, W, D, fh, H, bands=[H - 0.1], thick=0.2)
    door(b, '+Y', W, D, 0.0, fh, 1.4, 1.9, double=True)
    brace(b, '+Y', W, D, -0.6, fh + 0.15, 0.6, fh + 1.75, t=0.1)
    brace(b, '+Y', W, D, 0.6, fh + 0.15, -0.6, fh + 1.75, t=0.1)
    b.box((0, D / 2 + 0.35, 0.075), (1.9, 0.5, 0.15), 'MAT_Stone')
    gable_roof(b, W, D, H, 1.4, 0.4, 0.2, ridge='Y', gable_mat='MAT_Wood')
    return b.finalize()


def build_fence():
    b = Builder('SM_Fence_Seg')
    for x in (-1.0 + 0.075, 1.0 - 0.075):
        b.box_z(x, 0, 0, 1.1, 0.15, 0.15, 'MAT_Wood')
        b.pyramid((x, 0, 1.1), (0.15, 0.15), 0.08, 'MAT_Wood')
    for z in (0.42, 0.85):
        b.box((0, 0, z), (2.0, 0.08, 0.06), 'MAT_Wood')
    return b.finalize()


def build_stonewall():
    b = Builder('SM_StoneWall_Seg')
    b.box_z(0, 0, 0, 0.12, 2.0, 0.58, 'MAT_Stone')
    b.bevel_box((0, 0, 0.12 + 0.39), (2.0, 0.5, 0.78), 'MAT_Stone', offset=0.08, segments=1, top_only=True)
    return b.finalize()


def build_hedge():
    b = Builder('SM_Hedge')
    b.bevel_box((0, 0, 0.49), (2.0, 0.9, 1.0), 'MAT_Leaf', offset=0.28, segments=3, smooth=True, noise_amp=0.05, seed=3,
                clamp_z0=0.0)
    return b.finalize()


def build_tree_a():
    b = Builder('SM_Tree_A')
    b.cyl((0, 0, 0), 0.45, 0.3, 3.2, 8, 'MAT_Bark')
    for i in range(3):
        a = math.radians(30 + 120 * i)
        M = Matrix.Translation(Vector((0, 0, 2.6))) @ Matrix.Rotation(a, 4, 'Z') @ Matrix.Rotation(math.radians(38), 4, 'X')
        b.cyl((0, 0, 0), 0.13, 0.06, 2.0, 6, 'MAT_Bark', M=M)
    b.icosphere((0, 0, 5.6), 2.2, 'MAT_Leaf', subdiv=3, scale=(1, 1, 0.8), noise_amp=0.2, seed=1)
    for i in range(3):
        a = math.radians(90 + 120 * i)
        b.icosphere((1.05 * math.cos(a), 1.05 * math.sin(a), 4.5), 1.6, 'MAT_Leaf', subdiv=3, scale=(1, 1, 0.8),
                    noise_amp=0.16, seed=2 + i)
    return b.finalize()


def build_tree_b():
    b = Builder('SM_Tree_B')
    b.cyl((0, 0, 0), 0.34, 0.22, 2.4, 8, 'MAT_Bark')
    b.icosphere((0, 0, 3.55), 1.55, 'MAT_Leaf', subdiv=3, scale=(1, 1, 0.85), noise_amp=0.16, seed=7)
    b.icosphere((0.72, 0.15, 2.95), 1.05, 'MAT_Leaf', subdiv=3, scale=(1, 1, 0.8), noise_amp=0.12, seed=8)
    b.icosphere((-0.7, -0.2, 3.05), 1.05, 'MAT_Leaf', subdiv=3, scale=(1, 1, 0.8), noise_amp=0.12, seed=9)
    return b.finalize()


def build_bush():
    b = Builder('SM_Bush')
    b.icosphere((0, 0, 0.46), 0.65, 'MAT_Leaf', subdiv=3, scale=(1, 1, 0.82), noise_amp=0.08, seed=11, clamp_z0=0.0)
    return b.finalize()


def build_rock(name, size, seed):
    b = Builder(name)
    sx, sy, sz = size
    b.icosphere((0, 0, sz * 0.42), 1.0, 'MAT_Cliff', subdiv=2, scale=(sx / 2, sy / 2, sz * 0.58), noise_amp=0.17,
                seed=seed, smooth=False, clamp_z0=0.0, freq=0.9)
    return b.finalize()


def build_statue():
    b = Builder('SM_Statue')
    off = math.radians(22.5)
    b.cyl((0, 0, 0.0), 1.6, 1.6, 0.35, 8, 'MAT_Stone', smooth=False, angle_offset=off)
    b.cyl((0, 0, 0.35), 1.2, 1.2, 0.35, 8, 'MAT_Stone', smooth=False, angle_offset=off)
    b.cyl((0, 0, 0.70), 0.9, 0.9, 0.9, 8, 'MAT_Stone', smooth=False, angle_offset=off)
    z = 1.6
    b.box_z(0, 0, z, z + 0.2, 0.95, 0.95, 'MAT_Stone')
    z += 0.2
    for dx in (-0.16, 0.16):
        b.box_z(dx, 0, z, z + 0.85, 0.24, 0.28, 'MAT_Stone')
    z += 0.85
    b.cyl((0, 0, z - 0.05), 0.33, 0.37, 0.95, 8, 'MAT_Stone', smooth=False, angle_offset=off)
    b.box_z(0, 0, z + 0.75, z + 0.98, 0.95, 0.42, 'MAT_Stone')  # shoulders
    for dx in (-0.47, 0.47):
        b.box_z(dx, 0.02, z + 0.15, z + 0.9, 0.2, 0.22, 'MAT_Stone')
    b.box_z(0, -0.32, z - 0.55, z + 0.95, 0.85, 0.22, 'MAT_Stone')  # cloak
    b.cyl((0, 0, z + 0.95), 0.14, 0.14, 0.12, 8, 'MAT_Stone', smooth=False)  # neck
    b.icosphere((0, 0, z + 0.98 + 0.3), 0.28, 'MAT_Stone', subdiv=2, smooth=True)
    return b.finalize()


def build_well():
    b = Builder('SM_Well')
    b.lathe([(1.1, 0.0), (1.1, 0.9), (0.85, 0.9), (0.85, 0.0)], 12, 'MAT_Stone', smooth=False, close_ring=True)
    b.cyl((0, 0, 0.45), 0.86, 0.86, 0.04, 12, 'MAT_Soil', smooth=False)
    for x in (-0.95, 0.95):
        b.box_z(x, 0, 0.4, 3.0, 0.16, 0.16, 'MAT_Timber')
    b.cyl((-1.06, 0, 2.05), 0.06, 0.06, 2.12, 8, 'MAT_Timber', rot=Euler((0, math.pi / 2, 0)))
    b.cyl((-0.42, 0, 2.05), 0.15, 0.15, 0.84, 10, 'MAT_Wood', rot=Euler((0, math.pi / 2, 0)))
    b.box((1.14, 0, 2.05), (0.05, 0.05, 0.3), 'MAT_Metal')
    b.box((1.14, 0, 1.92), (0.05, 0.14, 0.05), 'MAT_Metal')
    b.box((0, 0, 2.55), (2.2, 0.12, 0.12), 'MAT_Timber')  # collar beam
    gable_roof(b, 2.6, 1.7, 2.5, 0.6, 0.15, 0.08, ridge='X', gable_walls=False, gable_ov=0.2)
    return b.finalize()


def build_barrel():
    b = Builder('SM_Barrel')
    b.lathe([(0.36, 0.0), (0.42, 0.45), (0.36, 0.9)], 12, 'MAT_Wood', smooth=True, caps=True)
    for z in (0.16, 0.66):
        b.cyl((0, 0, z), 0.405 if z < 0.4 else 0.405, 0.405, 0.07, 12, 'MAT_Metal', smooth=True)
    return b.finalize()


def build_crate():
    b = Builder('SM_Crate')
    b.box((0, 0, 0.4), (0.76, 0.76, 0.76), 'MAT_Wood')
    e = 0.37
    for a in (-e, e):
        for c in (-e, e):
            b.box((0, a, 0.4 + c), (0.8, 0.06, 0.06), 'MAT_Timber')
            b.box((a, 0, 0.4 + c), (0.06, 0.8, 0.06), 'MAT_Timber')
            b.box((a, c, 0.4), (0.06, 0.06, 0.8), 'MAT_Timber')
    return b.finalize()


def build_garden():
    b = Builder('SM_GardenPlot')
    b.box_z(0, 0, 0, 0.25, 3.76, 2.76, 'MAT_Soil')
    for y in (-1.5 + 0.06, 1.5 - 0.06):
        b.box_z(0, y, 0, 0.35, 4.0, 0.12, 'MAT_Wood')
    for x in (-2.0 + 0.06, 2.0 - 0.06):
        b.box_z(x, 0, 0, 0.35, 0.12, 2.76, 'MAT_Wood')
    for x in (-2.0 + 0.06, 2.0 - 0.06):
        for y in (-1.5 + 0.06, 1.5 - 0.06):
            b.box_z(x, y, 0, 0.42, 0.16, 0.16, 'MAT_Wood')
    k = 0
    for y in (-1.05, -0.35, 0.35, 1.05):
        b.bevel_box((0, y, 0.25 + 0.09), (3.3, 0.46, 0.2), 'MAT_Soil', offset=0.06, segments=1, top_only=True)
        for x in (-1.15, 0.0, 1.15):
            k += 1
            b.icosphere((x, y, 0.43 + 0.13), 0.21, 'MAT_Leaf', subdiv=1, scale=(1, 1, 0.8), noise_amp=0.05, seed=20 + k)
    return b.finalize()


def build_lantern():
    b = Builder('SM_Lantern_Post')
    b.box_z(0, 0, 0, 0.15, 0.36, 0.36, 'MAT_Stone')
    b.box_z(0, 0, 0, 3.0, 0.16, 0.16, 'MAT_Timber')
    b.pyramid((0, 0, 3.0), (0.16, 0.16), 0.08, 'MAT_Timber')
    b.box((0, 0.35, 2.85), (0.1, 0.72, 0.1), 'MAT_Timber')
    p0, p1 = Vector((0, 0.08, 2.35)), Vector((0, 0.62, 2.8))
    dv = p1 - p0
    b.box(tuple((p0 + p1) / 2), (0.08, dv.length, 0.08), 'MAT_Timber', rot=Euler((math.atan2(dv.z, dv.y), 0, 0)))
    ly = 0.6
    b.box((0, ly, 2.76), (0.03, 0.03, 0.1), 'MAT_Metal')
    b.pyramid((0, ly, 2.71), (0.37, 0.37), 0.13, 'MAT_Metal')
    b.box((0, ly, 2.68), (0.35, 0.35, 0.06), 'MAT_Metal')
    for dx in (-0.155, 0.155):
        for dy in (-0.155, 0.155):
            b.box((dx, ly + dy, 2.44), (0.04, 0.04, 0.44), 'MAT_Metal')
    b.box((0, ly, 2.205), (0.35, 0.35, 0.05), 'MAT_Metal')
    b.box((0, ly, 2.44), (0.29, 0.29, 0.40), 'MAT_Glow')
    return b.finalize()


def build_banner():
    b = Builder('SM_Banner_Post')
    b.box_z(0, 0, 0, 0.2, 0.4, 0.4, 'MAT_Stone')
    b.cyl((0, 0, 0), 0.08, 0.08, 5.0, 8, 'MAT_Timber')
    b.box((0, 0, 4.7), (1.4, 0.08, 0.08), 'MAT_Timber')
    b.icosphere((0, 0, 5.08), 0.13, 'MAT_Metal', subdiv=1)
    for dx in (-0.62, 0.62):
        b.box((dx, 0, 4.7), (0.06, 0.12, 0.12), 'MAT_Metal')
    pts = [(-0.55, 4.66), (0.55, 4.66), (0.55, 2.26), (0.0, 2.66), (-0.55, 2.26)]
    b.prism(pts, 0.13, 0.15, 'MAT_Cloth', plane='XZ')
    return b.finalize()


def build_gate_pillar():
    b = Builder('SM_Gate_Pillar')
    b.box_z(0, 0, 0, 0.3, 1.1, 1.1, 'MAT_Stone')
    b.box_z(0, 0, 0.3, 3.4, 0.9, 0.9, 'MAT_Stone')
    b.bevel_box((0, 0, 3.4 + 0.14), (1.3, 1.3, 0.28), 'MAT_Stone', offset=0.06, segments=1, top_only=True)
    b.box_z(0, 0, 3.68, 3.76, 0.5, 0.5, 'MAT_Stone')
    b.icosphere((0, 0, 3.98), 0.23, 'MAT_Stone', subdiv=2)
    return b.finalize()


def build_steps():
    b = Builder('SM_Steps_Stone')
    for i in range(3):
        y1 = 0.45 - 0.3 * i
        y0 = y1 - 0.3
        b.box((0, (y0 + y1) / 2, 0.18 * (i + 1) / 2), (2.0, 0.3, 0.18 * (i + 1)), 'MAT_Stone')
    return b.finalize()


def build_cliff_block():
    b = Builder('SM_Cliff_Block')
    cells = {(i, j) for i in range(8) for j in range(6)}
    b.voxel_block(cells, 10, 0.5, 'MAT_Cliff', noise_amp=0.25, seed=5)
    return b.finalize()


def build_cliff_corner():
    b = Builder('SM_Cliff_Corner')
    cells = {(i, j) for i in range(8) for j in range(8) if not (i < 4 and j >= 4)}
    b.voxel_block(cells, 10, 0.5, 'MAT_Cliff', noise_amp=0.25, seed=9)
    return b.finalize()


def build_signpost():
    b = Builder('SM_Signpost')
    b.box_z(0, 0, 0, 2.2, 0.14, 0.14, 'MAT_Timber')
    b.pyramid((0, 0, 2.2), (0.18, 0.18), 0.07, 'MAT_Timber')
    b.box((0, 0, 2.2), (0.2, 0.2, 0.04), 'MAT_Timber')
    pts = [(-0.45, -0.11), (0.32, -0.11), (0.45, 0.0), (0.32, 0.11), (-0.45, 0.11)]
    M1 = Matrix.Translation(Vector((0.3, 0.0, 1.8))) @ Matrix.Rotation(math.radians(30), 4, 'Z')
    b.prism(pts, -0.02, 0.02, 'MAT_Wood', plane='XZ', M=M1)
    pts2 = [(-x, z) for (x, z) in pts]
    M2 = Matrix.Translation(Vector((-0.3, 0.0, 1.5))) @ Matrix.Rotation(math.radians(-40), 4, 'Z')
    b.prism(pts2, -0.02, 0.02, 'MAT_Wood', plane='XZ', M=M2)
    return b.finalize()


def build_axistest():
    b = Builder('SM_AxisTest')
    pts = [(-0.15, 0.0), (0.15, 0.0), (0.15, 1.3), (0.4, 1.3), (0.0, 2.0), (-0.4, 1.3), (-0.15, 1.3)]
    b.prism(pts, 0.0, 0.1, 'MAT_Stone', plane='XY')
    b.box((0.8, 0.5, 0.15), (0.3, 0.3, 0.3), 'MAT_Glow')
    return b.finalize()


BUILDERS = [
    ('SM_House_A', build_house_a), ('SM_House_B', build_house_b), ('SM_House_C', build_house_c),
    ('SM_House_D', build_house_d), ('SM_Fence_Seg', build_fence), ('SM_StoneWall_Seg', build_stonewall),
    ('SM_Hedge', build_hedge), ('SM_Tree_A', build_tree_a), ('SM_Tree_B', build_tree_b), ('SM_Bush', build_bush),
    ('SM_Rock_A', lambda: build_rock('SM_Rock_A', (1.6, 1.3, 1.0), 31)),
    ('SM_Rock_B', lambda: build_rock('SM_Rock_B', (0.9, 0.7, 0.6), 37)),
    ('SM_Statue', build_statue), ('SM_Well', build_well), ('SM_Barrel', build_barrel), ('SM_Crate', build_crate),
    ('SM_GardenPlot', build_garden), ('SM_Lantern_Post', build_lantern), ('SM_Banner_Post', build_banner),
    ('SM_Gate_Pillar', build_gate_pillar), ('SM_Steps_Stone', build_steps), ('SM_Cliff_Block', build_cliff_block),
    ('SM_Cliff_Corner', build_cliff_corner), ('SM_Signpost', build_signpost), ('SM_AxisTest', build_axistest),
]


def export(ob):
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(OUTDIR, ob.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_NONE', axis_forward='-Y', axis_up='Z',
                             mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False,
                             bake_anim=False, path_mode='COPY', embed_textures=False, object_types={'MESH'},
                             use_custom_props=False)
    print("EXPORTED", path, os.path.getsize(path))


for name, fn in BUILDERS:
    if ONLY and name not in ONLY:
        continue
    ob = fn()
    export(ob)
    ob.hide_set(True)
print("ALL DONE")
