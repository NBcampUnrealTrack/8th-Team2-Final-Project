"""Boss phase-2 attack VFX (party LEFT, boss RIGHT; attacks travel leftward)."""
from core import *

R = random.Random


# =============================================================================
# 1. 앞발 짓누르기
# =============================================================================
def stomp_telegraph():
    W, H, N = 512, 224, 16
    cx, cy, RX = 256, 124, 224
    RY = RX * GROUND_K
    rng = R(1)
    motes = [(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.random(), rng.uniform(.7, 1.3)) for _ in range(40)]
    frames = []
    for f in range(N):
        fr = Frame(W, H)
        ph = f / N
        pulse = .5 + .5 * math.cos(ph * 2 * math.pi)
        # danger fill
        b = fr.box(cx - RX, cy - RY, cx + RX, cy + RY)
        sl, X, Y = b
        q, d = ellipse_q(X, Y, cx, cy, RX, RY)
        fill = sm(-d, 2) * (0.05 + 0.10 * q ** 3)
        fr.emit(sl, EARTH[1], fill * (0.8 + 0.4 * pulse))
        # boundary ring
        ring(fr, cx, cy, RX, RY, 4, EARTH, .9 + .4 * pulse, soft=4, edge_bias=1)
        ring(fr, cx, cy, RX - 14, (RX - 14) * GROUND_K, 1.5, EARTH, .35)
        # contracting pulses (incoming impact)
        for k in range(2):
            p = (ph + k * .5) % 1
            r = RX * (1 - ease_in(p, 1.5)) + 6
            ring(fr, cx, cy, r, r * GROUND_K, 2 + 3 * p, EARTH, .15 + .8 * p, soft=3, edge_bias=.6)
        # rotating diamond markers on the boundary
        for k in range(8):
            a = k * math.pi / 4 + ph * math.pi / 4
            x, y = cx + math.cos(a) * RX, cy + math.sin(a) * RY
            star4(fr, x, y, 9 + 4 * pulse, EARTH, 1.1, rot=0, thin=.2, h_stretch=1.5)
        # centre mark
        glow(fr, cx, cy, 12, EARTH[2], .3 + .4 * pulse, rx_k=2.2)
        # rising motes
        for (u, v, o, sp) in motes:
            if u * u + v * v > 1:
                continue
            t = (ph * sp + o) % 1
            x = cx + u * RX * .95 + math.sin(t * 6 + o * 9) * 3
            y = cy + v * RY * .95 - t * 60
            dot(fr, x, y, 1.6, EARTH, (1 - t) * 1.4 * sstep(0, .15, t))
        fr.bake_glow(.5, 4, .3, 14)
        frames.append(fr)
    return frames


def stomp_impact_glow():
    W, H, N = 512, 352, 16
    cx, cy = 256, 256
    rng = R(4)
    spikes = []
    for i in range(14):
        a = math.radians(rng.uniform(-172, -8))
        spikes.append((a, rng.uniform(95, 215) * (1.15 if abs(a + math.pi / 2) < .6 else .75), rng.uniform(7, 14),
                       rng.uniform(-10, 10)))
    sparks = [dict(x=cx + rng.uniform(-20, 20), y=cy - rng.uniform(0, 8), vx=rng.uniform(-9, 9), vy=-rng.uniform(4, 13))
              for _ in range(34)]
    frames = []
    for f in range(N):
        fr = Frame(W, H)
        # flash + spikes
        if f <= 4:
            k = [1, .85, .55, .3, .12][f]
            glow(fr, cx, cy - 10, 34 * (1 + f * .2), EARTH[2], 1.0 * k, rx_k=1.9)
            glow(fr, cx, cy - 6, 15, EARTH[4], 1.8 * k, rx_k=2.2)
            grow = [0.55, 1.0, 1.0, .8, .45][f]
            for a, L, w, ox in spikes:
                spike(fr, cx + ox, cy - 4, a, L * grow, w * (1.2 - f * .18), EARTH, 1.5 * k)
        # ground flash ellipse
        if f <= 6:
            k = 1 - f / 7
            b = fr.box(0, 0, W, H); sl, X, Y = b
            q, d = ellipse_q(X, Y, cx, cy, 110 + f * 18, (110 + f * 18) * GROUND_K)
            fr.emit(sl, EARTH[1], np.exp(-np.maximum(q, 0) ** 2 * 2.5) * .55 * k)
        # shockwave ring
        if 1 <= f <= 12:
            p = (f - 1) / 11
            rx = 40 + 205 * ease_out(p, 2.4)
            ring(fr, cx, cy, rx, rx * GROUND_K, 14 * (1 - p) + 2, EARTH, 1.3 * (1 - p) ** 1.2, soft=6, edge_bias=1)
            if p < .6:
                rr = rx * .78
                ring(fr, cx, cy, rr, rr * GROUND_K, 3, EARTH, .7 * (1 - p / .6), soft=3, edge_bias=.8)
        # sparks
        for s in sparks:
            if f >= 1:
                life = 1 - f / N
                streak(fr, s['x'], s['y'], s['vx'], s['vy'], math.hypot(s['vx'], s['vy']) * 1.6 + 2, 1.8, EARTH, 1.5 * life)
                s['x'] += s['vx']; s['y'] += s['vy']; s['vy'] += 1.3; s['vx'] *= .92
        fr.bake_glow(.6, 5, .4, 18)
        frames.append(fr)
    return frames


