# Lantern deer: a slender fawn deer whose antlers branch like bare twigs and carry the sleepers'
# memories as small warm lanterns. It walks the waterways, waits for whoever follows, and drinks.
# Legs are posed by planar IK, so the hooves stay planted while the body moves.
import math

import bpy
from mathutils import Euler, Vector
from mathutils.bvhtree import BVHTree

from gse import geo, mat, pipeline, rig

FX = 0.11
BX = 0.12
TIP_L = (0.245, -0.81, 2.38)
TIP_R = (-0.235, -0.80, 2.36)

BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("hips", (0, 0.10, 1.06), (0, 0.46, 1.08), "root"),
	("spine", (0, 0.10, 1.10), (0, -0.20, 1.12), "hips"),
	("chest", (0, -0.20, 1.12), (0, -0.46, 1.14), "spine"),
	("neck1", (0, -0.47, 1.16), (0, -0.63, 1.42), "chest"),
	("neck2", (0, -0.63, 1.42), (0, -0.78, 1.66), "neck1"),
	("head", (0, -0.78, 1.68), (0, -1.12, 1.62), "neck2"),
	("ear_L", (0.07, -0.80, 1.80), (0.23, -0.80, 1.91), "head"),
	("ear_R", (-0.07, -0.80, 1.80), (-0.23, -0.80, 1.91), "head"),
	("antler_tip_L", TIP_L, (TIP_L[0], TIP_L[1], TIP_L[2] + 0.08), "head"),
	("antler_tip_R", TIP_R, (TIP_R[0], TIP_R[1], TIP_R[2] + 0.08), "head"),
	("tail", (0, 0.84, 1.18), (0, 0.92, 1.04), "hips"),
]
for _s, _sx in (("L", 1.0), ("R", -1.0)):
	_f, _b = FX * _sx, BX * _sx
	BONES += [
		("front_upper_" + _s, (_f, -0.40, 1.06), (_f, -0.31, 0.82), "chest"),
		("front_lower_" + _s, (_f, -0.31, 0.82), (_f, -0.38, 0.45), "front_upper_" + _s),
		("front_cannon_" + _s, (_f, -0.38, 0.45), (_f, -0.38, 0.11), "front_lower_" + _s),
		("front_hoof_" + _s, (_f, -0.38, 0.11), (_f, -0.43, 0.0), "front_cannon_" + _s),
		("back_upper_" + _s, (_b, 0.42, 1.08), (_b, 0.31, 0.78), "hips"),
		("back_lower_" + _s, (_b, 0.31, 0.78), (_b, 0.50, 0.47), "back_upper_" + _s),
		("back_cannon_" + _s, (_b, 0.50, 0.47), (_b, 0.46, 0.11), "back_lower_" + _s),
		("back_hoof_" + _s, (_b, 0.46, 0.11), (_b, 0.41, 0.0), "back_cannon_" + _s),
	]
HELPERS = ("root", "antler_tip_L", "antler_tip_R")
DEFORM = [b[0] for b in BONES if b[0] not in HELPERS]
LEGS = {}
for _s in ("L", "R"):
	LEGS["front_" + _s] = ["front_upper_" + _s, "front_lower_" + _s, "front_cannon_" + _s, "front_hoof_" + _s]
	LEGS["back_" + _s] = ["back_upper_" + _s, "back_lower_" + _s, "back_cannon_" + _s, "back_hoof_" + _s]
LEG_BONES = {b for c in LEGS.values() for b in c}

HEAD = {b[0]: Vector(b[1]) for b in BONES}
TAIL = {b[0]: Vector(b[2]) for b in BONES}
PARENT = {b[0]: b[3] for b in BONES}
CONTACT = {k: TAIL[c[-1]].copy() for k, c in LEGS.items()}

WALK_FRAMES = 28
WALK_SPEED = 1.3
SKIN = []


# ---- posing: forward kinematics as rig.key applies it, and a planar two-bone IK per leg -------

def quat(r):
	return Euler([math.radians(a) for a in r], 'XYZ').to_quaternion()


def fk(pose, root, upto=None):
	out = {}
	for (name, _, _, parent) in BONES:
		q = quat(pose.get(name, (0, 0, 0)))
		if parent is None:
			out[name] = (q, HEAD[name] + Vector(root))
		else:
			pd, ph = out[parent]
			out[name] = (pd @ q, ph + pd @ (HEAD[name] - HEAD[parent]))
		if name == upto:
			break
	return out


