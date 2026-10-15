"""Pure data: v004 waypoints in UE cm, Z denotes the expected floor contact.

This file is safe to inspect without an Unreal session. It does not execute QA.
"""

def affine(point):
    x, y, z = point
    return (140.4 + .74*x, .67*y, z)


def design(point):
    return affine(tuple(45*v for v in point))


def gorm_local(point):
    x, y, z = point
    return (-692.1 + .9*(x+1125), -1085.4 + .9*(y+1620), .9*z)


def stair(x, points, shift_x=0):
    return [tuple(v + (shift_x if i == 0 else 0) for i, v in enumerate(design((x, y, z))))
            for y, z in points]


BASE_ROUTES = {
    'rail': [(15.5,-78,0),(15.5,-42,0),(15.5,-18,3.5),(15.5,18,3.5),(15.5,42,7),(15.5,83,7)],
    'cemetery': [(65,48,7),(78,55,10.2),(95,65,14),(103,65,14)],
    'ashslope': [(-72,12,3.5),(-96,12,3.5),(-102,30,3.5),(-117,30,6),(-126,-8,6),(-142,-8,8.5),(-151,30,8.5),(-167,30,11),(-176,-8,11),(-192,-8,13.5)],
    'deepvein': [(12,92,7),(12,103,7),(12,127,3),(12,151,3),(12,178,-1),(12,199,-1)],
    'pinehollow': [(12,-102,0),(12,-125,0),(12,-148,0),(12,-170,0),(12,-190,0)],
}

ROUTES = {name: [design(point) for point in points] for name, points in BASE_ROUTES.items()}
# Mixed transforms are deliberate: Gorm's house accessories keep .9 local offsets,
# while the adjacent lower west stair moves affine +60 cm X.
ROUTES['gorm'] = [
    (-399,-1306.5,0), gorm_local((-1030,-1900,0)),
    gorm_local((-1160,-1830,0)), gorm_local((-1160,-1790,0)),
    gorm_local((-1000,-1850,0)), (-399,-1239.5,0),
    (-399,-964.8,121.7), (-399,-814.05,157.5),
]
ROUTES['stairs_lower_west'] = stair(-18, [(-42,0),(-34,1.9),(-31,3.2),(-27,3.5)], 60)
ROUTES['stairs_lower_east'] = stair(44, [(-42,0),(-34,1.9),(-31,3.2),(-27,3.5)])
ROUTES['stairs_upper_west'] = stair(-18, [(18,3.5),(26,5.4),(29,6.7),(33,7)])
ROUTES['stairs_upper_east'] = stair(44, [(18,3.5),(26,5.4),(29,6.7),(33,7)])
ROUTES['smithy_approach'] = [(656.55,510,157.5),(260,510,157.5),(-92.7,510,157.5),(-92.7,600,157.5)]
# The tavern case reaches the door apron; no traversable interior is assumed.
ROUTES['tavern_approach'] = [(656.55,-2190,0),(170,-2190,0),(-126,-2190,0),(-126,-1970,0)]
ROUTES['housing_north_alley'] = [(-1235,-1430,0),(-1235,-1300,0),(-1235,-1190,0),(-1235,-1105,0),(-1235,-980,0)]
ROUTES['housing_middle_alley'] = [(-1153,-1970,0),(-1153,-1850,0),(-1153,-1650,0),(-1153,-1460,0)]

ROUTE_ORDER = list(ROUTES)
TOLERANCE_CM = {name: (26 if name == 'gorm' or 'alley' in name else 40 if 'stairs' in name or 'approach' in name else 58)
                for name in ROUTES}
TIMEOUT_SECONDS = {name: 120 if name in ('ashslope','deepvein','pinehollow') else 70 for name in ROUTES}

EXPECTED_ANCHORS = {
    'entry': (1077.7,195.975,215),
    'lower': (1172.7,253.475,29),
    'hatch': (1172.7,195.975,164.25),
}