def _crack_net(rng, cx, cy, RX, n, L, k=GROUND_K):
    cracks = []
    for i in range(n):
        a = rng.uniform(0, 2 * math.pi) if n > 7 else (i / n) * 2 * math.pi + rng.uniform(-.3, .3)
        x, y = cx, cy
        pts = [(x, y)]
        ln = L * rng.uniform(.7, 1.2)
        for s in range(7):
            a += rng.uniform(-.45, .45)
            x += math.cos(a) * ln / 7
            y += math.sin(a) * ln / 7 * k
            pts.append((x, y))
        cracks.append(pts)
    return cracks


# ---------------- ground fracture (straight, angular breaks) ----------------
def _pt_at(pts, t):
    seg = [math.hypot(x1 - x0, y1 - y0) for (x0, y0), (x1, y1) in zip(pts, pts[1:])]
    tot = sum(seg) * t
    out = [pts[0]]
    for (p0, p1), L in zip(zip(pts, pts[1:]), seg):
        if tot >= L:
            out.append(p1); tot -= L
        else:
            f = tot / max(L, 1e-6)
            out.append((p0[0] + (p1[0] - p0[0]) * f, p0[1] + (p1[1] - p0[1]) * f))
            break
    return out


def fracture(rng, cx, cy, R, n=8, k=GROUND_K):
    rays, branches, rings = [], [], []
    a0 = rng.uniform(0, 6.28)
    for i in range(n):
        ang = a0 + i * 2 * math.pi / n + rng.uniform(-.25, .25)
        L = R * rng.uniform(.65, 1.05)
        ns = rng.randint(2, 4)
        x, y = cx, cy
        pts = [(x, y)]
        for sgi in range(ns):
            seg = L / ns * rng.uniform(.8, 1.2)
            x += math.cos(ang) * seg; y += math.sin(ang) * seg * k
            pts.append((x, y))
            if sgi < ns - 1 and rng.random() < .65:
                ba = ang + rng.choice([-1, 1]) * rng.uniform(.5, 1.0)
                bl = seg * rng.uniform(.35, .7)
                branches.append([(x, y), (x + math.cos(ba) * bl, y + math.sin(ba) * bl * k)])
            ang += rng.uniform(-.5, .5)
        rays.append(pts)
    for fr_ in (.32, .62):
        for i in range(n):
            if rng.random() < .7:
                p1 = _pt_at(rays[i], fr_)[-1]; p2 = _pt_at(rays[(i + 1) % n], fr_ * rng.uniform(.85, 1.15))[-1]
                mid = ((p1[0] + p2[0]) / 2 + rng.uniform(-4, 4), (p1[1] + p2[1]) / 2 + rng.uniform(-2, 2))
                rings.append([p1, mid, p2])
    return dict(rays=rays, branches=branches, rings=rings, c=(cx, cy), R=R)


def draw_fracture(fr, F, g=1.0, a=1.0, scale=1.0):
    """Decal: dark gaps + lit broken lip (translucent layer)."""
    w = scale
    for pts in F['rays']:
        pp = _pt_at(pts, min(1, g))
        polyline_decal(fr, pp, 9 * w, (0.16, 0.11, 0.08), .5 * a)
        polyline_decal(fr, [(x, y - 3 * w) for x, y in pp], 1.6 * w, DUST['light'], .3 * a)
        polyline_decal(fr, pp, 4.5 * w, (0.05, 0.035, 0.03), .97 * a)
    if g > .45:
        ga = min(1, (g - .45) / .35) * a
        for pts in F['branches']:
            polyline_decal(fr, pts, 4 * w, (0.20, 0.14, 0.10), .4 * ga)
            polyline_decal(fr, pts, 2.0 * w, (0.06, 0.04, 0.035), .9 * ga)
    if g > .7:
        ga = min(1, (g - .7) / .3) * a
        for pts in F['rings']:
            polyline_decal(fr, [(x, y - 1.5 * w) for x, y in pts], 1.4 * w, DUST['light'], .3 * ga)
            polyline_decal(fr, pts, 2.2 * w, (0.07, 0.05, 0.04), .85 * ga)


def emit_fracture(fr, F, g=1.0, inten=1.0, rings=True):
    for pts in F['rays']:
        polyline_emit(fr, _pt_at(pts, min(1, g)), 1.3, EARTH, inten)
    if g > .45:
        for pts in F['branches']:
            polyline_emit(fr, pts, 1.0, EARTH, inten * .6)
    if rings and g > .7:
        for pts in F['rings']:
            polyline_emit(fr, pts, 1.0, EARTH, inten * .4)


def crater(fr, cx, cy, r, a=1.0):
    sl, X, Y = fr.box(cx - r * 1.3, cy - r * GROUND_K * 1.6, cx + r * 1.3, cy + r * GROUND_K * 1.6)
    q, d = ellipse_q(X, Y, cx, cy, r, r * GROUND_K)
    fr.over(sl, (0.08, 0.06, 0.05), sm(-d, 3) * .55 * a)
    # near rim catches light, far rim in shadow
    rim_n = sm(3 - np.abs(d), 1) * (Y > cy)
    fr.over(sl, DUST['light'], rim_n * .5 * a)


