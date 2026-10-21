# Moss boar: a heavy wild boar whose back has grown into a small forest of moss, ferns and mushrooms.
# It sleeps beside the sleepers it guards and charges only when startled.
# Legs are posed by planar IK, so the hooves stay planted while the body moves.
import math

import bmesh
import bpy
from mathutils import Euler, Vector
from mathutils.bvhtree import BVHTree

from gse import geo, mat, pipeline, rig

FX = 0.20

BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("hips", (0, 0.12, 0.74), (0, 0.46, 0.74), "root"),
	("spine", (0, 0.12, 0.78), (0, -0.20, 0.82), "hips"),
	("chest", (0, -0.20, 0.82), (0, -0.48, 0.80), "spine"),
	("neck", (0, -0.48, 0.80), (0, -0.66, 0.80), "chest"),
	("head", (0, -0.66, 0.80), (0, -0.92, 0.62), "neck"),
	("snout", (0, -0.92, 0.61), (0, -1.15, 0.55), "head"),
	("ear_L", (0.11, -0.70, 0.94), (0.20, -0.66, 1.14), "head"),
	("ear_R", (-0.11, -0.70, 0.94), (-0.20, -0.66, 1.14), "head"),
	("tail", (0, 0.90, 0.80), (0, 0.98, 0.60), "hips"),
]
for _s, _x in (("L", FX), ("R", -FX)):
	BONES += [
		("front_upper_" + _s, (_x, -0.36, 0.64), (_x, -0.28, 0.40), "chest"),
		("front_lower_" + _s, (_x, -0.28, 0.40), (_x, -0.34, 0.18), "front_upper_" + _s),
		("front_foot_" + _s, (_x, -0.34, 0.18), (_x, -0.37, 0.0), "front_lower_" + _s),
		("back_upper_" + _s, (_x, 0.52, 0.66), (_x, 0.42, 0.38), "hips"),
		("back_lower_" + _s, (_x, 0.42, 0.38), (_x, 0.54, 0.19), "back_upper_" + _s),
		("back_foot_" + _s, (_x, 0.54, 0.19), (_x, 0.52, 0.0), "back_lower_" + _s),
	]
DEFORM = [b[0] for b in BONES if b[0] != "root"]
LEGS = {}
for _s in ("L", "R"):
	LEGS["front_" + _s] = ["front_upper_" + _s, "front_lower_" + _s, "front_foot_" + _s]
	LEGS["back_" + _s] = ["back_upper_" + _s, "back_lower_" + _s, "back_foot_" + _s]
LEG_BONES = {b for c in LEGS.values() for b in c}

HEAD = {b[0]: Vector(b[1]) for b in BONES}
TAIL = {b[0]: Vector(b[2]) for b in BONES}
PARENT = {b[0]: b[3] for b in BONES}
CONTACT = {k: TAIL[c[-1]].copy() for k, c in LEGS.items()}

RUN_FRAMES = 16
RUN_SPEED = 2.6
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
	# ground "floor" lifts the body out of the ground, "snap" also drops it onto the ground.
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


def lerp(a, b, t):
	return tuple(x + (y - x) * t for x, y in zip(a, b))


def ease(t, kind="smooth"):
	if kind == "lin":
		return t
	if kind == "in":
		return t * t
	if kind == "out":
		return 1.0 - (1.0 - t) * (1.0 - t)
	return t * t * (3.0 - 2.0 * t)


