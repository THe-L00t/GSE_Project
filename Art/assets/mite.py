# Spore mite: a tick fat with spores. A bloated hide, a glowing spore sac bursting through dark shell
# plates on its back, a small armoured head with mandibles and two amber eyes, six jointed legs.
import math

import bpy
from mathutils import Euler, Quaternion, Vector

from gse import geo, mat, pipeline, rig

# Legs on the left (+X): coxa, knee, foot tip. The right side mirrors them.
LEG_L = {
	"f": ((0.12, -0.27, 0.25), -52.0, 0.14, 0.14, 0.34),
	"m": ((0.15, -0.18, 0.24), -6.0, 0.13, 0.13, 0.31),
	"b": ((0.15, -0.09, 0.24), 30.0, 0.155, 0.12, 0.32),
}
FOOT_Z = 0.004
SAC_C = (0.0, 0.14, 0.50)
SAC_R = 0.19
SAC_BONES = (("sac_T", 1.0), ("sac_L", 0.8), ("sac_R", 0.8), ("sac_F", 0.8), ("sac_B", 0.8))


def leg_points(key, sx):
	(ax, ay, az), phi, fh, fv, reach = LEG_L[key]
	d = Vector((math.cos(math.radians(phi)) * sx, math.sin(math.radians(phi)), 0.0))
	a = Vector((ax * sx, ay, az))
	k = a + d * fh + Vector((0, 0, fv))
	f = a + d * reach
	f.z = FOOT_Z
	return a, k, f


LEGS = {}
for _k in ("f", "m", "b"):
	for _s, _sx in (("L", 1.0), ("R", -1.0)):
		LEGS[_k + "_" + _s] = leg_points(_k, _sx)

BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("body", (0, -0.06, 0.27), (0, -0.28, 0.27), "root"),
	("abdomen", (0, -0.02, 0.30), (0, 0.38, 0.30), "body"),
	("head", (0, -0.30, 0.26), (0, -0.44, 0.25), "body"),
	("mandible_L", (0.028, -0.425, 0.24), (0.022, -0.505, 0.17), "head"),
	("mandible_R", (-0.028, -0.425, 0.24), (-0.022, -0.505, 0.17), "head"),
	("sac_T", SAC_C, (0.0, 0.14, 0.70), "abdomen"),
	("sac_L", SAC_C, (0.19, 0.14, 0.50), "abdomen"),
	("sac_R", SAC_C, (-0.19, 0.14, 0.50), "abdomen"),
	("sac_F", SAC_C, (0.0, -0.08, 0.50), "abdomen"),
	("sac_B", SAC_C, (0.0, 0.36, 0.50), "abdomen"),
]
for _n, (_a, _k, _f) in LEGS.items():
	BONES.append(("femur_" + _n, tuple(_a), tuple(_k), "body"))
	BONES.append(("tibia_" + _n, tuple(_k), tuple(_f), "femur_" + _n))
DEFORM = [b[0] for b in BONES if b[0] != "root"]
BODY_HEAD = Vector((0, -0.06, 0.27))


def clamp(x, lo=0.0, hi=1.0):
	return max(lo, min(hi, x))


def smooth(t):
	t = clamp(t)
	return t * t * (3.0 - 2.0 * t)


def weigh(obj, fn):
	bpy.context.view_layer.update()
	mw = obj.matrix_world
	groups = {}
	for v in obj.data.vertices:
		for n, w in fn(mw @ v.co).items():
			if w <= 0.001:
				continue
			g = groups.get(n)
			if g is None:
				g = obj.vertex_groups.get(n) or obj.vertex_groups.new(name=n)
				groups[n] = g
			g.add([v.index], min(1.0, w), 'REPLACE')
	return obj


def sac_weights(co):
	# The sac inflates along five bones; squared direction components share the skin between them.
	v = Vector(co) - Vector(SAC_C)
	if v.length < 1e-6:
		return {"sac_T": 1.0}
	v.normalize()
	return {"sac_T": max(0.0, v.z) ** 2, "sac_L": max(0.0, v.x) ** 2, "sac_R": max(0.0, -v.x) ** 2,
			"sac_F": max(0.0, -v.y) ** 2, "sac_B": max(0.0, v.y) ** 2, "abdomen": max(0.0, -v.z) ** 2}