def ang2(a, b):
	return math.degrees(math.atan2(a[0] * b[1] - a[1] * b[0], a[0] * b[0] + a[1] * b[1]))


def rot2(v, deg):
	c, s = math.cos(math.radians(deg)), math.sin(math.radians(deg))
	return (v[0] * c - v[1] * s, v[0] * s + v[1] * c)


def leg(pose, root, chain, target, ends, splay=0.0):
	# Upper two bones solve to reach the target; the rest keep `ends` (degrees from rest, ground frame).
	parent = PARENT[chain[0]]
	pd, ph = fk(pose, root, parent)[parent]
	frame = pd @ quat((0, splay, 0))
	inv = frame.inverted()
	s = ph + pd @ (HEAD[chain[0]] - HEAD[parent])
	down = inv @ Vector((0, 0, -1))
	tilt = -ang2((0.0, -1.0), (down.y, down.z))
	t = inv @ (Vector(target) - s)
	oy = oz = 0.0
	ends_local = []
	for b, w in zip(chain[2:], ends):
		r = TAIL[b] - HEAD[b]
		a = w - tilt
		v = rot2((r.y, r.z), a)
		oy += v[0]
		oz += v[1]
		ends_local.append(a)
	jy, jz = t.y - oy, t.z - oz
	r1 = TAIL[chain[0]] - HEAD[chain[0]]
	r2 = TAIL[chain[1]] - HEAD[chain[1]]
	u1, u2 = (r1.y, r1.z), (r2.y, r2.z)
	l1, l2 = math.hypot(*u1), math.hypot(*u2)
	rj = (u1[0] + u2[0], u1[1] + u2[1])
	bend = 1.0 if rj[0] * u1[1] - rj[1] * u1[0] > 0.0 else -1.0
	raw = math.hypot(jy, jz)
	d = max(abs(l1 - l2) + 1e-4, min(raw, (l1 + l2) * 0.9995))
	phi = math.atan2(jz, jy)
	cb = (l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d)
	a = phi + bend * math.acos(max(-1.0, min(1.0, cb)))
	j1 = (l1 * math.cos(a), l1 * math.sin(a))
	j2 = (math.cos(phi) * d, math.sin(phi) * d)
	a1 = ang2(u1, j1)
	a2 = ang2(u2, (j2[0] - j1[0], j2[1] - j1[1]))
	pose[chain[0]] = (a1, splay, 0.0)
	pose[chain[1]] = (a2 - a1, 0.0, 0.0)
	prev = a2
	for b, a in zip(chain[2:], ends_local):
		pose[b] = (a - prev, 0.0, 0.0)
		prev = a
	return max(0.0, raw - (l1 + l2))


def end_offset(chain, ends):
	oy = oz = 0.0
	for b, w in zip(chain[2:], ends):
		r = TAIL[b] - HEAD[b]
		v = rot2((r.y, r.z), w)
		oy += v[0]
		oz += v[1]
	return oy, oz


def swing_target(name, y_lift, ends_lift, y_land, ends_land, ends, u, arc):
	# A swinging leg is led by its knee or hock, which arcs from where it rose to where it lands;
	# the folded lower leg hangs from it, so the hoof target never drags the leg out of reach.
	chain = LEGS[name]
	a = end_offset(chain, ends_lift)
	b = end_offset(chain, ends_land)
	e = 0.5 - 0.5 * math.cos(math.pi * u)
	jy = (y_lift - a[0]) + ((y_land - b[0]) - (y_lift - a[0])) * e
	jz = -a[1] + (a[1] - b[1]) * e + arc * math.sin(math.pi * u)
	o = end_offset(chain, ends)
	return (CONTACT[name].x, jy + o[0], jz + o[1])


def default_foot(name):
	return (CONTACT[name], (0.0,) * (len(LEGS[name]) - 2), 0.0)


def lowest(pose, root):
	f = fk(pose, root)
	return min((f[b][1] + f[b][0] @ (p - HEAD[b])).z for b, p in SKIN)