def stomp_impact_dust():
    W, H, N = 512, 320, 24
    cx, cy = 256, 236
    rng = R(8)
    puffs = []
    for i in range(30):
        a = i / 30 * 2 * math.pi + rng.uniform(-.1, .1)
        sp = rng.uniform(6, 11) * (1 if abs(math.cos(a)) > .3 else .7)
        puffs.append(dict(vx=sp * math.cos(a), vy=sp * math.sin(a) * GROUND_K - rng.uniform(.3, 1.0),
                          r0=rng.uniform(10, 17), delay=rng.choice([0, 0, 1, 1, 2]), seed=i, life=rng.randint(17, 23)))
    for i in range(5):
        puffs.append(dict(vx=rng.uniform(-1.5, 1.5), vy=-rng.uniform(3, 5), r0=rng.uniform(12, 18), delay=1,
                          seed=40 + i, life=20))
    rocks = []   # debris chunks are pixel-art particles (T_Boss2_RockAtlas) spawned by the engine/preview
    leaves = [dict(x=cx + rng.uniform(-30, 30), y=cy - 12, vx=rng.uniform(-6, 6), vy=-rng.uniform(6, 12),
                   ph=rng.uniform(0, 6), spr=make_leaf(rng.uniform(9, 14))) for i in range(12)]
    F = fracture(rng, cx, cy, 150, 9)
    frames = []
    for f in range(N):
        fr = Frame(W, H)
        # crack decals
        if f >= 1:
            ca = 1 if f < 16 else 1 - (f - 15) / 9
            crater(fr, cx, cy, 30, ca)
            draw_fracture(fr, F, g=min(1, f / 5), a=ca, scale=.8)
        # rocks
        for r in rocks:
            if f >= 1 and r['y'] < cy + 30:
                a = 1 if f < 18 else 1 - (f - 17) / 7
                sprite_over(fr, r['spr'], r['x'], r['y'], a, r['rot'])
            if f >= 1:
                r['x'] += r['vx']; r['y'] += r['vy']; r['vy'] += 1.5; r['rot'] += r['vr']
        # leaves
        for l in leaves:
            if f >= 1:
                flip = math.cos(l['ph'] + f * .9)
                a = 1 if f < 18 else 1 - (f - 17) / 7
                sprite_over(fr, l['spr'], l['x'], l['y'], a, rot=l['ph'] + f * .35, sy=max(.25, abs(flip)) * (1 if flip > 0 else -1))
                l['x'] += l['vx'] + math.sin(f * .7 + l['ph']) * 1.5
                l['y'] += l['vy']; l['vy'] = min(l['vy'] + 1.2, 1.8); l['vx'] *= .9
        # dust (front-most)
        for p_ in sorted(puffs, key=lambda q_: q_['vy']):
            t = f - 1 - p_['delay']
            if t < 0 or t > p_['life']:
                continue
            p = t / p_['life']
            e = ease_out(p, 2.6)
            x = cx + p_['vx'] * 18 * e
            y = cy - 4 + p_['vy'] * 18 * e - 18 * p
            r = p_['r0'] * (0.6 + 1.6 * ease_out(p, 1.8))
            puff(fr, x, y, r, alpha=1.0, erode=max(0, (p - .45) * 1.6), seed=p_['seed'], ox=f * 2)
        frames.append(fr)
    return frames


# =============================================================================
# 2. 뿌리 후려치기 (boss on the right sweeps leftward across the party)
# =============================================================================
SW_W, SW_H = 768, 224
SW_C, SW_RX, SW_RY = (384, 4), 360, 150   # ellipse centred at the boss (top edge); near side passes the party
SW_HEAD = [8, 10, 14, 30, 55, 80, 105, 130, 150, 162, 168, 171, 172, 173, 173, 173]   # deg, right -> front -> left
SW_TAIL = [6, 8, 10, 24, 42, 60, 72, 80, 78, 68, 54, 40, 26, 14, 6, 2]
SW_THICK = 22.0


def _arc_pt(phi, dr=0.0):
    return SW_C[0] + (SW_RX + dr) * math.cos(phi), SW_C[1] + (SW_RY + dr * SW_RY / SW_RX) * math.sin(phi)


def _arc_tan(phi):
    tx, ty = -math.sin(phi) * SW_RX, math.cos(phi) * SW_RY   # direction of increasing angle
    tl = math.hypot(tx, ty)
    return tx / tl, ty / tl


def root_lash_swing():
    W, H, N = SW_W, SW_H, 16
    cx, cy = SW_C
    rng = R(12)
    sparks = []
    frames = []
    X, Y = grid(W, H)
    q, dpx = ellipse_q(X, Y, cx, cy, SW_RX, SW_RY)
    phi = np.arctan2((Y - cy) / SW_RY, (X - cx) / SW_RX)
    near = np.clip(np.sin(phi), 0, 1)  # 1 = closest to camera
    for f in range(N):
        fr = Frame(W, H)
        head = math.radians(SW_HEAD[f]); L = math.radians(SW_TAIL[f])
        fade = 1 if f < 10 else max(0, 1 - (f - 9) / 6)
        rel = np.angle(np.exp(1j * (head - phi)))  # tail side has rel>0
        s_ = 1 - rel / max(L, 1e-3)
        on = (rel >= -0.02) & (rel <= L)
        sc = np.clip(s_, 0, 1)
        thick = SW_THICK * (0.55 + 0.6 * near) * (sc ** .75) * sstep(1.0, .8, sc) ** .7 * (1 if f > 2 else .5)
        cov = sm(thick - np.abs(dpx), 1.2) * on * (thick > .3)
        u = dpx / np.maximum(thick, 1e-3)
        heat = np.clip(0.5 + 0.5 * u, 0, 1) ** 1.4 * (0.3 + 0.7 * sc)
        col = ramp(LIFE, 0.12 + 0.72 * heat)
        inten = (0.4 + 1.1 * sc ** 1.4) * cov * fade * (0.55 + 0.45 * sstep(-1, .3, u))
        lines = (np.sin(dpx * 1.1) > .6) * (u < .2) * (sc > .15) * .4
        fr.add += col * (inten * (1 + lines))[..., None]
        rim = sm(1.6 - np.abs(dpx - thick * .82), 1) * on * (sc > .2)
        fr.add += np.array(LIFE[3], f32) * (rim * 1.1 * sc * fade)[..., None]
        if 2 <= f <= 10:
            for _ in range(9):
                a = head - L * rng.uniform(0, .5)
                x, y = _arc_pt(a, rng.uniform(-10, 16))
                tx, ty = _arc_tan(a)
                sparks.append(dict(x=x, y=y, vx=tx * rng.uniform(3, 8) + rng.uniform(-1.5, 1.5),
                                   vy=ty * rng.uniform(2, 5) + rng.uniform(-3, 0), born=f, life=rng.randint(5, 9)))
        for sp in sparks:
            age = f - sp['born']
            if 0 <= age <= sp['life']:
                streak(fr, sp['x'], sp['y'], sp['vx'], sp['vy'], 8, 1.5, LIFE, 1.6 * (1 - age / sp['life']))
                sp['x'] += sp['vx']; sp['y'] += sp['vy']; sp['vy'] += .5
        if 3 <= f <= 8:
            x, y = _arc_pt(head, 4)
            glow(fr, x, y, 12, LIFE[3], 1.4)
        fr.bake_glow(.55, 4, .3, 12)
        frames.append(fr)
    return frames