def track(keys, ground=None, snap_from=None):
	# keys: (frame, pose, root, feet[, ease]). Planted legs stay solved between keys; legs posed by
	# hand in either key are blended as rotations.
	def mode(f):
		return "snap" if snap_from is not None and f >= snap_from else ground
	res = [solve(k[1], k[2], k[3], ground=mode(k[0])) for k in keys]
	out = []
	for i in range(len(keys) - 1):
		f0, p0, r0, ft0 = keys[i][:4]
		f1, p1, r1, ft1 = keys[i + 1][:4]
		kind = keys[i + 1][4] if len(keys[i + 1]) > 4 else "smooth"
		for f in range(f0, f1):
			t = ease((f - f0) / float(f1 - f0), kind)
			names = (set(p0) | set(p1)) - LEG_BONES
			body = {n: lerp(p0.get(n, (0, 0, 0)), p1.get(n, (0, 0, 0)), t) for n in names}
			root = lerp(r0, r1, t)
			feet = {}
			for name, chain in LEGS.items():
				if chain[0] in p0 or chain[0] in p1:
					for b in chain:
						body[b] = lerp(res[i][0][b], res[i + 1][0][b], t)
				else:
					a = ft0.get(name, default_foot(name))
					b = ft1.get(name, default_foot(name))
					feet[name] = (lerp(a[0], b[0], t), lerp(a[1], b[1], t), a[2] + (b[2] - a[2]) * t)
			pose, rt = solve(body, root, feet, ground=mode(f))
			out.append((f, pose, tuple(rt)))
	out.append((keys[-1][0], res[-1][0], tuple(res[-1][1])))
	return out


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
	# Every few vertices of the bound mesh with its strongest bone: the body the ground must hold up.
	names = {g.index: g.name for g in obj.vertex_groups if g.name in HEAD}
	sway = obj.vertex_groups.get("sway")
	SKIN.clear()
	verts = obj.data.vertices
	for i in range(0, len(verts), step):
		v = verts[i]
		if sway is not None and any(ge.group == sway.index and ge.weight > 0.5 for ge in v.groups):
			continue
		best = max(((ge.weight, names[ge.group]) for ge in v.groups if ge.group in names), default=None)
		if best is not None:
			SKIN.append((best[1], obj.matrix_world @ v.co))


# ---- modelling ------------------------------------------------------------------------------------

def body_bone(y):
	if y < -0.68:
		return "head"
	if y < -0.50:
		return "neck"
	if y < -0.12:
		return "chest"
	if y < 0.30:
		return "spine"
	return "hips"


def tag_body(obj):
	for b in ("head", "neck", "chest", "spine", "hips"):
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


def frond(base, heading, length, rise, width, material, droop=0.45, n=10):
	# A fern frond: an arching blade whose stations alternate wide and narrow, read as leaflets.
	h = Vector((math.cos(heading), math.sin(heading), 0.0))
	up = Vector((0.0, 0.0, 1.0))
	p0 = Vector(base)
	p1 = p0 + h * (length * 0.3) + up * (rise * 1.3)
	p2 = p0 + h * length + up * (rise * droop)
	bm = bmesh.new()
	rings = []
	for i in range(n + 1):
		t = i / float(n)
		pt = p0 * ((1 - t) ** 2) + p1 * (2 * (1 - t) * t) + p2 * (t * t)
		tan = ((p1 - p0) * (1 - t) + (p2 - p1) * t).normalized()
		side = tan.cross(up)
		if side.length < 1e-4:
			side = Vector((1.0, 0.0, 0.0))
		side.normalize()
		nrm = side.cross(tan).normalized()
		w = width * max(0.12, math.sin(math.pi * min(1.0, 0.1 + t * 0.95)))
		if i % 2 == 1:
			w *= 0.5
		if i == n:
			w = 0.004
		th = 0.012 * (1.0 - 0.5 * t)
		rings.append([bm.verts.new(pt + side * w), bm.verts.new(pt + nrm * th), bm.verts.new(pt - side * w), bm.verts.new(pt - nrm * th)])
	for a, b in zip(rings, rings[1:]):
		for k in range(4):
			j = (k + 1) % 4
			bm.faces.new((a[k], a[j], b[j], b[k]))
	bm.faces.new(list(reversed(rings[0])))
	bm.faces.new(rings[-1])
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	return geo._finish(bm, "frond", (0, 0, 0), (0, 0, 0), material, 0.0, 0, True)