def solve(pose, root, feet=None, settle=True, ground=None):
	# Plants every leg not posed by hand; if a planted leg cannot reach, the body sinks until it can.
	feet = feet or {}
	fixed = {k for k, c in LEGS.items() if c[0] in pose}
	root = Vector(root)
	out = dict(pose)

	def plant(sink):
		for _ in range(6):
			worst = 0.0
			for name, chain in LEGS.items():
				if name in fixed:
					continue
				tgt, ends, splay = feet.get(name, default_foot(name))
				worst = max(worst, leg(out, root, chain, tgt, ends, splay))
			if worst < 0.002 or not sink:
				break
			root.z -= worst * 1.1
	plant(settle)
	if ground and SKIN:
		for _ in range(4):
			z = lowest(out, root)
			lift = -z if ground == "snap" else max(0.0, -z - 0.005)
			if abs(lift) < 0.003:
				break
			root.z += lift
			plant(False)
	return out, root


def sample(fn, frames, ground=None, smooth=0):
	# A looping clip from a pose function. smooth eases the body's height into a smooth curve that
	# never rises above what the planted legs can reach.
	given = [fn(f) for f in range(frames)]
	solved = [solve(p, r, ft, ground=ground) for (p, r, ft) in given]
	if smooth:
		base = [rt.z for _, rt in solved]
		z = list(base)
		for _ in range(smooth):
			z = [min(base[i], (z[i - 1] + z[i] + z[(i + 1) % frames]) / 3.0) for i in range(frames)]
		solved = [solve(p, (rt.x, rt.y, z[i]), ft, settle=False, ground=ground)
				  for i, ((p, r, ft), (_, rt)) in enumerate(zip(given, solved))]
	out = [(f, pose, tuple(rt)) for f, (pose, rt) in enumerate(solved)]
	out.append((frames, solved[0][0], tuple(solved[0][1])))
	return out


def skin_points(obj, step=6):
	# Every few vertices of the bound mesh with its strongest bone.
	names = {g.index: g.name for g in obj.vertex_groups if g.name in HEAD}
	SKIN.clear()
	verts = obj.data.vertices
	for i in range(0, len(verts), step):
		v = verts[i]
		best = max(((ge.weight, names[ge.group]) for ge in v.groups if ge.group in names), default=None)
		if best is not None:
			SKIN.append((best[1], obj.matrix_world @ v.co))


# ---- modelling ------------------------------------------------------------------------------------

def body_bone(y):
	if y < -0.20:
		return "chest"
	if y < 0.25:
		return "spine"
	return "hips"


def tag_body(obj):
	for b in ("chest", "spine", "hips"):
		geo.tag_fn(obj, b, lambda co, n, b=b: 1.0 if body_bone(co.y) == b else 0.0)
	return obj


def surface(objs):
	bpy.context.view_layer.update()
	verts, polys = [], []
	for o in objs:
		mw = o.matrix_world
		base = len(verts)
		verts += [mw @ v.co for v in o.data.vertices]
		polys += [[base + i for i in p.vertices] for p in o.data.polygons]
	return BVHTree.FromPolygons(verts, polys)


def top(tree, x, y):
	hit = tree.ray_cast(Vector((x, y, 5.0)), Vector((0, 0, -1)))
	return hit[0].z if hit[0] is not None else 1.0