def root_lash_leaves():
    W, H, N = SW_W, SW_H + 64, 16
    rng = R(13)
    leaves = []
    frames = []
    for f in range(N):
        fr = Frame(W, H)
        head = math.radians(SW_HEAD[f]); L = math.radians(SW_TAIL[f])
        if 3 <= f <= 9:
            for _ in range(5):
                a = head - L * rng.uniform(.05, .55)
                x, y = _arc_pt(a, rng.uniform(-12, 10))
                tx, ty = _arc_tan(a)
                leaves.append(dict(x=x, y=y, vx=tx * rng.uniform(2, 6), vy=ty * rng.uniform(1, 4) - 1,
                                   ph=rng.uniform(0, 6), born=f, spr=make_leaf(rng.uniform(8, 13), rng.randint(0, 99)),
                                   flower=rng.random() < .15))
        for l in leaves:
            age = f - l['born']
            if age < 0:
                continue
            a = max(0, 1 - max(0, age - 4) / 6)
            if a <= 0:
                continue
            flip = math.cos(l['ph'] + f * 1.1)
            spr = l['spr'] if not l['flower'] else _FLOWER
            sprite_over(fr, spr, l['x'], l['y'], a, rot=l['ph'] + f * .4, sy=max(.25, abs(flip)) * (1 if flip > 0 else -1))
            l['x'] += l['vx'] + math.sin(f + l['ph']) * 1.2; l['y'] += l['vy']; l['vy'] = min(l['vy'] + .5, 1.6); l['vx'] *= .92
        frames.append(fr)
    return frames


_FLOWER = make_flower(5)


def root_lash_hit():
    W, H, N = 256, 256, 12
    c = 128
    rng = R(21)
    spikes = [(math.radians(180 + rng.uniform(-75, 75)), rng.uniform(50, 110), rng.uniform(5, 10)) for _ in range(9)]
    spikes += [(math.radians(rng.uniform(-60, 60)), rng.uniform(20, 45), rng.uniform(3, 6)) for _ in range(4)]
    sparks = [dict(x=c, y=c, a=math.radians(180 + rng.uniform(-80, 80)), sp=rng.uniform(6, 13)) for _ in range(18)]
    frames = []
    for f in range(N):
        fr = Frame(W, H)
        if f <= 4:
            k = [1, .9, .6, .3, .1][f]
            glow(fr, c, c, 30 + f * 6, LIFE[3], 2.4 * k)
            glow(fr, c, c, 12, LIFE[4], 3 * k)
            g = [.6, 1, 1, .7, .4][f]
            for a, L_, w in spikes:
                spike(fr, c, c, a, L_ * g, w * (1.2 - f * .2), LIFE, 2.2 * k)
            star4(fr, c, c, 40 * g, LIFE, 2 * k, rot=.2)
        if 1 <= f <= 9:
            p = (f - 1) / 8
            r = 18 + 90 * ease_out(p, 2.2)
            ring(fr, c, c, r, r * .85, 8 * (1 - p) + 1.5, LIFE, 1.8 * (1 - p), soft=3, edge_bias=1)
        for s in sparks:
            if f >= 1:
                d = s['sp'] * f
                x, y = c + math.cos(s['a']) * d, c + math.sin(s['a']) * d + f * f * .35
                streak(fr, x, y, math.cos(s['a']) * s['sp'], math.sin(s['a']) * s['sp'] + f * .7, 10, 1.5, LIFE, 1.6 * (1 - f / N))
        fr.bake_glow(.6, 4, .35, 12)
        frames.append(fr)
    return frames


