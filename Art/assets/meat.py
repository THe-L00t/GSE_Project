# A pickup: a raw boar haunch on the bone, dark red muscle under a pale fat cap with fat streaks,
# the white bone end with its knuckle sticking out. cooked_meat reuses the same haunch.
import math

from mathutils import Vector

from gse import geo, mat, pipeline

TILT = 10.0
HEIGHT = 0.34


def place(obj):
	# Lay the haunch along X, bone end to -X and slightly raised, hovering at pickup height.
	geo.apply_transform(obj)
	obj.rotation_euler = (0.0, math.radians(TILT), 0.0)
	geo.apply_transform(obj)
	for v in obj.data.vertices:
		v.co.z += HEIGHT
	obj.data.update()
	return obj


PROFILE = [(0.04, 0.0), (0.058, 0.04), (0.09, 0.1), (0.112, 0.165), (0.12, 0.225), (0.112, 0.28), (0.1, 0.315), (0.0, 0.322)]
SQUASH = 0.86


def radius_at(z):
	for (r0, z0), (r1, z1) in zip(PROFILE, PROFILE[1:]):
		if z0 <= z <= z1:
			return r0 + (r1 - r0) * (z - z0) / max(z1 - z0, 1e-4)
	return PROFILE[-1][0]


def surface(x, angle, inset=0.003):
	# A point on the haunch's skin at x along it, angle round it.
	r = radius_at(x + 0.13) - inset
	return (x, r * SQUASH * math.cos(angle), r * math.sin(angle))


def haunch(m, cap=True, extra=None):
	# m: dict of materials meat, fat, streak, bone, hide. Built along the X axis at height 0, then placed.
	parts = []
	body = geo.lathe(PROFILE, (-0.13, 0.0, 0.0), (0.0, 90.0, 0.0), mat=m["meat"], segments=20, name="body")
	body.scale = (1.0, SQUASH, 1.0)
	parts.append(body)
	# The cut face at the hip: a rim of fat round the meat.
	rim = [(0.18, 0.094 * SQUASH * math.cos(i / 16.0 * math.tau), 0.094 * math.sin(i / 16.0 * math.tau)) for i in range(17)]
	parts.append(geo.tube(rim, 0.011, mat=m["fat"], segments=6, name="fat_rim"))
	parts.append(geo.blob((0.012, 0.026, 0.024), (0.188, 0.008, -0.012), mat=m["bone"], subdivisions=2, name="bone_cut"))
	if cap:
		parts.append(geo.blob((0.12, 0.07, 0.028), (0.07, 0.0, 0.104), (0.0, 6.0, 0.0), mat=m["fat"], lumpy=0.3, seed=3, name="fat_cap"))
	for a in (0.5, 2.2, 3.6, 4.6):
		parts.append(geo.tube([surface(-0.06, a + 0.25), surface(0.04, a), surface(0.15, a - 0.2)], [0.005, 0.008, 0.006],
							  mat=m["streak"], segments=5, name="streak"))
	# Bristly hide left on the shank end.
	hide = geo.tube([(-0.14, 0.0, 0.0), (-0.1, 0.0, 0.0), (-0.07, 0.0, 0.0)], [0.046, 0.058, 0.07], mat=m["hide"], segments=14, name="hide")
	hide.scale = (1.0, SQUASH, 1.0)
	parts.append(hide)
	parts.append(geo.cylinder(0.026, 0.16, (-0.18, 0.0, 0.0), (0.0, 90.0, 0.0), mat=m["bone"], segments=10, name="shank"))
	parts.append(geo.blob((0.032, 0.03, 0.03), (-0.268, 0.022, 0.006), mat=m["bone"], subdivisions=2, name="knuckle"))
	parts.append(geo.blob((0.03, 0.028, 0.028), (-0.27, -0.02, -0.004), mat=m["bone"], subdivisions=2, name="knuckle"))
	parts += extra or []
	return [place(p) for p in parts]


def build():
	m = {
		"meat": mat.make("meat_raw", (0.44, 0.13, 0.12), pattern="hair", rough=0.55, bump=0.3, grime=0.3),
		"fat": mat.make("fat_raw", (0.85, 0.77, 0.68), pattern="skin", rough=0.5, bump=0.15, grime=0.2),
		"streak": mat.make("fat_streak", (0.80, 0.68, 0.62), pattern="skin", rough=0.5, bump=0.1, grime=0.2),
		"bone": mat.get("bone"),
		"hide": mat.get("fur_boar"),
	}
	parts = haunch(m)
	for p in parts:
		geo.tag(p, "spin", "hover")
	return pipeline.organic("meat", parts, voxel=0.007, tris=1300, size=256)