def antler(sx, lanterns, wood, glow, seed):
	# Bare-twig antler: a main beam with three tines and two twiglets; lanterns hang from chosen tines.
	rnd = geo.rng(seed)

	def j(p, k=0.012):
		return (p[0] * sx + rnd.uniform(-k, k), p[1] + rnd.uniform(-k, k), p[2] + rnd.uniform(-k, k))
	tip = TIP_L if sx > 0 else TIP_R
	main = [(0.055 * sx, -0.84, 1.78), j((0.09, -0.83, 1.90), 0.0), j((0.15, -0.80, 2.02), 0.0), j((0.20, -0.78, 2.14), 0.0),
			j((0.23, -0.79, 2.26), 0.0), tip]
	branches = [("brow", [main[1], j((0.12, -0.92, 1.95)), j((0.17, -0.99, 1.98)), j((0.21, -1.02, 1.96))], [0.020, 0.016, 0.012, 0.008]),
				("mid", [main[2], j((0.24, -0.83, 2.08)), j((0.32, -0.85, 2.10)), j((0.38, -0.86, 2.08))], [0.020, 0.016, 0.012, 0.008]),
				("rear", [main[3], j((0.25, -0.70, 2.21)), j((0.29, -0.62, 2.24)), j((0.31, -0.57, 2.21))], [0.018, 0.014, 0.011, 0.007]),
				("twig", [main[4], j((0.29, -0.86, 2.31)), j((0.34, -0.91, 2.33)), j((0.38, -0.94, 2.31))], [0.014, 0.011, 0.008, 0.006])]
	out = [geo.tube(main, [0.034, 0.030, 0.026, 0.021, 0.016, 0.009], mat=wood, segments=6, name="antler")]
	for name, pts, radii in branches:
		out.append(geo.tube(pts, radii, mat=wood, segments=5, name="tine"))
		if name == "mid":
			out.append(geo.tube([pts[2], j((0.35, -0.82, 2.17)), j((0.36, -0.80, 2.21))], [0.010, 0.007, 0.005], mat=wood, segments=5, name="twig"))
	lamps = []
	for name, pts, radii in branches:
		if name in lanterns:
			tx, ty, tz = pts[-1]
			out.append(geo.tube([(tx, ty, tz + 0.005), (tx, ty, tz - 0.05)], [0.006, 0.006], mat=wood, segments=4, name="stalk"))
			lamp = geo.sphere((0.046, 0.046, 0.054), (tx, ty, tz - 0.095), mat=glow, segments=10, rings=7, name="lantern")
			geo.tag(lamp, "pulse")
			lamps.append(lamp)
	return out, lamps