def parry_cue():
    W, H, N = 256, 256, 16
    c = 128
    frames = []
    size = [8, 30, 56, 70, 66, 58, 50, 42, 34, 27, 20, 14, 9, 5, 2, 0]
    for f in range(N):
        fr = Frame(W, H)
        s = size[f]
        k = 1.0 if f < 6 else (1 - (f - 6) / 10)
        if s > 0:
            glow(fr, c, c, 10 + s * .25, (1, .98, .85), 2.6 * k)
            star4(fr, c, c, s, [(1, .8, .3), (1, .9, .55), (1, 1, .85), (1, 1, 1), (1, 1, 1)], 3.0 * k,
                  rot=0, thin=.07, h_stretch=1.9)
            star4(fr, c, c, s * .45, LIFE, 1.6 * k, rot=math.pi / 4, thin=.1, h_stretch=1.0)
        if 1 <= f <= 9:
            p = (f - 1) / 8
            r = 14 + 80 * ease_out(p, 2.5)
            ring(fr, c, c, r, r, 3.5 * (1 - p) + 1, [(1, .8, .3), (1, .95, .6), (1, 1, .9), (1, 1, 1), (1, 1, 1)],
                 2.2 * (1 - p), soft=2, edge_bias=1)
        for j in range(6):  # small diamond sparkles orbiting
            if 2 <= f <= 12:
                a = j * math.pi / 3 + f * .15
                rr = 26 + f * 3.2
                star4(fr, c + math.cos(a) * rr, c + math.sin(a) * rr, 6 * (1 - f / 14), LIFE, 1.6, thin=.18, h_stretch=1)
        fr.bake_glow(.65, 4, .45, 14)
        frames.append(fr)
    return frames


# =============================================================================
# 3. 옹이 잔광
# =============================================================================
def _flame_mask(X, Y, cx, cy, h, w, ph, n_off=0):
    """teardrop flame pointing up, base at (cx,cy). returns signed px distance-ish."""
    u = (cy - Y) / h  # 0 base -> 1 tip
    nz = sample_noise(X, Y * .7 + ph * 40, 2.0, n_off, 0, seed=3, sigma=4) - .5
    xx = (X - cx) - np.sin(u * 3 + ph * 6.28) * u * w * .35 - nz * w * .6 * u
    half = w * np.sin(np.pi * np.clip(u * .9 + .1, 0, 1)) * np.clip(1 - u, 0, 1) ** .45
    half = np.where((u >= -0.35) & (u <= 1), np.where(u < 0, w * np.sqrt(np.clip(1 - (u / .35) ** 2, 0, 1)), half), -50)
    return half - np.abs(xx)


def knot_charge():
    W, H, N = 256, 256, 16
    cx, cy = 128, 150
    rng = R(31)
    motes = [dict(a=rng.uniform(0, 2 * math.pi), r0=rng.uniform(70, 120), o=rng.random(), sp=rng.uniform(.8, 1.3),
                  spin=rng.choice([-1, 1]) * rng.uniform(.6, 1.4)) for _ in range(26)]
    frames = []
    for f in range(N):
        fr = Frame(W, H)
        ph = f / N
        pulse = .5 + .5 * math.sin(ph * 2 * math.pi)
        glow(fr, cx, cy - 18, 30 + pulse * 6, LIFE[1], .4 + .2 * pulse)
        # god rays
        sl, X, Y = fr.box(0, 0, W, H)
        ang = np.arctan2(Y - (cy - 18), X - cx)
        d = np.hypot(X - cx, Y - (cy - 18))
        rays = sample_noise(ang * 40 + 300, np.full_like(ang, ph * 60), 1.0, 0, 0, seed=5, sigma=3)
        rays = np.clip((rays - .45) * 3, 0, 1) * np.exp(-(d / (48 + 12 * pulse)) ** 1.5) * sstep(8, 26, d) * sstep(118, 80, d)
        fr.emit(sl, LIFE[2], rays * .9)
        # tri-lobed flame core
        for (ox, hh, ww, inten, st) in ((0, 64, 22, .9, .45), (-13, 38, 10, .8, .55), (13, 42, 10, .8, .55),
                                         (0, 46, 14, 1.4, .75), (0, 26, 7, 2.4, .95)):
            m = _flame_mask(X, Y, cx + ox, cy, hh * (1 + .12 * pulse), ww, ph, n_off=ox * 7)
            fr.emit(sl, ramp(LIFE, np.full(m.shape, st))[0, 0], sm(m, 1.5) * inten)
        # converging motes
        for m_ in motes:
            t = (ph * m_['sp'] + m_['o']) % 1
            r = m_['r0'] * (1 - ease_in(t, 1.6)) + 4
            a = m_['a'] + t * m_['spin'] * 2.2
            x, y = cx + math.cos(a) * r, cy - 18 + math.sin(a) * r * .8
            va = a + math.pi / 2 * (1 if m_['spin'] > 0 else -1)
            spd = m_['r0'] * .05
            vx = -math.cos(a) * spd + math.cos(va) * spd * .5
            vy = -math.sin(a) * spd + math.sin(va) * spd * .5
            streak(fr, x, y, vx, vy, 8 + 10 * t, 1.5, LIFE, 1.8 * sstep(0, .2, t) * (1 - t * .3))
        fr.bake_glow(.6, 4, .4, 14)
        frames.append(fr)
    return frames


KP_ORIGIN = (256, 64)
KP_DIR = math.radians(96)   # toward the camera (down-screen), slightly left to the party
KP_KY = 0.62                # depth squash of the wavefront