def build():
	fur = mat.get("fur_boar")
	fur_dark = mat.make("boar_fur_dark", (0.21, 0.17, 0.14), pattern="fur", rough=0.95, bump=0.6)
	cheek = mat.make("boar_cheek", (0.40, 0.34, 0.28), pattern="fur", rough=0.95, bump=0.5)
	bristle = mat.make("boar_bristle", (0.15, 0.12, 0.10), pattern="hair", rough=0.9, bump=0.5)
	nose = mat.make("boar_nose", (0.54, 0.43, 0.42), pattern="skin", rough=0.6, bump=0.15, edge=0.1)
	nostril = mat.make("boar_nostril", (0.14, 0.10, 0.10), pattern="plain", rough=0.5, bump=0.0)
	hoof = mat.make("boar_hoof", (0.11, 0.10, 0.09), pattern="plain", rough=0.6, bump=0.1)
	tusk = mat.get("bone")
	eye = mat.glow("boar_eye", (0.92, 0.62, 0.26), 0.6)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	fern = mat.make("boar_fern", (0.26, 0.40, 0.22), pattern="leaf", rough=0.8, bump=0.4)
	cap = mat.make("boar_mushroom_cap", (0.80, 0.74, 0.60), pattern="plain", rough=0.7, bump=0.2, edge=0.15)
	stem = mat.make("boar_mushroom_stem", (0.70, 0.66, 0.56), pattern="plain", rough=0.8, bump=0.1)
	spore = mat.get("spore_glow")

	soft = []
	hard = []

	def part(obj, bone, into=soft):
		geo.tag(obj, bone)
		into.append(obj)
		return obj

	# Body: one swept hull, massive and humped at the shoulders, falling away to a smaller rump.
	# Stations from tail to head: (y, top, bottom, half width).
	stations = [(0.97, 0.79, 0.66, 0.05), (0.93, 0.86, 0.57, 0.14), (0.85, 0.92, 0.51, 0.21), (0.70, 0.96, 0.47, 0.25), (0.45, 0.98, 0.45, 0.28),
				(0.15, 1.02, 0.43, 0.32), (-0.12, 1.10, 0.40, 0.36), (-0.32, 1.14, 0.39, 0.38), (-0.48, 1.10, 0.41, 0.36),
				(-0.62, 1.01, 0.45, 0.30), (-0.74, 0.93, 0.49, 0.24)]
	seg = 18
	hull = geo.tube([(0, y, (t + b) * 0.5) for (y, t, b, w) in stations], [(t - b) * 0.5 for (y, t, b, w) in stations],
					mat=fur, segments=seg, name="hull")
	for i, (y, t, b, w) in enumerate(stations):
		for k in range(seg):
			hull.data.vertices[i * seg + k].co.x *= w / ((t - b) * 0.5)
	soft.append(tag_body(hull))
	mane = geo.tube([(0, -0.72, 0.92), (0, -0.52, 1.08), (0, -0.30, 1.15)], [0.035, 0.05, 0.04], mat=bristle, segments=8, name="mane")
	soft.append(tag_body(mane))
	for sx in (1.0, -1.0):
		part(geo.blob((0.10, 0.18, 0.20), (0.24 * sx, -0.32, 0.62), mat=fur, name="shoulder_pad"), "chest")
		part(geo.blob((0.10, 0.22, 0.22), (0.21 * sx, 0.52, 0.62), mat=fur, name="ham"), "hips")

	# Head: long and wedge-shaped, the snout ending in a pink-grey disc.
	part(geo.blob((0.19, 0.20, 0.19), (0, -0.74, 0.76), mat=fur, lumpy=0.05, seed=6, name="skull"), "head")
	part(geo.blob((0.17, 0.16, 0.12), (0, -0.82, 0.60), mat=cheek, name="jowls"), "head")
	sn = geo.tube([(0, -0.78, 0.70), (0, -0.92, 0.62), (0, -1.05, 0.565), (0, -1.11, 0.55)], [0.17, 0.12, 0.095, 0.09],
				  mat=fur_dark, segments=14, name="snout")
	geo.tag_fn(sn, "head", lambda co, n: 1.0 if co.y >= -0.92 else 0.0)
	geo.tag_fn(sn, "snout", lambda co, n: 1.0 if co.y < -0.92 else 0.0)
	soft.append(sn)
	disc = geo.cylinder(0.09, 0.05, (0, -1.125, 0.55), (90, 0, 0), mat=nose, segments=14, name="nose")
	disc.scale = (1.0, 0.85, 1.0)
	part(disc, "snout")
	for sx in (1.0, -1.0):
		part(geo.blob((0.02, 0.012, 0.028), (0.033 * sx, -1.15, 0.55), mat=nostril, name="nostril"), "snout")
		ear = geo.cone(0.08, 0.24, (0.11 * sx, -0.70, 0.94 + 0.12), (-10, 25 * sx, 0), mat=fur_dark, segments=10, name="ear")
		ear.scale = (1.0, 0.4, 1.0)
		part(ear, "ear_L" if sx > 0 else "ear_R")
	part(geo.tube([(0, 0.90, 0.79), (0, 0.96, 0.71), (0, 0.98, 0.61)], [0.032, 0.026, 0.022], mat=fur_dark, segments=8, name="tail"), "tail")
	part(geo.blob((0.035, 0.03, 0.06), (0, 0.985, 0.58), mat=bristle, name="tail_tuft"), "tail")

	# Short strong legs with dark stockings and hooves.
	for s, sx in (("L", 1.0), ("R", -1.0)):
		x = FX * sx
		part(geo.tube([(x, -0.36, 0.62), (x, -0.28, 0.40)], [0.10, 0.08], mat=fur, segments=10, name="fu"), "front_upper_" + s)
		part(geo.tube([(x, -0.28, 0.40), (x, -0.34, 0.18)], [0.08, 0.062], mat=fur_dark, segments=10, name="fl"), "front_lower_" + s)
		part(geo.tube([(x, -0.34, 0.18), (x, -0.36, 0.05)], [0.06, 0.056], mat=fur_dark, segments=10, name="ff"), "front_foot_" + s)
		part(geo.blob((0.066, 0.08, 0.045), (x, -0.375, 0.042), mat=hoof, name="hoof"), "front_foot_" + s)
		part(geo.tube([(x, 0.51, 0.62), (x, 0.42, 0.38)], [0.12, 0.09], mat=fur, segments=10, name="bu"), "back_upper_" + s)
		part(geo.tube([(x, 0.42, 0.38), (x, 0.54, 0.19)], [0.085, 0.062], mat=fur_dark, segments=10, name="bl"), "back_lower_" + s)
		part(geo.blob(0.064, (x, 0.54, 0.19), mat=fur_dark, name="hock"), "back_lower_" + s)
		part(geo.tube([(x, 0.54, 0.19), (x, 0.525, 0.05)], [0.06, 0.056], mat=fur_dark, segments=10, name="bf"), "back_foot_" + s)
		part(geo.blob((0.066, 0.08, 0.045), (x, 0.515, 0.042), mat=hoof, name="hoof"), "back_foot_" + s)

	# The moss mantle: clumps that sit on the back and follow its curve, spilling down the flanks.
	back = surface([hull])
	mantle = []
	clumps = [(0.0, -0.30, 0.26, 0.22, 0.15), (0.12, -0.08, 0.20, 0.20, 0.13), (-0.12, -0.02, 0.20, 0.22, 0.13),
			  (0.02, 0.20, 0.24, 0.22, 0.12), (0.10, 0.44, 0.18, 0.18, 0.11), (-0.10, 0.50, 0.17, 0.17, 0.10),
			  (0.0, 0.70, 0.12, 0.12, 0.07)]
	for i, (cx, cy, rx, ry, rz) in enumerate(clumps):
		z = top(back, cx, cy)
		mantle.append(geo.blob((rx, ry, rz), (cx, cy, z - rz * 0.1), (0, 0, 20 * i), mat=moss, lumpy=0.25, seed=11 + i, name="moss"))
	# Moss spilling over the cushion's edge onto the flanks.
	for sx in (1.0, -1.0):
		for (dy, dz, ry) in ((-0.30, 0.97, 0.15), (0.24, 0.90, 0.14)):
			y = dy + 0.04 * sx
			mantle.append(geo.blob((0.07, ry, 0.09), (0.31 * sx, y, dz), (0, 25 * sx, 0), mat=moss_dark, lumpy=0.25, seed=int(40 + dy * 10 + sx), name="drape"))
	for o in mantle:
		geo.tag(o, "pulse")
		soft.append(tag_body(o))
	tree = surface(mantle)

	# Tusks curving up out of the lower jaw; small amber eyes.
	for sx in (1.0, -1.0):
		pts = [(0.07 * sx, -1.00, 0.52), (0.115 * sx, -1.04, 0.54), (0.145 * sx, -1.055, 0.60), (0.15 * sx, -1.03, 0.67), (0.13 * sx, -0.99, 0.72)]
		part(geo.tube(pts, [0.03, 0.027, 0.021, 0.013, 0.004], mat=tusk, segments=7, name="tusk"), "snout", hard)
		part(geo.sphere(0.028, (0.155 * sx, -0.84, 0.80), mat=eye, segments=8, rings=6, name="eye"), "head", hard)

	# Ferns: a tall clump over the shoulders and a smaller one on the rump; fronds arch out and droop.
	rnd = geo.rng(5)
	for (cx, cy, count, length, rise, n) in ((0.03, -0.28, 6, 0.42, 0.24, 10), (-0.10, 0.44, 4, 0.30, 0.17, 8)):
		z = top(tree, cx, cy) - 0.03
		for k in range(count):
			hd = 2.0 * math.pi * k / count + rnd.uniform(-0.3, 0.3)
			fr = frond((cx, cy, z), hd, length * rnd.uniform(0.85, 1.1), rise * rnd.uniform(0.8, 1.15), 0.075, fern, droop=0.15, n=n)
			geo.tag(fr, "sway", "foliage")
			part(fr, body_bone(cy), hard)
		for k in range(2 if count > 4 else 1):
			hd = rnd.uniform(0, 2.0 * math.pi)
			fr = frond((cx, cy, z), hd, length * 0.35, rise * 1.2, 0.05, fern, droop=0.9, n=6)
			geo.tag(fr, "sway", "foliage")
			part(fr, body_bone(cy), hard)

	# Cream-capped mushrooms and spore motes nestled in the moss.
	for (mx, my, r, h) in ((0.16, 0.04, 0.10, 0.12), (0.20, 0.30, 0.066, 0.08), (0.12, 0.60, 0.052, 0.06)):
		z = top(tree, mx, my) - 0.03
		part(geo.cylinder(r * 0.28, h + 0.03, (mx, my, z + (h + 0.03) * 0.5), mat=stem, segments=8, radius_top=r * 0.22, name="stem"), body_bone(my), hard)
		part(geo.lathe([(r * 0.25, 0.0), (r * 0.95, r * 0.08), (r, r * 0.25), (r * 0.8, r * 0.55), (r * 0.45, r * 0.72), (0.0, r * 0.78)],
					   (mx, my, z + h + 0.01), mat=cap, segments=10, name="cap"), body_bone(my), hard)
	for (gx, gy, r) in ((0.22, -0.12, 0.045), (-0.18, 0.28, 0.04), (0.06, 0.70, 0.035)):
		z = top(tree, gx, gy)
		g = geo.sphere(r, (gx, gy, z + r * 0.2), mat=spore, segments=10, rings=7, name="spore")
		geo.tag(g, "pulse")
		part(g, body_bone(gy), hard)

	res = pipeline.mixed("boar", hard, soft, voxel=0.016, tris=3400, size=512)
	low = res["mesh"]

	arm = rig.armature("boar_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)
	skin_points(low)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("sleep", 0), ("wake", 18), ("idle", 0), ("run", 0),
							 ("run", 4), ("run", 8), ("run", 12), ("windup", 9),
							 ("windup", 26), ("strike", 6), ("recover", 8), ("die", 26)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 2.9
	return res


# ---- animation ------------------------------------------------------------------------------------

def bump(t, at, width):
	d = (t - at) / width
	return math.exp(-d * d)


SLEEP_FRONT = {"front_upper_L": (40, 0, 0), "front_lower_L": (-120, 0, 0), "front_foot_L": (140, 0, 0),
			   "front_upper_R": (40, 0, 0), "front_lower_R": (-120, 0, 0), "front_foot_R": (140, 0, 0)}
SLEEP_BACK = {"back_upper_L": (-70, -8, 0), "back_lower_L": (125, 0, 0), "back_foot_L": (30, 0, 0),
			  "back_upper_R": (-70, 8, 0), "back_lower_R": (125, 0, 0), "back_foot_R": (30, 0, 0)}
SLEEP_ROOT = (0, 0.02, -0.38)


def sleep_pose(f, frames=120):
	t = f / float(frames)
	br = math.sin(2.0 * math.pi * 3.0 * t)
	pose = {"spine": (-1.5 * br, 0, 0), "chest": (1.0 * br, 0, 0), "neck": (4, 0, 0), "head": (6, 0, 4),
			"snout": (2 + 3 * bump(t, 0.55, 0.03), 0, 0),
			"ear_L": (-25 + 30 * bump(t, 0.3, 0.02), 0, 10), "ear_R": (-25, 0, -10),
			"tail": (10, 0, 15 * math.sin(2 * math.pi * t))}
	pose.update(SLEEP_FRONT)
	pose.update(SLEEP_BACK)
	return pose, (SLEEP_ROOT[0], SLEEP_ROOT[1], SLEEP_ROOT[2] + 0.008 * br), {}


def idle_pose(f, frames=75):
	t = f / float(frames)
	br = math.sin(2.0 * math.pi * 2.0 * t)
	look = math.sin(2.0 * math.pi * t)
	sniff = bump(t, 0.5, 0.12)
	pose = {"hips": (0, 0, 0), "spine": (-1.0 * br, 0, 0), "chest": (0.8 * br, 0, 0),
			"neck": (2 + 6 * sniff, 0, 4 * look), "head": (3 + 8 * sniff, 0, 6 * look),
			"snout": (3 * math.sin(2 * math.pi * 9 * t) * sniff, 0, 0),
			"ear_L": (-30 * bump(t, 0.22, 0.03), 0, 0), "ear_R": (-30 * bump(t, 0.78, 0.03), 0, 0),
			"tail": (15, 0, 25 * math.sin(2 * math.pi * 3 * t))}
	return pose, (0, 0, 0.006 * br), {}


GALLOP = {"back_L": 0.00, "back_R": 0.10, "front_L": 0.40, "front_R": 0.50}
DUTY = 0.34


def run_pose(f):
	p = f / float(RUN_FRAMES)
	w = 2.0 * math.pi * p
	stride = RUN_SPEED * RUN_FRAMES / 30.0
	pose = {"hips": (-4.0 * math.sin(w - 0.6), 0, 0), "spine": (3.0 * math.sin(w + 0.9), 0, 0), "chest": (-2.0 * math.sin(w + 0.9), 0, 0),
			"neck": (8 + 4 * math.sin(w - 2.0), 0, 0), "head": (6 - 3 * math.sin(w - 2.0), 0, 0),
			"snout": (-4, 0, 0), "ear_L": (-40, 0, -8), "ear_R": (-40, 0, 8),
			"tail": (95 + 12 * math.sin(w), 0, 0)}
	root = (0, 0, -0.01 + 0.03 * math.cos(w - 2.0 * math.pi * 0.93))
	feet = {}
	for name, touch in GALLOP.items():
		q = (p - touch) % 1.0
		c = CONTACT[name]
		s = stride * DUTY
		y0 = c.y - (0.03 if name.startswith("front") else 0.0)
		if q < DUTY:
			k = q / DUTY
			feet[name] = ((c.x, y0 - s * 0.5 + s * k, 0.0), (20.0 * max(0.0, k - 0.6) / 0.4,), 0.0)
		else:
			u = (q - DUTY) / (1.0 - DUTY)
			carry = 20.0 * (1.0 - u) ** 4
			front = name.startswith("front")
			if front:
				fold = 95.0 * math.sin(math.pi * min(1.0, u / 0.8)) if u < 0.8 else -10.0 * math.sin(math.pi * (u - 0.8) / 0.2)
			else:
				fold = 50.0 * math.sin(math.pi * min(1.0, u / 0.85)) - 12.0 * math.sin(math.pi * max(0.0, u - 0.85) / 0.15)
			ends = (fold + carry,)
			tgt = swing_target(name, y0 + s * 0.5, (20.0,), y0 - s * 0.5, (0.0,), ends, u, 0.08 if front else 0.06)
			feet[name] = (tgt, ends, 0.0)
	return pose, root, feet


def foot(name, dy=0.0, z=0.0, end=0.0):
	c = CONTACT[name]
	return ((c.x, c.y + dy, z), (end,), 0.0)


def roll_root(deg, cz=0.76, rx=0.37, rz=0.36):
	# Root offset that keeps a body of this cross-section resting on the ground as it rolls over.
	a = math.radians(deg)
	zc = math.cos(a) * cz
	ext = math.sqrt((rx * math.sin(a)) ** 2 + (rz * math.cos(a)) ** 2)
	return (-math.sin(a) * cz * 0.85, 0.0, max(0.0, ext - zc))


def animations(arm):
	clips = []

	sleep_keys = sample(sleep_pose, 120, ground="floor")
	clips.append(rig.clip(arm, "sleep", sleep_keys, loop=True))

	s0, r0, _ = sleep_pose(0)
	i0, ir0, _ = idle_pose(0)
	kneel = dict(SLEEP_BACK)
	kneel.update({"front_upper_L": (10, 0, 0), "front_lower_L": (-70, 0, 0), "front_foot_L": (90, 0, 0),
				  "front_upper_R": (10, 0, 0), "front_lower_R": (-70, 0, 0), "front_foot_R": (90, 0, 0)})
	sit_back = {"back_upper_L": (-45, -4, 0), "back_lower_L": (90, 0, 0), "back_foot_L": (20, 0, 0),
				"back_upper_R": (-45, 4, 0), "back_lower_R": (90, 0, 0), "back_foot_R": (20, 0, 0)}
	wake = [(0, s0, r0, {}),
			(8, dict(kneel, neck=(-6, 0, 0), head=(-2, 0, 0), ear_L=(10, 0, 0), ear_R=(10, 0, 0)), (0, 0.02, -0.30), {}),
			(17, dict(sit_back, hips=(-14, 0, 0), neck=(-4, 0, 0), head=(0, 0, 0)), (0, 0.05, -0.20), {}),
			(25, dict(hips=(-2, 0, 0), spine=(2, 0, 0), neck=(2, 0, 0), head=(2, 0, 0)), (0, 0.01, -0.02), {}),
			(30, i0, ir0, {})]
	clips.append(rig.clip(arm, "wake", track(wake, ground="floor"), loop=False))

	clips.append(rig.clip(arm, "idle", sample(idle_pose, 75), loop=True))
	clips.append(rig.clip(arm, "run", sample(run_pose, RUN_FRAMES, smooth=12), loop=True))

	low = {"neck": (12, 0, 0), "head": (14, 0, 0), "ear_L": (-35, 0, 0), "ear_R": (-35, 0, 0), "tail": (40, 0, 0)}
	snort = dict(low, snout=(-14, 0, 0), head=(6, 0, 0))
	ready = {"hips": (-3, 0, 0), "spine": (3, 0, 0), "neck": (14, 0, 0), "head": (16, 0, 0), "snout": (-4, 0, 0),
			 "ear_L": (-45, 0, -6), "ear_R": (-45, 0, 6), "tail": (60, 0, 0)}
	windup = [(0, i0, ir0, {}),
			  (5, low, (0, 0.04, -0.04), {"front_R": foot("front_R", -0.06, 0.10, 40)}),
			  (9, snort, (0, 0.05, -0.04), {"front_R": foot("front_R", 0.15, 0.0, 10)}, "lin"),
			  (13, low, (0, 0.05, -0.05), {"front_R": foot("front_R", -0.07, 0.10, 40)}),
			  (17, dict(snort, snout=(-16, 0, 0)), (0, 0.06, -0.05), {"front_R": foot("front_R", 0.15, 0.0, 10)}, "lin"),
			  (21, low, (0, 0.06, -0.06), {}),
			  (26, ready, (0, 0.07, -0.07), {})]
	clips.append(rig.clip(arm, "windup", track(windup), loop=False))

	launch = {"hips": (3, 0, 0), "spine": (-2, 0, 0), "neck": (8, 0, 0), "head": (12, 0, 0), "snout": (-6, 0, 0),
			  "ear_L": (-55, 0, 0), "ear_R": (-55, 0, 0), "tail": (90, 0, 0),
			  "front_upper_L": (-45, 0, 0), "front_lower_L": (-10, 0, 0), "front_foot_L": (45, 0, 0),
			  "front_upper_R": (-35, 0, 0), "front_lower_R": (-15, 0, 0), "front_foot_R": (50, 0, 0),
			  "back_upper_L": (40, 0, 0), "back_lower_L": (5, 0, 0), "back_foot_L": (25, 0, 0),
			  "back_upper_R": (35, 0, 0), "back_lower_R": (10, 0, 0), "back_foot_R": (25, 0, 0)}
	hook = {"hips": (-6, 0, 0), "spine": (-2, 0, 0), "neck": (-10, 0, 0), "head": (-24, 0, 0), "snout": (-10, 0, 0),
			"ear_L": (-50, 0, 0), "ear_R": (-50, 0, 0), "tail": (80, 0, 0)}
	land = {"hips": (1, 0, 0), "neck": (4, 0, 0), "head": (2, 0, 0), "ear_L": (-30, 0, 0), "ear_R": (-30, 0, 0), "tail": (50, 0, 0)}
	strike = [(0, ready, (0, 0.07, -0.07), {}),
			  (3, launch, (0, -0.10, 0.07), {}, "out"),
			  (6, hook, (0, -0.16, 0.06), {"front_L": foot("front_L", -0.16), "front_R": foot("front_R", -0.10),
										   "back_L": foot("back_L", 0.10, 0.06, 20), "back_R": foot("back_R", 0.12, 0.04, 20)}, "lin"),
			  (10, land, (0, -0.02, -0.04), {})]
	clips.append(rig.clip(arm, "strike", track(strike), loop=False))

	def shake(a, d=0.0):
		return {"neck": (6, 0, a * 0.4), "head": (4, a * 0.6, a), "snout": (0, 0, -a * 0.3),
				"ear_L": (-20, 0, -a * 1.2), "ear_R": (-20, 0, -a * 1.2), "tail": (30, 0, a), "chest": (0, -a * 0.1, 0)}
	recover = [(0, land, (0, -0.02, -0.04), {}),
			   (4, shake(-26), (0, 0, -0.03), {}),
			   (8, shake(26), (0, 0, -0.03), {}),
			   (12, shake(-22), (0, 0, -0.03), {}),
			   (16, shake(16), (0, 0, -0.03), {}),
			   (20, shake(-8), (0, 0, -0.03), {}),
			   (27, {"neck": (10, 0, 0), "head": (10, 0, 0), "snout": (-6, 0, 0), "spine": (2, 0, 0), "tail": (15, 0, 0)}, (0, 0, -0.04), {}),
			   (34, {"neck": (4, 0, 0), "head": (5, 0, 0), "tail": (15, 0, 0)}, (0, 0, -0.01), {}),
			   (42, i0, ir0, {})]
	clips.append(rig.clip(arm, "recover", track(recover), loop=False))

	limp = {"front_upper_L": (-10, 25, 0), "front_lower_L": (15, 0, 0), "front_foot_L": (20, 0, 0),
			"front_upper_R": (-15, 10, 0), "front_lower_R": (30, 0, 0), "front_foot_R": (15, 0, 0),
			"back_upper_L": (5, 25, 0), "back_lower_L": (10, 0, 0), "back_foot_L": (15, 0, 0),
			"back_upper_R": (15, 10, 0), "back_lower_R": (20, 0, 0), "back_foot_R": (10, 0, 0)}
	buckle = {"front_upper_L": (30, 0, 0), "front_lower_L": (-80, 0, 0), "front_foot_L": (70, 0, 0),
			  "front_upper_R": (35, 0, 0), "front_lower_R": (-90, 0, 0), "front_foot_R": (80, 0, 0),
			  "back_upper_L": (-30, 0, 0), "back_lower_L": (60, 0, 0), "back_foot_L": (10, 0, 0),
			  "back_upper_R": (-35, 0, 0), "back_lower_R": (70, 0, 0), "back_foot_R": (10, 0, 0)}

	def fallen(deg, extra):
		p = dict(extra)
		p["root"] = (0, deg, 0)
		return p
	die = [(0, i0, ir0, {}),
		   (6, {"hips": (0, -4, 0), "neck": (14, 0, 0), "head": (12, 0, -6), "ear_L": (-30, 0, 0), "ear_R": (-30, 0, 0)}, (0, 0, -0.08), {}),
		   (13, fallen(-25, dict(buckle, neck=(16, 0, 0), head=(14, 0, -10), ear_L=(-40, 0, 0), ear_R=(-40, 0, 0))), (0.10, 0, -0.30), {}, "in"),
		   (20, fallen(-70, dict(limp, neck=(10, 0, 0), head=(6, 6, 0))), roll_root(-70), {}, "in"),
		   (24, fallen(-93, dict(limp, neck=(4, 0, 0), head=(0, 12, 0), tail=(20, 0, 0))), roll_root(-93), {}, "lin"),
		   (27, fallen(-90, dict(limp, neck=(4, 0, 0), head=(0, 14, 0), tail=(20, 0, 0))), roll_root(-90), {})]
	clips.append(rig.clip(arm, "die", track(die, ground="floor", snap_from=13), loop=False))
	return clips