def build():
	hide = mat.make("mite_hide", (0.43, 0.41, 0.35), pattern="skin", rough=0.75, bump=0.35, var=0.18, var_scale=5.0)
	shell = mat.get("mite_shell")
	head_mat = mat.make("mite_head", (0.22, 0.25, 0.20), pattern="shell", rough=0.5, bump=0.5, edge=0.3)
	leg_mat = mat.make("mite_leg", (0.20, 0.19, 0.16), pattern="plain", rough=0.55, bump=0.2, edge=0.3)
	band = mat.make("mite_band", (0.48, 0.45, 0.36), pattern="plain", rough=0.6, bump=0.15)
	vein = mat.make("mite_vein", (0.10, 0.22, 0.20), pattern="plain", rough=0.45, bump=0.2, grime=0.1)
	sac_mat = mat.get("spore_glow")
	eye = mat.get("lamp_glow")

	parts = []

	def part(obj, *tags):
		geo.tag(obj, *tags)
		parts.append(obj)
		return obj

	# Bloated abdomen, front body, head.
	part(geo.blob((0.255, 0.31, 0.20), (0, 0.125, 0.30), mat=hide, lumpy=0.06, seed=3, name="abdomen"), "abdomen")
	part(geo.blob((0.16, 0.15, 0.11), (0, -0.19, 0.26), mat=hide, lumpy=0.05, seed=4, name="podosoma"), "body")
	part(geo.blob((0.14, 0.10, 0.04), (0, -0.165, 0.335), (-4, 0, 0), mat=shell, lumpy=0.12, seed=5, name="scutum"), "body")
	part(geo.blob((0.085, 0.085, 0.065), (0, -0.35, 0.25), mat=head_mat, lumpy=0.05, seed=6, name="head"), "head")
	part(geo.blob((0.06, 0.04, 0.03), (0, -0.34, 0.31), (-10, 0, 0), mat=head_mat, name="head_plate"), "head")
	for sx in (1.0, -1.0):
		part(geo.sphere(0.021, (0.052 * sx, -0.395, 0.29), mat=eye, segments=10, rings=8, name="eye"), "head")
		side = "L" if sx > 0 else "R"
		part(geo.tube([(0.028 * sx, -0.415, 0.245), (0.042 * sx, -0.475, 0.22), (0.022 * sx, -0.505, 0.17)], [0.022, 0.017, 0.011],
					  mat=leg_mat, segments=7, name="mandible"), "mandible_" + side)
		part(geo.tube([(0.06 * sx, -0.40, 0.235), (0.08 * sx, -0.455, 0.20), (0.07 * sx, -0.47, 0.165)], [0.018, 0.02, 0.015],
					  mat=head_mat, segments=7, name="palp"), "head")
	# The spore sac: an egg of spores bursting up through cracked shell plates, netted with dark veins.
	sac = geo.blob((SAC_R, SAC_R * 1.15, SAC_R * 0.95), SAC_C, (-8, 0, 0), mat=sac_mat, subdivisions=4, lumpy=0.09, seed=9, name="sac")
	geo.tag(sac, "pulse")
	parts.append(weigh(sac, sac_weights))
	rnd = geo.rng(11)
	def on_sac(az, el, lift=0.004):
		d = Vector((math.cos(az) * math.cos(el), math.sin(az) * math.cos(el) * 1.15, math.sin(el) * 0.95))
		return Vector(SAC_C) + d * (SAC_R + lift)
	for i in range(5):
		az = i / 5.0 * math.tau + rnd.uniform(-0.4, 0.4)
		el = math.radians(rnd.uniform(62.0, 75.0))
		pts = []
		for j in range(6):
			pts.append(on_sac(az, el))
			az += rnd.uniform(-0.35, 0.35)
			el -= math.radians(rnd.uniform(9.0, 14.0))
		v = geo.tube(pts, [0.005, 0.008, 0.010, 0.011, 0.012, 0.013], mat=vein, segments=6, name="vein")
		parts.append(weigh(v, sac_weights))
		k = rnd.randint(2, 3)
		rel = pts[k] - Vector(SAC_C)
		br_az = math.atan2(rel.y, rel.x) + rnd.choice((-1.0, 1.0)) * 0.45
		br = geo.tube([pts[k], on_sac(br_az, math.radians(30.0))], [0.008, 0.005], mat=vein, segments=5, name="vein_branch")
		parts.append(weigh(br, sac_weights))
	# Dark seeds pressed against the membrane from inside.
	for i in range(9):
		c = on_sac(rnd.uniform(0.0, math.tau), math.radians(rnd.uniform(25.0, 70.0)), -0.004)
		d = geo.sphere(rnd.uniform(0.011, 0.016), c, mat=vein, segments=8, rings=6, name="seed")
		d.scale = (1.0, 1.0, 0.55)
		parts.append(weigh(d, sac_weights))
	# Shell plates: a ring cracked open around the sac.
	n = 8
	for i in range(n):
		a = i / float(n) * math.tau + 0.2
		c = (SAC_C[0] + math.cos(a) * 0.19, SAC_C[1] + math.sin(a) * 0.23, SAC_C[2] - 0.03 + 0.02 * math.sin(i * 2.3))
		p = geo.blob((0.11, 0.038, 0.085), c, (-22 + 6 * math.sin(i * 1.7), 0, math.degrees(a) + 90.0), mat=shell, lumpy=0.2, seed=20 + i, name="plate")
		part(p, "abdomen")
	for i in range(5):
		a = i / 5.0 * math.tau + 1.0
		c = (math.cos(a) * 0.18, 0.14 + math.sin(a) * 0.24, 0.39)
		part(geo.blob((0.09, 0.03, 0.06), c, (-50, 0, math.degrees(a) + 90.0), mat=shell, lumpy=0.2, seed=40 + i, name="plate_low"), "abdomen")

	# Six jointed legs, banded at the joints.
	for name, (a, k, f) in LEGS.items():
		part(geo.blob(0.046, a, mat=head_mat, name="coxa"), "body")
		part(geo.tube([a + (k - a) * 0.3, k], [0.036, 0.032], mat=leg_mat, segments=8, name="femur"), "femur_" + name)
		mid = k + (f - k) * 0.55
		out = Vector((f.x - k.x, f.y - k.y, 0.0)).normalized() * 0.025
		part(geo.tube([k, mid + out, f + Vector((0, 0, 0.004))], [0.03, 0.022, 0.012], mat=leg_mat, segments=8, name="tibia"), "tibia_" + name)
		part(geo.sphere(0.034, k, mat=band, segments=10, rings=8, name="knee"), "tibia_" + name)
		part(geo.tube([mid + out - (f - k) * 0.04, mid + out + (f - k) * 0.04], 0.025, mat=band, segments=8, name="ankle"), "tibia_" + name)

	res = pipeline.organic("mite", parts, voxel=0.0075, tris=4200, size=512, smooth=2)
	low = res["mesh"]

	arm = rig.armature("mite_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=2, factor=0.5)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("walk", 0), ("walk", 2), ("windup", 14), ("strike", 5), ("recover", 3),
							 ("die", 10), ("die", 27)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 1.5
	return res


# ---- leg solving ------------------------------------------------------------------------------

def euler_deg(q):
	e = q.to_euler('XYZ')
	return (math.degrees(e.x), math.degrees(e.y), math.degrees(e.z))


def rot(e):
	return Euler([math.radians(a) for a in e], 'XYZ').to_quaternion()


def to_body(w, root, root_rot, body_rot):
	# A world point in the body's rest space, so the legs can be solved as if nothing moved.
	p = rot(root_rot).inverted() @ (Vector(w) - Vector(root))
	return BODY_HEAD + rot(body_rot).inverted() @ (p - BODY_HEAD)


def leg_frame(name):
	a, k, f = LEGS[name]
	phi = math.atan2(f.y - a.y, f.x - a.x)
	axis = Vector((math.sin(phi), -math.cos(phi), 0.0))
	a0 = math.atan2(k.z - a.z, (k - a).xy.length)
	b0 = math.atan2(f.z - k.z, (f - k).xy.length)
	return a, k, f, phi, axis, a0, b0


def solve(name, target):
	# Two-bone solve in the leg's vertical plane, knee kept high, then a turn about the coxa.
	a, k, f, phi, axis, a0, b0 = leg_frame(name)
	l1 = (k - a).length
	l2 = (f - k).length
	t = Vector(target)
	dx, dy = t.x - a.x, t.y - a.y
	h = math.hypot(dx, dy)
	v = t.z - a.z
	d = clamp(math.hypot(h, v), abs(l1 - l2) + 1e-3, l1 + l2 - 1e-3)
	ang = math.atan2(v, h) + math.acos(clamp((l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d), -1.0, 1.0))
	kx, kz = l1 * math.cos(ang), l1 * math.sin(ang)
	bng = math.atan2(v - kz, h - kx)
	yaw = math.atan2(dy, dx) - phi
	yaw = (yaw + math.pi) % math.tau - math.pi
	qf = Quaternion((0, 0, 1), yaw) @ Quaternion(axis, ang - a0)
	qt = Quaternion(axis, (bng - b0) - (ang - a0))
	return {"femur_" + name: euler_deg(qf), "tibia_" + name: euler_deg(qt)}


def curl(name, lift, knee, yaw=0.0):
	# Direct leg angles (degrees) for poses off the ground: lift raises the femur, knee opens the joint.
	a, k, f, phi, axis, a0, b0 = leg_frame(name)
	qf = Quaternion((0, 0, 1), math.radians(yaw)) @ Quaternion(axis, math.radians(lift))
	qt = Quaternion(axis, math.radians(knee))
	return {"femur_" + name: euler_deg(qf), "tibia_" + name: euler_deg(qt)}


def rest_foot(name):
	return LEGS[name][2].copy()


def pose(body=(0, 0, 0), root=(0, 0, 0), root_rot=(0, 0, 0), feet=None, curls=None, extra=None):
	# feet: {leg: world target}; legs not listed stand on their rest footprints, unless curled.
	out = {"body": body}
	if root_rot != (0, 0, 0):
		out["root"] = root_rot
	feet = feet or {}
	curls = curls or {}
	for name in LEGS:
		if name in curls:
			out.update(curl(name, *curls[name]))
		else:
			t = feet.get(name, rest_foot(name))
			out.update(solve(name, to_body(t, root, root_rot, body)))
	if extra:
		out.update(extra)
	return out


def swell_keys(arm, keys):
	# The sac bones slide along their own length: out to swell, in to shrink.
	for f, s in keys:
		for bone, k in SAC_BONES:
			pb = arm.pose.bones[bone]
			pb.location = (0.0, s * k, 0.0)
			pb.keyframe_insert("location", frame=f)


def make_clip(arm, name, frames, loop):
	# frames: list of (frame, pose, root, swell)
	c = rig.clip(arm, name, [(f, p, r) for (f, p, r, s) in frames], loop=loop)
	swell_keys(arm, [(f, s) for (f, p, r, s) in frames])
	return c


def jaws(open_deg):
	return {"mandible_L": (0, 0, open_deg), "mandible_R": (0, 0, -open_deg)}


def animations(arm):
	clips = []

	# Idle: the sac breathes, the body settles under it, feet tap and shift, the jaws work.
	n = 45
	frames = []
	for f in range(0, n + 1, 3):
		t = f / float(n) * math.tau
		sw = 0.03 * math.sin(t) + 0.006 * math.sin(2.0 * t)
		root = (0.0, 0.0, -0.004 - 0.006 * math.sin(t))
		body = (1.5 * math.sin(t + 0.6), 1.0 * math.sin(t * 2.0), 0.0)
		feet = {}
		tap = clamp(1.0 - abs(f - 11) / 5.0)
		if tap > 0.0:
			p = rest_foot("f_R")
			feet["f_R"] = p + Vector((0.0, -0.02 * tap, 0.045 * tap))
		shift = clamp(1.0 - abs(f - 30) / 6.0)
		if shift > 0.0:
			p = rest_foot("b_L")
			feet["b_L"] = p + Vector((0.01 * shift, 0.015 * shift, 0.035 * shift))
		twitch = clamp(1.0 - abs(f - 39) / 3.0)
		if twitch > 0.0:
			p = rest_foot("m_R")
			feet["m_R"] = p + Vector((0.0, 0.0, 0.03 * twitch))
		extra = jaws(6.0 + 6.0 * math.sin(t * 3.0))
		extra["head"] = (2.0 * math.sin(t), 0.0, 5.0 * math.sin(t + 1.0))
		extra["abdomen"] = (-1.5 * math.sin(t), 0.0, 1.5 * math.sin(t * 2.0))
		frames.append((f, pose(body, root, feet=feet, extra=extra), root, sw))
	clips.append(make_clip(arm, "idle", frames, True))

	# Walk: alternating tripods, 6 frames a cycle, two cycles in the clip. Planted feet slide back
	# 0.32 m per half cycle, so one cycle covers 0.64 m (3.2 m/s at 30 fps).
	stride, lift, cyc = 0.32, 0.07, 6.0
	tripod = {"f_L": 0.0, "m_R": 0.0, "b_L": 0.0, "f_R": 0.5, "m_L": 0.5, "b_R": 0.5}
	frames = []
	for f in range(0, 13):
		ph = f / cyc
		root = (0.0, 0.0, -0.01 + 0.008 * math.cos(ph * math.tau * 2.0))
		body = (0.0, 3.0 * math.sin(ph * math.tau), 0.0)
		feet = {}
		for name, off in tripod.items():
			p = (ph + off) % 1.0
			r = rest_foot(name)
			if p < 0.5:
				u = p / 0.5
				feet[name] = r + Vector((0.0, -stride * 0.5 + stride * u, 0.0))
			else:
				u = (p - 0.5) / 0.5
				feet[name] = r + Vector((0.0, stride * 0.5 - stride * smooth(u), lift * math.sin(math.pi * u)))
		extra = jaws(4.0 + 4.0 * math.sin(ph * math.tau * 2.0))
		extra["abdomen"] = (0.0, -2.0 * math.sin(ph * math.tau - 0.8), 4.0 * math.sin(ph * math.tau - 1.2))
		extra["head"] = (0.0, 0.0, -3.0 * math.sin(ph * math.tau))
		frames.append((f, pose(body, root, feet=feet, extra=extra), root, 0.008 * math.sin(ph * math.tau * 2.0)))
	clips.append(make_clip(arm, "walk", frames, True))

	# Windup: it sinks back on its hind legs, raises the front pair, the sac swells and the jaws open.
	def crouch(e):
		root = (0.0, 0.07 * e, -0.05 * e)
		body = (-9.0 * e, 0.0, 0.0)
		feet = {}
		for s, sx in (("L", 1.0), ("R", -1.0)):
			feet["f_" + s] = rest_foot("f_" + s) + Vector((0.05 * sx * e, -0.04 * e, 0.27 * e))
		extra = jaws(28.0 * e)
		extra["head"] = (-8.0 * e, 0.0, 0.0)
		extra["abdomen"] = (7.0 * e, 0.0, 0.0)
		return pose(body, root, feet=feet, extra=extra), root
	frames = []
	for f, e in ((0, 0.0), (4, 0.35), (8, 0.75), (14, 1.0)):
		p, r = crouch(smooth(e) if f < 14 else 1.0)
		frames.append((f, p, r, 0.06 * e))
	windup_end = frames[-1]
	clips.append(make_clip(arm, "windup", frames, False))

	# Strike: it springs, fully stretched, front legs and jaws reaching, hind legs trailing.
	def air(root, body, front, mid, back, jaw, sw):
		feet = {}
		for s, sx in (("L", 1.0), ("R", -1.0)):
			feet["f_" + s] = Vector((front[0] * sx, front[1], front[2]))
			feet["m_" + s] = Vector((mid[0] * sx, mid[1], mid[2]))
			feet["b_" + s] = Vector((back[0] * sx, back[1], back[2]))
		extra = jaws(jaw)
		extra["head"] = (-6.0, 0.0, 0.0)
		return pose(body, root, feet=feet, extra=extra), root, sw
	launch = air((0.0, 0.02, 0.06), (6.0, 0, 0), (0.30, -0.78, 0.36), (0.47, -0.26, 0.12), (0.34, 0.32, 0.014), 34.0, 0.04)
	flight = air((0.0, -0.02, 0.17), (4.0, 0, 0), (0.27, -0.84, 0.24), (0.47, -0.30, 0.20), (0.30, 0.42, 0.26), 36.0, 0.03)
	reach = air((0.0, 0.0, 0.07), (8.0, 0, 0), (0.33, -0.66, 0.05), (0.47, -0.24, 0.07), (0.36, 0.26, 0.11), 10.0, 0.02)
	clips.append(make_clip(arm, "strike", [(0,) + windup_end[1:], (2,) + launch, (5,) + flight, (9,) + reach], False))

	# Recover: it lands hard, legs splayed and the sac wobbling, then gathers itself back to rest.
	def splay(root, body, out, sw, jaw=4.0):
		feet = {}
		for name in LEGS:
			r = rest_foot(name)
			a = LEGS[name][0]
			d = Vector((r.x - a.x, r.y - a.y, 0.0)).normalized()
			feet[name] = r + d * out
		return pose(body, root, feet=feet, extra=jaws(jaw)), root, sw
	land = splay((0.0, 0.0, -0.075), (7.0, 0, 0), 0.07, 0.045)
	rebound = splay((0.0, 0.0, -0.015), (-3.0, 0, 0), 0.05, -0.02)
	settle = splay((0.0, 0.0, -0.03), (2.0, 0, 0), 0.03, 0.012, 10.0)
	rest = (pose(), (0, 0, 0), 0.0)
	clips_recover = [(0,) + reach, (3,) + land, (8,) + rebound, (14,) + settle, (24,) + rest]

	# Die: a spasm, it flips onto its back, the sac empties and the legs curl in.
	def dead(lift, knee, yaw):
		return {n: (lift, knee, yaw * (1.0 if n.endswith("_L") else -1.0) * (1.0 if n.startswith("b") else -1.0)) for n in LEGS}
	spasm_feet = {}
	for name in LEGS:
		spasm_feet[name] = rest_foot(name) + Vector((0, 0, 0.05))
	spasm = pose((0, 14.0, 0), (0.0, 0.0, 0.08), feet=spasm_feet, extra=jaws(30.0))
	tip = pose((0, 0, 0), (0.05, 0.0, 0.43), (0, 50.0, 0), curls=dead(-10.0, 20.0, 0.0), extra=jaws(26.0))
	roll = pose((0, 0, 0), (0.06, 0.0, 0.43), (0, 95.0, 0), curls=dead(-30.0, 10.0, 0.0), extra=jaws(20.0))
	back = pose((0, 0, 0), (0.05, 0.0, 0.625), (0, 165.0, 0), curls=dead(-62.0, -8.0, 6.0), extra=jaws(8.0))
	twitch = pose((0, 0, 0), (0.05, 0.0, 0.612), (0, 160.0, 0), curls=dead(-45.0, 15.0, 4.0), extra=jaws(14.0))
	still = pose((0, 0, 0), (0.05, 0.0, 0.582), (0, 150.0, 0), curls=dead(-70.0, -25.0, 10.0), extra=jaws(2.0))
	die = make_clip(arm, "die", [(0,) + rest, (4, spasm, (0.0, 0.0, 0.08), 0.04), (7, tip, (0.05, 0.0, 0.43), 0.01),
								  (10, roll, (0.06, 0.0, 0.43), -0.02), (16, back, (0.05, 0.0, 0.625), -0.06),
								  (21, twitch, (0.05, 0.0, 0.612), -0.065), (27, still, (0.05, 0.0, 0.582), -0.075)], False)
	clips.append(die)
	# Recover is exported last so the preview frames itself on a mite standing at rest.
	clips.append(make_clip(arm, "recover", clips_recover, False))
	return clips