def knot_push():
    W, H, N = 512, 400, 16
    ox, oy = KP_ORIGIN
    spread = math.radians(40)
    rng = R(33)
    streaks_ = [dict(off=rng.uniform(-1, 1), d0=rng.uniform(-60, 30), sp=rng.uniform(22, 32), ln=rng.uniform(18, 46))
                for _ in range(30)]
    motes = [dict(off=rng.uniform(-1, 1), sp=rng.uniform(12, 22), o=rng.uniform(0, 40)) for _ in range(44)]
    frames = []
    X, Y = grid(W, H)
    dx, dy = X - ox, (Y - oy) / KP_KY
    dist = np.hypot(dx, dy)
    ang = np.arctan2(dy, dx)
    rel = np.angle(np.exp(1j * (ang - KP_DIR)))
    cone = sstep(spread, spread * .5, np.abs(rel))

    def P(a, d):
        return ox + math.cos(a) * d, oy + math.sin(a) * d * KP_KY

    for f in range(N):
        fr = Frame(W, H)
        sl = (slice(0, H), slice(0, W))
        if f <= 5:
            k = [1, .95, .7, .45, .25, .1][f]
            glow(fr, ox, oy, 22 + f * 6, LIFE[3], 2.4 * k)
            glow(fr, ox, oy, 9, LIFE[4], 3.0 * k)
            for j in range(12):
                a = KP_DIR + (j - 5.5) * .2
                L_ = (50 + 40 * (j % 3 == 1)) * [.5, 1, 1, .8, .5, .3][f]
                x1, y1 = P(a, L_)
                spike(fr, ox, oy, math.atan2(y1 - oy, x1 - ox), math.hypot(x1 - ox, y1 - oy), 6, LIFE, 1.8 * k)
        if f <= 10:
            flood = cone * np.exp(-dist / 160) * sstep(10, 40, dist) * sstep(30 + f * 36, f * 36 - 20, dist) * (1 - f / 11) * .22
            fr.emit(sl, LIFE[1], flood)
        for i, (lag, gain) in enumerate(((0, 1.0), (3, .8), (6, .55))):
            t = f - lag
            if t < 0:
                continue
            r = 24 + t * 36 * (1 - t / 50)
            fade = sstep(480, 300, r) * sstep(-1, 1.5, t)
            if fade <= 0:
                continue
            th = 8 + r * .06          # wavefront thickens as it nears the camera
            band = dist - r
            lead = sm(-band, 1.2) * sm(band + 3 + r * .015, 1.2)
            wake = np.exp(-np.maximum(-band, 0) / th) * sm(-band, 1.2) * sm(band + th * 3, 2)
            shape = cone * (lead * 1.6 + wake * .7) * fade * gain
            heat = np.clip(lead + wake * .4, 0, 1)
            fr.add += ramp(LIFE, .35 + .65 * heat) * (shape * 1.4)[..., None]
        for s in streaks_:
            d = s['d0'] + f * s['sp']
            if d < 20 or d > 470:
                continue
            a = KP_DIR + s['off'] * spread * .9
            x, y = P(a, d); x2, y2 = P(a, d - s['ln'] * (1 + d / 300))
            spike(fr, x, y, math.atan2(y2 - y, x2 - x), math.hypot(x2 - x, y2 - y), 1.6 + d / 200, LIFE,
                  1.3 * (1 - f / N) * sstep(20, 60, d))
        for m in motes:
            d = f * m['sp'] - m['o']
            if d > 10:
                x, y = P(KP_DIR + m['off'] * spread, d)
                dot(fr, x, y + math.sin(d * .05 + m['o']) * 3, 1.4 + d / 160, LIFE, 1.5 * (1 - f / N))
        fr.bake_glow(.6, 5, .4, 16)
        frames.append(fr)
    return frames


# =============================================================================
# 4. 언덕 무너뜨리기
# =============================================================================
def _crack_field(rng, cx, cy, RX, n_epi, L):
    out = []
    for i in range(n_epi):
        a = rng.uniform(0, 2 * math.pi); rr = math.sqrt(rng.uniform(0, 1))
        ex, ey = cx + math.cos(a) * RX * rr, cy + math.sin(a) * RX * GROUND_K * rr
        out += _crack_net(rng, ex, ey, 0, rng.randint(2, 4), L * rng.uniform(.6, 1.1))
    return out


def hill_tel1():
    W, H, N = 512, 224, 16
    cx, cy = 256, 150
    rng = R(41)
    F = fracture(rng, cx, cy, 200, 9)
    pebbles = [dict(x=rng.uniform(60, 450), y=cy + rng.uniform(-30, 30), o=rng.randint(0, 15),
                    spr=make_rock(rng.uniform(2.5, 5), 300 + i, moss=False)) for i in range(18)]
    motes = [(rng.uniform(40, 470), rng.uniform(-1, 1), rng.random(), rng.uniform(.6, 1.2)) for _ in range(30)]
    tr, ad = [], []
    for f in range(N):
        t_ = Frame(W, H); a_ = Frame(W, H)
        ph = f / N
        pulse = .5 + .5 * math.sin(ph * 2 * math.pi)
        g = .5 + .06 * math.sin(ph * 2 * math.pi)
        draw_fracture(t_, F, g=g, a=1, scale=.7)
        emit_fracture(a_, F, g=g, inten=.35 + .4 * pulse, rings=False)
        for (x, v, o, sp) in motes:
            t = (ph * sp + o) % 1
            y = cy + v * 34 - t * 90
            dot(a_, x + math.sin(t * 7 + o * 9) * 3, y, 1.5, EARTH, 1.2 * (1 - t) * sstep(0, .2, t))
        for i, pts in enumerate(F['rays'][::2]):
            t = ((f + i * 5) % N) / N
            px_, py_ = _pt_at(pts, g)[-1]
            if t < .7:
                puff(t_, px_ + t * 6, py_ - 4 - t * 22, 5 + t * 9, 1, erode=max(0, (t - .3) * 2), seed=i)
        a_.bake_glow(.6, 3, .35, 10)
        tr.append(t_); ad.append(a_)
    return ad, tr