def build():
	coat = mat.get("fur_deer")
	pale = mat.make("deer_pale", (0.66, 0.60, 0.50), pattern="fur", rough=0.95, bump=0.4)
	white = mat.make("deer_white", (0.82, 0.80, 0.74), pattern="fur", rough=0.95, bump=0.4)
	legs = mat.make("deer_leg", (0.31, 0.26, 0.21), pattern="fur", rough=0.95, bump=0.4)
	hoof = mat.make("deer_hoof", (0.12, 0.10, 0.09), pattern="plain", rough=0.6, bump=0.1)
	nose = mat.make("deer_nose", (0.10, 0.09, 0.08), pattern="plain", rough=0.4, bump=0.05)
	eye = mat.make("deer_eye", (0.05, 0.04, 0.04), pattern="plain", rough=0.25, grime=0.0, edge=0.0, bump=0.0)
	wood = mat.get("antler")
	glow = mat.get("lamp_glow")
	moss = mat.get("moss")

	soft = []
	hard = []

	def part(obj, bone, into=soft):
		geo.tag(obj, bone)
		into.append(obj)
		return obj

	# Body: a slender swept hull, deepest at the chest, the back almost level.
	# The rump rounds off on an ellipse from y 0.56 (top 1.31, bottom 0.90, half width 0.22) to y 0.88.
	stations = []
	for a in (84, 72, 58, 42, 22):
		c, e = math.cos(math.radians(a)), math.sin(math.radians(a))
		stations.append((0.56 + 0.32 * e, 1.105 + 0.205 * c + 0.03 * e, 1.105 - 0.205 * c + 0.03 * e, 0.22 * c))
	stations += [(0.45, 1.32, 0.93, 0.22), (0.25, 1.28, 0.93, 0.20), (0.05, 1.27, 0.86, 0.21), (-0.20, 1.31, 0.79, 0.22),
				 (-0.40, 1.37, 0.76, 0.20), (-0.55, 1.34, 0.80, 0.17), (-0.64, 1.28, 0.90, 0.13), (-0.68, 1.18, 0.98, 0.08)]
	seg = 16
	hull = geo.tube([(0, y, (t + b) * 0.5) for (y, t, b, w) in stations], [(t - b) * 0.5 for (y, t, b, w) in stations],
					mat=coat, segments=seg, name="hull")
	for i, (y, t, b, w) in enumerate(stations):
		for k in range(seg):
			hull.data.vertices[i * seg + k].co.x *= w / ((t - b) * 0.5)
	soft.append(tag_body(hull))
	soft.append(tag_body(geo.blob((0.14, 0.40, 0.06), (0, -0.02, 0.84), (-4, 0, 0), mat=pale, name="belly")))
	part(geo.blob((0.11, 0.08, 0.15), (0, -0.56, 1.04), mat=pale, name="breast"), "chest")
	part(geo.blob((0.065, 0.04, 0.10), (0, 0.85, 1.05), mat=white, name="rump_patch"), "hips")

	# Neck rising forward to a small fine head with a dark nose and big ears.
	neck = geo.tube([(0, -0.50, 1.12), (0, -0.62, 1.34), (0, -0.71, 1.50), (0, -0.79, 1.64)], [0.155, 0.12, 0.10, 0.088], mat=coat, segments=12, name="neck")
	neck.scale = (0.85, 1.0, 1.0)
	geo.tag_fn(neck, "neck1", lambda co, n: 1.0 if co.z < 1.40 else 0.0)
	geo.tag_fn(neck, "neck2", lambda co, n: 1.0 if co.z >= 1.40 else 0.0)
	soft.append(neck)
	throat = geo.blob((0.055, 0.05, 0.13), (0, -0.66, 1.36), (32, 0, 0), mat=pale, name="throat")
	geo.tag_fn(throat, "neck1", lambda co, n: 1.0 if co.z < 1.40 else 0.0)
	geo.tag_fn(throat, "neck2", lambda co, n: 1.0 if co.z >= 1.40 else 0.0)
	soft.append(throat)
	part(geo.blob((0.10, 0.125, 0.10), (0, -0.85, 1.72), mat=coat, name="skull"), "head")
	part(geo.tube([(0, -0.90, 1.70), (0, -1.02, 1.655), (0, -1.12, 1.625)], [0.08, 0.06, 0.045], mat=coat, segments=12, name="muzzle"), "head")
	part(geo.blob((0.042, 0.06, 0.03), (0, -1.06, 1.585), mat=pale, name="chin"), "head")
	part(geo.blob((0.04, 0.03, 0.035), (0, -1.15, 1.625), mat=nose, name="nose"), "head")
	for s, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.blob((0.05, 0.02, 0.10), (0.152 * sx, -0.80, 1.857), (0, 55 * sx, 0), mat=coat, name="ear"), "ear_" + s)
		part(geo.sphere(0.022, (0.078 * sx, -0.92, 1.745), mat=eye, segments=8, rings=6, name="eye"), "head", hard)

	# White tail hanging over the rump.
	part(geo.blob((0.055, 0.035, 0.11), (0, 0.885, 1.11), (-18, 0, 0), mat=coat, name="tail"), "tail")
	part(geo.blob((0.05, 0.03, 0.10), (0, 0.905, 1.095), (-18, 0, 0), mat=white, name="tail_under"), "tail")

	# Long fine legs: dark below the knee, small black hooves.
	for s, sx in (("L", 1.0), ("R", -1.0)):
		x, xb = FX * sx, BX * sx
		part(geo.tube([(x, -0.40, 1.00), (x, -0.31, 0.82)], [0.085, 0.058], mat=coat, segments=10, name="fu"), "front_upper_" + s)
		part(geo.tube([(x, -0.31, 0.82), (x, -0.38, 0.45)], [0.056, 0.038], mat=coat, segments=10, name="fl"), "front_lower_" + s)
		part(geo.blob(0.041, (x, -0.38, 0.45), mat=legs, name="knee"), "front_cannon_" + s)
		part(geo.tube([(x, -0.38, 0.45), (x, -0.38, 0.12)], [0.032, 0.028], mat=legs, segments=8, name="fc"), "front_cannon_" + s)
		part(geo.tube([(x, -0.38, 0.12), (x, -0.415, 0.045)], [0.03, 0.03], mat=legs, segments=8, name="fp"), "front_hoof_" + s)
		part(geo.blob((0.033, 0.05, 0.036), (x, -0.425, 0.036), mat=hoof, name="hoof"), "front_hoof_" + s)
		part(geo.tube([(xb, 0.43, 1.02), (xb, 0.31, 0.78)], [0.10, 0.065], mat=coat, segments=10, name="bu"), "back_upper_" + s)
		part(geo.tube([(xb, 0.31, 0.78), (xb, 0.50, 0.47)], [0.06, 0.038], mat=coat, segments=10, name="bl"), "back_lower_" + s)
		part(geo.blob(0.042, (xb, 0.50, 0.47), mat=legs, name="hock"), "back_cannon_" + s)
		part(geo.tube([(xb, 0.50, 0.47), (xb, 0.46, 0.12)], [0.032, 0.028], mat=legs, segments=8, name="bc"), "back_cannon_" + s)
		part(geo.tube([(xb, 0.46, 0.12), (xb, 0.425, 0.045)], [0.03, 0.03], mat=legs, segments=8, name="bp"), "back_hoof_" + s)
		part(geo.blob((0.033, 0.05, 0.036), (xb, 0.415, 0.036), mat=hoof, name="hoof"), "back_hoof_" + s)

	# A strip of moss along the spine.
	back = surface([hull])
	rnd = geo.rng(9)
	for k, (y, rx, ry) in enumerate(((-0.46, 0.05, 0.07), (-0.34, 0.07, 0.10), (-0.18, 0.06, 0.09), (-0.04, 0.08, 0.11), (0.14, 0.05, 0.07),
									 (0.26, 0.07, 0.10), (0.42, 0.06, 0.08), (0.56, 0.045, 0.06))):
		x = rnd.uniform(-0.035, 0.035)
		m = geo.blob((rx, ry, 0.04), (x, y, top(back, x, y) + 0.004), (0, 0, rnd.uniform(-30, 30)), mat=moss, lumpy=0.35, seed=20 + k, name="moss")
		geo.tag(m, "pulse")
		soft.append(tag_body(m))

	# Antlers: the left carries three lanterns, the right two; the brow tines stay bare beside the face.
	for sx, lit, seed in ((1.0, ("twig", "mid", "rear"), 3), (-1.0, ("mid", "rear"), 4)):
		twigs, lamps = antler(sx, lit, wood, glow, seed)
		for o in twigs + lamps:
			part(o, "head", hard)

	res = pipeline.mixed("deer", hard, soft, voxel=0.011, tris=4200, size=512)
	low = res["mesh"]

	arm = rig.armature("deer_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)
	skin_points(low)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("idle", 22), ("idle", 60), ("walk", 0),
							 ("walk", 7), ("walk", 14), ("walk", 21), ("wait", 0),
							 ("wait", 45), ("drink", 0), ("drink", 30), ("drink", 60)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 3.2
	return res


# ---- animation ------------------------------------------------------------------------------------

def bump(t, at, width):
	d = (t - at) / width
	return math.exp(-d * d)


def wrap_bump(t, at, width):
	return max(bump(t, at, width), bump(t, at - 1.0, width), bump(t, at + 1.0, width))


def idle_pose(f, frames=90):
	t = f / float(frames)
	br = math.sin(2.0 * math.pi * 3.0 * t)
	look = math.sin(2.0 * math.pi * t)
	pose = {"spine": (-0.8 * br, 0, 0), "chest": (0.6 * br, 0, 0),
			"neck1": (1.0 * br, 0, 4 * look), "neck2": (0, 0, 5 * look), "head": (3 * bump(t, 0.5, 0.15), 3 * look, 6 * look),
			"ear_L": (-25 * wrap_bump(t, 0.25, 0.02), 0, 12 * wrap_bump(t, 0.6, 0.04)),
			"ear_R": (-25 * wrap_bump(t, 0.42, 0.02), 0, -10 * wrap_bump(t, 0.85, 0.04)),
			"tail": (-35 * wrap_bump(t, 0.72, 0.025), 0, 0)}
	return pose, (0, 0, 0.004 * br), {}


GAIT = {"back_L": 0.0, "front_L": 0.25, "back_R": 0.5, "front_R": 0.75}
DUTY = 0.6


def walk_pose(f):
	p = f / float(WALK_FRAMES)
	w = 2.0 * math.pi * p
	stride = WALK_SPEED * WALK_FRAMES / 30.0
	pose = {"hips": (0, 1.5 * math.sin(w), 0), "spine": (0, -0.8 * math.sin(w), 0),
			"neck1": (2.0 * math.sin(2 * w + 0.6), 0, 0), "neck2": (-1.0 * math.sin(2 * w + 0.6), 0, 0),
			"head": (-1.5 * math.sin(2 * w + 0.6), 0, 2.0 * math.sin(w)),
			"ear_L": (-8, 0, 3 * math.sin(w)), "ear_R": (-8, 0, 3 * math.sin(w)),
			"tail": (-6 + 6 * math.sin(2 * w), 0, 6 * math.sin(w))}
	root = (0, 0, 0.012 * math.cos(2 * w))
	feet = {}
	s = stride * DUTY
	for name, touch in GAIT.items():
		q = (p - touch) % 1.0
		c = CONTACT[name]
		j0 = HEAD[LEGS[name][0]]
		front = name.startswith("front")
		if q < DUTY:
			k = q / DUTY
			y = c.y - s * 0.5 + s * k
			z = 0.0
			lean = math.degrees(math.atan2(y - j0.y, j0.z))
			ends = (0.85 * lean, 25.0 * max(0.0, k - 0.75) / 0.25)
		else:
			u = (q - DUTY) / (1.0 - DUTY)
			y_lift, y_land = c.y + s * 0.5, c.y - s * 0.5
			lean_lift = math.degrees(math.atan2(y_lift - j0.y, j0.z))
			lean_land = math.degrees(math.atan2(y_land - j0.y, j0.z))
			lean = lean_lift + (lean_land - lean_lift) * (0.5 - 0.5 * math.cos(math.pi * u))
			lift = math.sin(math.pi * min(1.0, u / 0.85))
			carry = (1.0 - u) ** 3
			if front:
				ends = (0.85 * lean + 75.0 * lift, 25.0 * carry + 35.0 * lift)
			else:
				ends = (0.85 * lean + 20.0 * lift - 12.0 * math.sin(math.pi * max(0.0, u - 0.7) / 0.3), 25.0 * carry + 30.0 * lift)
			tgt = swing_target(name, y_lift, (0.85 * lean_lift, 25.0), y_land, (0.85 * lean_land, 0.0), ends, u, 0.06 if front else 0.05)
			feet[name] = (tgt, ends, 0.0)
			continue
		feet[name] = ((c.x, y, z), ends, 0.0)
	return pose, root, feet


def wait_pose(f, frames=90):
	t = f / float(frames)
	br = math.sin(2.0 * math.pi * 3.0 * t)
	sway = math.sin(2.0 * math.pi * t)
	pose = {"spine": (-0.8 * br, 0, 4), "chest": (0.6 * br, 0, 8),
			"neck1": (-10, 0, 28 + 2 * sway), "neck2": (-4, 0, 36 + 3 * sway), "head": (2, 0, 52 + 4 * sway),
			"ear_L": (12 - 25 * wrap_bump(t, 0.3, 0.02), 0, 10), "ear_R": (12, 0, -10 - 20 * wrap_bump(t, 0.65, 0.025)),
			"tail": (-30 * wrap_bump(t, 0.85, 0.025), 0, 8)}
	return pose, (0, 0, 0.004 * br), {}


NOSE = Vector((0, -1.16, 1.62))
DRINK = {"hips": (14, 0, 0), "spine": (1, 0, 0), "chest": (1, 0, 0), "neck1": (68, 0, 0), "neck2": (44, 0, 0), "head": (-40, 0, 0)}


def drink_drop():
	# How far the body sinks so the nose meets the water.
	f = fk(DRINK, (0, 0, 0))
	d, h = f["head"]
	return (h + d @ (NOSE - HEAD["head"])).z - 0.03


def drink_pose(f, frames=90):
	t = f / float(frames)
	sip = sum(bump(t, a, 0.035) for a in (0.2, 0.45, 0.7))
	br = math.sin(2.0 * math.pi * 2.0 * t)
	pose = dict(DRINK)
	pose.update({"chest": (1 + 0.5 * br, 0, 0), "neck2": (44 + 2.5 * sip, 0, 0), "head": (-40 - 4 * sip, 0, 0),
				 "ear_L": (-10, 0, 18 * wrap_bump(t, 0.35, 0.03)), "ear_R": (-10, 0, -15 * wrap_bump(t, 0.85, 0.03)),
				 "tail": (-25 * wrap_bump(t, 0.58, 0.025), 0, 10 * math.sin(2 * math.pi * 2 * t))})
	feet = {"front_L": ((FX + 0.14, -0.50, 0.0), (0.0, 0.0), -20.0),
			"front_R": ((-FX - 0.14, -0.40, 0.0), (0.0, 0.0), 20.0)}
	return pose, (0, 0, -drink_drop()), feet


def animations(arm):
	clips = []
	clips.append(rig.clip(arm, "idle", sample(idle_pose, 90), loop=True))
	clips.append(rig.clip(arm, "walk", sample(walk_pose, WALK_FRAMES, smooth=12), loop=True))
	clips.append(rig.clip(arm, "wait", sample(wait_pose, 90), loop=True))
	clips.append(rig.clip(arm, "drink", sample(drink_pose, 90, ground="floor"), loop=True))
	return clips