def hill_tel2():
    W, H, N = 512, 320, 16
    cx, cy = 256, 226
    RX = 228; RY = RX * GROUND_K
    rng = R(42)
    F = fracture(rng, cx, cy, 236, 10)
    pillars = [(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.random()) for _ in range(9)]
    rocks = [dict(x=rng.uniform(40, 470), o=rng.random(), sp=rng.uniform(.8, 1.3),
                  spr=make_rock(rng.uniform(3, 7), 400 + i, moss=i % 4 == 0)) for i in range(16)]
    tr, ad = [], []
    for f in range(N):
        t_ = Frame(W, H); a_ = Frame(W, H)
        ph = f / N
        pulse = .5 + .5 * math.sin(ph * 2 * math.pi)
        # the hill's growing shadow
        sl, X, Y = t_.box(0, 0, W, H)
        q, d = ellipse_q(X, Y, cx, cy, RX * (.96 + .04 * pulse), RY * (.96 + .04 * pulse))
        t_.over(sl, (0.06, 0.05, 0.05), sm(-d, 18) * (.38 + .1 * pulse))
        draw_fracture(t_, F, g=1.0, a=1, scale=.9)
        emit_fracture(a_, F, g=1.0, inten=.8 + .7 * pulse)
        # danger ring
        ring(a_, cx, cy, RX, RY, 5, EARTH, 1.4 + .8 * pulse, soft=5, edge_bias=1)
        sl2, X2, Y2 = a_.box(0, 0, W, H)
        q2, d2 = ellipse_q(X2, Y2, cx, cy, RX, RY)
        a_.emit(sl2, EARTH[1], sm(-d2, 3) * q2 ** 4 * (.18 + .12 * pulse))
        # light pillars from the ground
        for (u, v, o) in pillars:
            if u * u + v * v > .85:
                continue
            x, y = cx + u * RX, cy + v * RY
            fl = .5 + .5 * math.sin((ph + o) * 2 * math.pi * 2)
            b = a_.box(x - 14, y - 150, x + 14, y + 4)
            if b:
                s3, X3, Y3 = b
                m = np.exp(-((X3 - x) / (3 + 4 * fl)) ** 2) * sstep(y - 150, y - 30, Y3) ** 2 * sstep(y + 4, y - 2, Y3)
                a_.emit(s3, EARTH[2], m * (.25 + .45 * fl))
        # falling pebbles
        a_.bake_glow(.6, 4, .4, 14)
        tr.append(t_); ad.append(a_)
    return ad, tr


# boulder plan shared with the particle emitter / preview: (x in frame px, radius, landing frame @24fps, atlas idx)
HILL_SPEC = [(256, 58, 7, 0), (150, 30, 4, 2), (360, 34, 5, 1), (90, 20, 9, 3), (420, 22, 8, 3), (310, 16, 10, 4),
             (200, 15, 11, 5)]


def hill_impact():
    W, H, N = 512, 416, 24
    cx, gy = 256, 300
    rng = R(51)
    spec = HILL_SPEC
    boulders = [dict(x=x, R=r, land=lf) for x, r, lf, s_ in spec]
    F = fracture(R(52), cx, gy, 250, 11)
    puffs, leaves, sparks = [], [], []
    ad, tr = [], []
    for f in range(N):
        t_ = Frame(W, H); a_ = Frame(W, H)
        if f >= boulders[0]['land']:
            tt = f - boulders[0]['land']
            fa = 1 if f < 18 else 1 - (f - 17) / 7
            crater(t_, cx, gy, 70, fa)
            draw_fracture(t_, F, g=min(1, tt / 4), a=fa, scale=1.0)
            if tt <= 8:
                emit_fracture(a_, F, g=min(1, tt / 4), inten=1.3 * (1 - tt / 9))
        for b in boulders:
            if f == b['land']:
                big = b['R'] / 30
                for k in range(int(6 + b['R'] * .45)):
                    side = -1 if k % 2 else 1
                    puffs.append(dict(x=b['x'] + side * rng.uniform(0, b['R'] * .6), y=gy - 4,
                                      vx=side * rng.uniform(1.5, 8) * big, vy=-rng.uniform(.5, 3.5) * big,
                                      r=rng.uniform(10, 18) * min(1.4, big) + 5, born=f, life=rng.randint(13, 17), seed=k + f * 7))
                for k in range(5):
                    leaves.append(dict(x=b['x'], y=gy - b['R'], vx=rng.uniform(-7, 7), vy=-rng.uniform(8, 13),
                                       ph=rng.uniform(0, 6), spr=make_leaf(rng.uniform(10, 15)) if k % 4 else _FLOWER))
                for k in range(int(8 * big + 4)):
                    sparks.append(dict(x=b['x'], y=gy - 6, vx=rng.uniform(-14, 14) * big, vy=-rng.uniform(5, 16) * big, born=f))
        main = boulders[0]['land']
        t = f - main
        if 0 <= t <= 5:
            k = [1, .85, .6, .4, .2, .08][t]
            glow(a_, cx, gy - 20, 70 + t * 12, EARTH[2], 1.1 * k, rx_k=1.7)
            glow(a_, cx, gy - 10, 26, EARTH[4], 1.8 * k, rx_k=2)
            g = [.6, 1, 1, .85, .6, .35][t]
            rr = R(77)
            for j in range(16):
                a = math.radians(rr.uniform(-170, -10))
                L_ = rr.uniform(120, 290) * (1.15 if abs(a + math.pi / 2) < .7 else .8)
                spike(a_, cx + rr.uniform(-30, 30), gy - 8, a, L_ * g, rr.uniform(8, 16) * (1.2 - t * .2), EARTH, 1.6 * k)
        if 0 <= t <= 12:
            p = t / 12
            rx = 60 + 220 * ease_out(p, 2.4)
            ring(a_, cx, gy, rx, rx * GROUND_K, 18 * (1 - p) + 2, EARTH, 1.4 * (1 - p) ** 1.2, soft=7, edge_bias=1)
            sl, X, Y = a_.box(0, 0, W, H)
            q, d = ellipse_q(X, Y, cx, gy, 150 + t * 10, (150 + t * 10) * GROUND_K)
            a_.emit(sl, EARTH[1], np.exp(-q ** 2 * 2.2) * .45 * (1 - p))
        # small rings from the satellite boulders
        for b in boulders[1:]:
            tt = f - b['land']
            if 0 <= tt <= 6:
                p = tt / 6
                rx = b['R'] * (1 + 3 * ease_out(p))
                ring(a_, b['x'], gy, rx, rx * GROUND_K, 6 * (1 - p) + 1, EARTH, 1.1 * (1 - p), soft=3, edge_bias=1)
                if tt <= 1:
                    glow(a_, b['x'], gy - 6, b['R'] * 1.1, EARTH[2], .9 * (1 - tt * .6), rx_k=1.6)
        for s in sparks:
            age = f - s['born']
            if 0 <= age <= 10:
                streak(a_, s['x'], s['y'], s['vx'], s['vy'], math.hypot(s['vx'], s['vy']) * 1.4, 2.0, EARTH, 2.0 * (1 - age / 10))
                s['x'] += s['vx']; s['y'] += s['vy']; s['vy'] += 1.6; s['vx'] *= .93
        for l in leaves:
            flip = math.cos(l['ph'] + f)
            sprite_over(t_, l['spr'], l['x'], l['y'], 1 if f < 20 else .5, rot=l['ph'] + f * .4,
                        sy=max(.25, abs(flip)) * (1 if flip > 0 else -1))
            l['x'] += l['vx'] + math.sin(f + l['ph']) * 2; l['y'] += l['vy']; l['vy'] = min(l['vy'] + 1.3, 2); l['vx'] *= .9
        for p_ in sorted(puffs, key=lambda q_: q_['born']):
            age = f - p_['born']
            if age < 0 or age > p_['life']:
                continue
            p = age / p_['life']
            e = ease_out(p, 2.4)
            x = p_['x'] + p_['vx'] * 12 * e
            y = p_['y'] + p_['vy'] * 12 * e - 26 * p
            puff(t_, x, y, p_['r'] * (.6 + 1.4 * ease_out(p, 1.8)), 1, erode=max(0, (p - .5) * 1.8), seed=p_['seed'] % 40, ox=f * 3)
        a_.bake_glow(.6, 5, .4, 18)
        ad.append(a_); tr.append(t_)
    return ad, tr


# -----------------------------------------------------------------------------
# registry: name -> (builder, fps, pivot, screen scale @1080p, loop, note)
#   builder returns list[Frame] (blend inferred) or (add_frames, trans_frames)
# -----------------------------------------------------------------------------
EFFECTS = {
    "Stomp_Telegraph":     (stomp_telegraph, 24, (256, 124), 1.6, True, "앞발 짓누르기 예비 1턴: 파티 진영 바닥에 루프"),
    "Stomp_Impact_Glow":   (stomp_impact_glow, 30, (256, 256), 1.9, False, "앞발 착지 프레임에 재생. 섬광·스파이크·충격파"),
    "Stomp_Impact_Dust":   (stomp_impact_dust, 30, (256, 236), 1.9, False, "Glow와 같은 프레임에 재생. 흙먼지·파편·잎·균열"),
    "RootLash_Swing":      (root_lash_swing, 30, SW_C, 2.0, False, "피벗=보스 발밑 앞(프레임 상단 중앙). 오른쪽→파티 앞→왼쪽으로 원근 호를 그리며 휩쓸기"),
    "RootLash_Leaves":     (root_lash_leaves, 30, SW_C, 2.0, False, "Swing과 같은 위치·타이밍 (반투명 잎/꽃)"),
    "RootLash_Hit":        (root_lash_hit, 30, (128, 128), 1.4, False, "패링 실패 시 피격 캐릭터 가슴 높이"),
    "ParryCue":            (parry_cue, 30, (128, 128), 1.0, False, "패링 입력창 시작 순간 뿌리 끝에서 1회"),
    "KnotGlow_Charge":     (knot_charge, 24, (128, 150), 1.0, True, "보스 앞쪽 옹이 소켓에 부착, 루프"),
    "KnotGlow_Push":       (knot_push, 30, KP_ORIGIN, 2.0, False, "피벗=옹이. 카메라(파티) 쪽으로 다가오며 커지는 압력파"),
    "HillCollapse_Telegraph1": (hill_tel1, 24, (256, 150), 1.9, True, "예비 1턴차: 균열·진동 (Glow+Dust 동시 재생)"),
    "HillCollapse_Telegraph2": (hill_tel2, 24, (256, 226), 1.7, True, "예비 2턴차: 위험 범위·빛기둥·낙석"),
    "HillCollapse_Impact": (hill_impact, 24, (256, 300), 2.3, False, "본 타격. 메인 바위 7프레임째 착지"),
}
