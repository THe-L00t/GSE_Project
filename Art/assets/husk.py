# Husk: an empty work coat the spores have learned to wear. No head, no hands: the collar opens onto
# darkness with two spore points deep inside, and lichen strands hang from the empty cuffs.
import math

import bpy

from gse import geo, mat, pipeline, rig

BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("hips", (0, 0, 0.98), (0, 0, 1.12), "root"),
	("spine", (0, 0, 1.12), (0, 0, 1.30), "hips"),
	("chest", (0, 0, 1.30), (0, 0, 1.50), "spine"),
	("collar", (0, 0, 1.50), (0, 0, 1.74), "chest"),
	("shoulder_L", (0.05, 0, 1.45), (0.155, 0, 1.465), "chest"),
	("upperarm_L", (0.155, 0, 1.465), (0.245, 0.01, 1.17), "shoulder_L"),
	("forearm_L", (0.245, 0.01, 1.17), (0.275, -0.03, 0.83), "upperarm_L"),
	("hand_L", (0.275, -0.03, 0.83), (0.28, -0.04, 0.62), "forearm_L"),
	("shoulder_R", (-0.05, 0, 1.45), (-0.155, 0, 1.465), "chest"),
	("upperarm_R", (-0.155, 0, 1.465), (-0.245, 0.01, 1.17), "shoulder_R"),
	("forearm_R", (-0.245, 0.01, 1.17), (-0.275, -0.03, 0.83), "upperarm_R"),
	("hand_R", (-0.275, -0.03, 0.83), (-0.28, -0.04, 0.62), "forearm_R"),
	("thigh_L", (0.095, 0, 0.98), (0.10, 0, 0.53), "hips"),
	("shin_L", (0.10, 0, 0.53), (0.10, 0.02, 0.09), "thigh_L"),
	("foot_L", (0.10, 0.02, 0.09), (0.10, -0.12, 0.03), "shin_L"),
	("thigh_R", (-0.095, 0, 0.98), (-0.10, 0, 0.53), "hips"),
	("shin_R", (-0.10, 0, 0.53), (-0.10, 0.02, 0.09), "thigh_R"),
	("foot_R", (-0.10, 0.02, 0.09), (-0.10, -0.12, 0.03), "shin_R"),
	("skirt_F", (0, -0.06, 1.00), (0, -0.15, 0.60), "hips"),
	("skirt_B", (0, 0.06, 1.00), (0, 0.15, 0.60), "hips"),
	("skirt_L", (0.08, 0, 1.00), (0.20, 0, 0.60), "hips"),
	("skirt_R", (-0.08, 0, 1.00), (-0.20, 0, 0.60), "hips"),
]
DEFORM = [b[0] for b in BONES if b[0] != "root"]
SKIRT = (("skirt_L", 0.0), ("skirt_B", 90.0), ("skirt_R", 180.0), ("skirt_F", -90.0))
# The coat from hem to neck as (radius, z); flattened to DEPTH front to back.
PROFILE = [(0.19, 0.60), (0.178, 0.74), (0.165, 0.95), (0.152, 1.10), (0.156, 1.28), (0.154, 1.40),
		   (0.138, 1.46), (0.105, 1.50), (0.075, 1.52)]
DEPTH = 0.72
CUFF = (0.275, -0.03, 0.82)


def clamp(x, lo=0.0, hi=1.0):
	return max(lo, min(hi, x))


def smoothstep(a, b, x):
	t = clamp((x - a) / (b - a))
	return t * t * (3.0 - 2.0 * t)


def weigh(obj, fn):
	# fn(world_co) -> {group: weight}; several bones per vertex where cloth hangs between them.
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


def skirt_share(co, t):
	# Below the waist the coat hangs from the hips, follows the thigh beneath it and flares on its own bones.
	out = {"hips": 1.0 - t}
	side = smoothstep(-0.07, 0.07, co.x)
	out["thigh_L"] = 0.45 * t * side
	out["thigh_R"] = 0.45 * t * (1.0 - side)
	ang = math.atan2(co.y, co.x)
	for n, a in SKIRT:
		c = math.cos(ang - math.radians(a))
		if c > 0.0:
			out[n] = 0.55 * t * c * c
	return out


def coat_weights(co):
	if co.z >= 1.31:
		return {"chest": 1.0}
	if co.z >= 1.13:
		return {"spine": 1.0}
	if co.z >= 1.00:
		return {"hips": 1.0}
	return skirt_share(co, smoothstep(0.0, 1.0, clamp((1.00 - co.z) / 0.36)))


def body_r(z):
	for (r0, z0), (r1, z1) in zip(PROFILE, PROFILE[1:]):
		if z <= z1:
			t = clamp((z - z0) / (z1 - z0))
			return r0 + (r1 - r0) * t
	return PROFILE[-1][0]


def front(x, z, out=0.0):
	# A point on the coat's front surface (plus out), for pockets and buttons.
	r = body_r(z)
	return (x, -(math.sqrt(max(r * r - x * x, 0.0)) * DEPTH + out), z)


def edit_verts(obj, fn):
	geo.apply_transform(obj)
	for v in obj.data.vertices:
		v.co = fn(v.co.copy())
	obj.data.update()
	return obj


def build():
	coat = mat.make("husk_coat", (0.37, 0.34, 0.26), pattern="cloth", rough=0.93, bump=0.5, var=0.16)
	patch = mat.make("husk_patch", (0.29, 0.26, 0.21), pattern="cloth", rough=0.93, bump=0.5)
	void = mat.make("husk_void", (0.02, 0.025, 0.03), pattern="plain", rough=1.0, var=0.0, grime=0.0, edge=0.0, bump=0.0)
	trousers = mat.make("husk_trousers", (0.22, 0.22, 0.22), pattern="cloth", rough=0.9, bump=0.45)
	shoe = mat.make("husk_shoe", (0.15, 0.12, 0.10), pattern="plain", rough=0.6, bump=0.3, edge=0.25)
	button = mat.make("husk_button", (0.13, 0.11, 0.09), pattern="plain", rough=0.5, bump=0.1)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	glow = mat.get("spore_glow")

	parts = []

	def part(obj, *tags):
		geo.tag(obj, *tags)
		parts.append(obj)
		return obj

	# The coat: a long lathe hanging from narrow shoulders, flattened front to back, hem torn.
	body = geo.lathe(PROFILE, mat=coat, segments=24, name="coat")
	body.scale = (1.0, DEPTH, 1.0)
	def tear(co):
		if co.z < 0.62:
			a = math.atan2(co.y, co.x)
			co.z -= 0.035 * (0.5 + 0.5 * math.sin(a * 5.0 + 1.3)) + 0.02 * max(0.0, math.sin(a))
		return co
	edit_verts(body, tear)
	weigh(body, coat_weights)
	parts.append(body)

	# Shoulder pads let the shoulders shrug; a placket and odd buttons run down the front.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.blob((0.07, 0.065, 0.042), (0.135 * sx, 0.0, 1.45), (0, 18 * sx, 0), mat=coat, name="pad_" + side), "shoulder_" + side)
	placket = [front(0.0, z, 0.002) for z in (1.47, 1.20, 0.90, 0.64)]
	parts.append(weigh(geo.tube(placket, 0.014, mat=patch, segments=6, name="placket"), coat_weights))
	for z in (1.30, 1.15, 1.00):
		parts.append(weigh(geo.sphere(0.014, front(0.016, z, 0.012), mat=button, segments=8, rings=6, name="button"), coat_weights))
	part(geo.box((0.09, 0.026, 0.11), front(0.085, 0.87, 0.0), (0, 0, 14), mat=patch, bevel=0.01, name="pocket_L"), "hips")
	part(geo.box((0.09, 0.026, 0.09), front(-0.085, 0.86, 0.0), (0, 0, -22), mat=patch, bevel=0.01, name="pocket_R"), "hips")
	part(geo.box((0.065, 0.024, 0.07), front(0.075, 1.30, 0.0), (0, 0, 22), mat=patch, bevel=0.008, name="chest_pocket"), "chest")
	# Tatters hanging below the torn hem at the back.
	for (a, ln) in ((115.0, 0.13), (150.0, 0.10), (60.0, 0.09)):
		r = math.radians(a)
		x, y = math.cos(r) * 0.18, math.sin(r) * 0.18 * DEPTH
		t = geo.box((0.06, 0.012, ln), (x, y, 0.57 - ln * 0.5), (0, 0, a + 90.0), mat=coat, bevel=0.004, name="tatter")
		weigh(t, lambda co: skirt_share(co, 1.0))
		parts.append(t)

	# The collar: a stiff cup open at the top and dipping at the front, lined with darkness.
	cup = geo.lathe([(0.115, 1.46), (0.118, 1.62), (0.110, 1.72), (0.090, 1.725), (0.088, 1.48)], mat=coat, segments=20, name="collar")
	lining = geo.lathe([(0.091, 1.49), (0.091, 1.70), (0.077, 1.70), (0.077, 1.49)], mat=void, segments=20, name="lining")
	plug = geo.cylinder(0.085, 0.10, (0, 0, 1.51), mat=void, segments=16, name="plug")
	def dip(co):
		n = math.hypot(co.x, co.y)
		front = max(0.0, -co.y / n) if n > 1e-5 else 0.0
		if co.z > 1.585:
			co.z = 1.585 + (co.z - 1.585) * (1.0 - 0.9 * front * front)
		co.y *= 0.85
		return co
	for o in (cup, lining, plug):
		edit_verts(o, dip)
		part(o, "collar")
	# Two spore points deep inside, where eyes would be.
	for x in (-0.03, 0.03):
		part(geo.sphere(0.019, (x, -0.006, 1.572), mat=glow, segments=10, rings=8, name="eye"), "collar", "pulse")

	# Moss on the shoulders and spilling out over the collar rim down the right shoulder.
	part(geo.blob((0.10, 0.08, 0.042), (0.095, 0.01, 1.49), (0, -24, 0), mat=moss, lumpy=0.3, seed=1, name="moss_sh_L"), "chest", "pulse")
	part(geo.blob((0.065, 0.055, 0.033), (-0.11, 0.03, 1.475), (0, 22, 0), mat=moss_dark, lumpy=0.3, seed=2, name="moss_sh_R"), "chest", "pulse")
	part(geo.blob((0.09, 0.04, 0.10), (0.02, 0.10, 1.39), (8, 0, 0), mat=moss_dark, lumpy=0.35, seed=3, name="moss_back"), "chest", "pulse")
	spill = [((-0.075, 0.02, 1.70), 0.042, "collar"), ((-0.105, -0.01, 1.63), 0.045, "collar"),
			 ((-0.125, -0.05, 1.55), 0.04, "chest"), ((-0.115, -0.10, 1.47), 0.032, "chest")]
	for i, (c, r, bone) in enumerate(spill):
		part(geo.blob((r, r * 0.8, r * 1.1), c, mat=moss, lumpy=0.35, seed=10 + i, name="moss_spill"), bone, "pulse")
	for c in ((0.125, 0.03, 1.52), (-0.09, 0.04, 1.73), (-0.14, -0.03, 1.56)):
		part(geo.sphere(0.014, c, mat=glow, segments=8, rings=6, name="spore"), "collar" if c[2] > 1.6 else "chest", "pulse")

	# Sleeves a little too long, cuffs empty: lichen strands hang where hands should be.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		cx, cy, cz = CUFF[0] * sx, CUFF[1], CUFF[2]
		part(geo.tube([(0.15 * sx, 0, 1.465), (0.245 * sx, 0.01, 1.17)], [0.06, 0.054], mat=coat, segments=10, name="upper_" + side), "upperarm_" + side)
		part(geo.tube([(0.245 * sx, 0.01, 1.17), (cx, cy, cz + 0.01)], [0.054, 0.062], mat=coat, segments=10, name="fore_" + side), "forearm_" + side)
		part(geo.cylinder(0.068, 0.05, (cx, cy + 0.002, cz + 0.03), (6, 0, 0), mat=patch, segments=12, name="cuff_" + side), "forearm_" + side)
		part(geo.cylinder(0.048, 0.02, (cx, cy, cz - 0.002), mat=void, segments=10, name="cuff_hole_" + side), "forearm_" + side)
		part(geo.blob((0.058, 0.058, 0.02), (cx, cy, cz - 0.002), mat=moss, lumpy=0.4, seed=30 + int(sx), name="cuff_moss"), "forearm_" + side, "pulse")
		part(geo.sphere(0.012, (cx - 0.03 * sx, cy - 0.035, cz + 0.004), mat=glow, segments=8, rings=6, name="cuff_spore"), "forearm_" + side, "pulse")
		# Three strands of very different lengths, so nothing reads as fingers.
		strands = ((30.0, 0.036, 0.27, 0.024), (160.0, 0.03, 0.07, 0.01), (250.0, 0.04, 0.15, 0.03))
		for k, (a, r, ln, wob) in enumerate(strands):
			ra = math.radians(a + 25.0 * sx)
			ox, oy = math.cos(ra) * r * sx, math.sin(ra) * r
			top = (cx + ox, cy + oy, cz - 0.01)
			pts = [top]
			for j in range(1, 5):
				f = j / 4.0
				w = wob * math.sin(f * 5.0 + k * 2.0)
				pts.append((top[0] + ox * 0.3 * f + w * sx, top[1] + oy * 0.3 * f + w * 0.6, top[2] - ln * f))
			part(geo.tube(pts, [0.018, 0.016, 0.014, 0.012, 0.011], mat=moss_dark, segments=6, name="strand"), "hand_" + side)
			if k == 0:
				e = pts[-1]
				part(geo.sphere(0.017, (e[0], e[1], e[2] - 0.008), mat=glow, segments=8, rings=6, name="strand_spore"), "hand_" + side, "pulse")

	# Trouser legs hanging empty into old shoes.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.095 * sx, 0, 0.96), (0.10 * sx, 0, 0.53)], [0.075, 0.066], mat=trousers, segments=10, name="thigh_" + side), "thigh_" + side)
		part(geo.tube([(0.10 * sx, 0, 0.53), (0.10 * sx, 0.015, 0.14)], [0.066, 0.068], mat=trousers, segments=10, name="shin_" + side), "shin_" + side)
		part(geo.blob((0.074, 0.074, 0.035), (0.10 * sx, 0.012, 0.15), mat=trousers, lumpy=0.3, seed=20 + int(sx), name="bunch_" + side), "shin_" + side)
		part(geo.box((0.05, 0.03, 0.06), (0.09 * sx, -0.06, 0.62), (0, 0, 8 * sx), mat=patch, bevel=0.01, name="knee_patch"), "thigh_" + side)
		part(geo.blob((0.06, 0.07, 0.05), (0.10 * sx, 0.04, 0.055), mat=shoe, name="heel_" + side), "shin_" + side)
		part(geo.blob((0.058, 0.10, 0.042), (0.10 * sx, -0.06, 0.042), (6, 0, 0), mat=shoe, name="toe_" + side), "foot_" + side)

	res = pipeline.organic("husk", parts, voxel=0.0095, tris=4800, size=512, smooth=2)
	low = res["mesh"]

	arm = rig.armature("husk_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("idle", 45), ("walk", 0), ("walk", 5), ("walk", 10), ("windup", 21),
							 ("strike", 6), ("recover", 10), ("die", 8), ("die", 16), ("die", 27), ("recover", 30)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 2.4
	return res


def add(*poses):
	out = {}
	for p in poses:
		for b, r in p.items():
			o = out.get(b, (0.0, 0.0, 0.0))
			out[b] = (o[0] + r[0], o[1] + r[1], o[2] + r[2])
	return out


def animations(arm):
	clips = []
	m = rig.mirror

	# Waiting in a queue that never moves: bowed, collar cocked too far, one shoulder dropped.
	stand = {"spine": (4, 0, 0), "chest": (7, -3, 3), "collar": (14, 13, -8),
			 "shoulder_L": (0, 7, 0), "shoulder_R": (0, 3, 0),
			 "upperarm_L": (-9, -2, 0), "upperarm_R": (-8, 3, 0), "forearm_L": (-6, 0, 0), "forearm_R": (-5, 0, 0),
			 "hand_L": (4, 0, 0), "hand_R": (4, 0, 0)}
	sway_l = add(stand, {"hips": (0, 2, 2), "thigh_L": (0, -2, 0), "thigh_R": (0, -2, 0), "spine": (0, -2, 0), "collar": (2, 3, 0),
						 "hand_L": (3, -4, 0), "hand_R": (-3, -3, 0), "skirt_L": (0, 3, 0), "skirt_R": (0, 3, 0)})
	sway_r = add(stand, {"hips": (0, -2, -2), "thigh_L": (0, 2, 0), "thigh_R": (0, 2, 0), "spine": (0, 2, 0), "collar": (-2, -2, 6),
						 "hand_L": (-3, 4, 0), "hand_R": (3, 3, 0), "skirt_L": (0, -3, 0), "skirt_R": (0, -3, 0)})
	# Partway through, the collar turns to look down the road for the bus, then back.
	look = add(stand, {"collar": (-6, 2, 34), "chest": (0, 0, 6), "upperarm_L": (-3, 0, 0)})
	clips.append(rig.clip(arm, "idle", [(0, stand, (0, 0, 0)), (20, sway_l, (0.012, 0, -0.004)), (42, look, (0.0, 0, 0.0)),
										(52, look, (0.0, 0, 0.0)), (70, sway_r, (-0.012, 0, -0.004)), (90, stand, (0, 0, 0))]))

	# A shambling walk: short dragging steps, sleeves swinging late, collar lolling. 20 frames, two steps.
	contact = add(stand, {"thigh_L": (-24, 0, 0), "shin_L": (8, 0, 0), "foot_L": (-8, 0, 0),
						  "thigh_R": (18, 0, 0), "shin_R": (20, 0, 0), "foot_R": (14, 0, 0),
						  "hips": (0, 0, 5), "spine": (4, 3, 0), "chest": (2, 0, -6), "collar": (6, -4, 4),
						  "upperarm_L": (14, 0, 0), "upperarm_R": (-12, 0, 0), "forearm_L": (-4, 0, 0), "forearm_R": (-10, 0, 0),
						  "hand_L": (14, 0, 0), "hand_R": (-6, 0, 0),
						  "skirt_F": (-6, 0, 0), "skirt_B": (8, 0, 0)})
	passing = add(stand, {"thigh_L": (2, 0, 0), "shin_L": (6, 0, 0),
						  "thigh_R": (-14, 0, 0), "shin_R": (34, 0, 0), "foot_R": (16, 0, 0),
						  "spine": (6, -3, 0), "chest": (0, 0, 0), "collar": (12, 6, -4),
						  "upperarm_L": (2, 0, 0), "upperarm_R": (-2, 0, 0), "hand_L": (8, 0, 0), "hand_R": (8, 0, 0),
						  "skirt_B": (12, 0, 0)})
	contact_r = add(stand, m({"thigh_L": (-20, 0, 0), "shin_L": (6, 0, 0), "foot_L": (-6, 0, 0),
							  "thigh_R": (16, 0, 0), "shin_R": (14, 0, 0), "foot_R": (8, 0, 0),
							  "hips": (0, 0, 5), "spine": (4, 3, 0), "chest": (2, 0, -6), "collar": (4, -6, 2),
							  "upperarm_L": (12, 0, 0), "upperarm_R": (-12, 0, 0), "forearm_R": (-8, 0, 0),
							  "hand_L": (12, 0, 0), "hand_R": (-6, 0, 0),
							  "skirt_F": (-6, 0, 0), "skirt_B": (8, 0, 0)}))
	passing_r = add(stand, m({"thigh_L": (2, 0, 0), "shin_L": (6, 0, 0),
							  "thigh_R": (-12, 0, 0), "shin_R": (22, 0, 0), "foot_R": (12, 0, 0),
							  "spine": (6, -3, 0), "collar": (10, 4, -2),
							  "hand_L": (8, 0, 0), "hand_R": (8, 0, 0), "skirt_B": (12, 0, 0)}))
	clips.append(rig.clip(arm, "walk", [(0, contact, (0.015, 0, -0.034)), (5, passing, (0.02, 0, 0.015)),
										(10, contact_r, (-0.015, 0, -0.025)), (15, passing_r, (-0.02, 0, 0.03)),
										(20, contact, (0.015, 0, -0.034))]))

	# Windup: both sleeves rise overhead, the coat billows and the collar tips back to the sky.
	rise = add(stand, {"upperarm_L": (-80, -10, 0), "upperarm_R": (-80, 10, 0), "forearm_L": (-20, 0, 0), "forearm_R": (-20, 0, 0),
					   "hand_L": (60, 0, 0), "hand_R": (60, 0, 0),
					   "chest": (-10, 3, -3), "collar": (-16, -10, 8), "skirt_F": (-8, 0, 0), "skirt_B": (8, 0, 0)})
	overhead = {"spine": (-6, 0, 0), "chest": (-12, 0, 0), "collar": (-26, 0, 0),
				"shoulder_L": (0, -12, 0), "shoulder_R": (0, 12, 0),
				"upperarm_L": (-160, -18, 0), "upperarm_R": (-160, 18, 0), "forearm_L": (-18, 0, 0), "forearm_R": (-18, 0, 0),
				"hand_L": (140, 0, 0), "hand_R": (140, 0, 0),
				"skirt_F": (-42, 0, 0), "skirt_B": (46, 0, 0), "skirt_L": (0, -40, 0), "skirt_R": (0, 40, 0),
				"foot_L": (6, 0, 0), "foot_R": (6, 0, 0)}
	clips.append(rig.clip(arm, "windup", [(0, stand, (0, 0, 0)), (8, rise, (0, 0.01, 0.01)), (21, overhead, (0, 0.03, 0.012))], loop=False))

	# Strike: the sleeves slam down in front, the whole coat folding after them.
	swing = {"spine": (10, 0, 0), "chest": (18, 0, 0), "collar": (12, 0, 0),
			 "upperarm_L": (-122, -8, 0), "upperarm_R": (-122, 8, 0), "forearm_L": (-10, 0, 0), "forearm_R": (-10, 0, 0),
			 "hand_L": (85, 0, 0), "hand_R": (85, 0, 0),
			 "skirt_F": (8, 0, 0), "skirt_B": (20, 0, 0), "skirt_L": (0, -8, 0), "skirt_R": (0, 8, 0)}
	slam = {"spine": (16, 0, 0), "chest": (30, 0, 0), "collar": (26, 0, 0),
			"upperarm_L": (-92, -4, 0), "upperarm_R": (-92, 4, 0), "forearm_L": (-8, 0, 0), "forearm_R": (-8, 0, 0),
			"hand_L": (30, 0, 0), "hand_R": (30, 0, 0),
			"thigh_L": (-26, 0, 0), "shin_L": (34, 0, 0), "foot_L": (-8, 0, 0), "thigh_R": (-6, 0, 0), "shin_R": (22, 0, 0), "foot_R": (-16, 0, 0),
			"skirt_F": (10, 0, 0), "skirt_B": (30, 0, 0), "skirt_L": (0, -10, 0), "skirt_R": (0, 10, 0)}
	clips.append(rig.clip(arm, "strike", [(0, overhead, (0, 0.03, 0.012)), (3, swing, (0, -0.04, 0.0)), (6, slam, (0, -0.08, -0.02)),
										  (9, slam, (0, -0.08, -0.025))], loop=False))

	# Recover: the empty coat slumps as if nothing holds it, then something pulls it upright again.
	slump = {"spine": (22, 0, 4), "chest": (34, -6, 0), "collar": (48, 14, 0),
			 "upperarm_L": (-50, 2, 0), "upperarm_R": (-52, -2, 0), "forearm_L": (-4, 0, 0), "forearm_R": (-4, 0, 0),
			 "hand_L": (0, 0, 0), "hand_R": (0, 0, 0),
			 "thigh_L": (-22, 0, 0), "shin_L": (34, 0, 0), "foot_L": (-10, 0, 0), "thigh_R": (-14, 0, 0), "shin_R": (30, 0, 0), "foot_R": (-14, 0, 0),
			 "skirt_B": (8, 0, 0)}
	jerk = add(stand, {"spine": (6, 0, 0), "chest": (10, 0, 0), "collar": (-10, -8, 0), "upperarm_L": (-6, 0, 0), "upperarm_R": (-6, 0, 0),
					   "hand_L": (12, 0, 0), "hand_R": (12, 0, 0)})
	clips.append(rig.clip(arm, "recover", [(0, slam, (0, -0.08, -0.025)), (11, slump, (0, -0.06, -0.035)), (17, slump, (0, -0.05, -0.03)),
										   (23, jerk, (0, -0.01, 0.01)), (30, stand, (0, 0, 0))], loop=False))

	# Die: whatever held the coat up lets go. It drops straight down and folds into a heap of empty cloth.
	jolt = add(stand, {"collar": (-30, -10, 0), "chest": (-6, 0, 0), "upperarm_L": (-10, -20, 0), "upperarm_R": (-10, 20, 0),
					   "skirt_L": (0, -10, 0), "skirt_R": (0, 10, 0)})
	buckle = {"spine": (14, 0, 0), "chest": (16, 0, 0), "collar": (36, 10, 0),
			  "upperarm_L": (-20, -8, 0), "upperarm_R": (-20, 8, 0), "hand_L": (-10, 0, 0), "hand_R": (-10, 0, 0),
			  "thigh_L": (-55, -10, 0), "shin_L": (100, 0, 0), "foot_L": (-40, 0, 0),
			  "thigh_R": (-50, 10, 0), "shin_R": (100, 0, 0), "foot_R": (-40, 0, 0),
			  "skirt_F": (-20, 0, 0), "skirt_B": (20, 0, 0), "skirt_L": (0, -15, 0), "skirt_R": (0, 15, 0)}
	fall = {"spine": (40, 0, 6), "chest": (20, 0, 0), "collar": (40, 18, 0),
			"upperarm_L": (-10, -15, 0), "upperarm_R": (-10, 15, 0), "forearm_L": (-20, 0, 0), "forearm_R": (-20, 0, 0),
			"thigh_L": (-85, -18, 0), "shin_L": (140, 0, 0), "foot_L": (-20, 0, 0),
			"thigh_R": (-82, 16, 0), "shin_R": (140, 0, 0), "foot_R": (-20, 0, 0),
			"skirt_F": (-45, 0, 0), "skirt_B": (50, 0, 0), "skirt_L": (0, -45, 0), "skirt_R": (0, 45, 0)}
	heap = {"spine": (72, 0, 10), "chest": (22, 0, -8), "collar": (55, 28, 0),
			"shoulder_L": (0, 10, 0), "shoulder_R": (0, -10, 0),
			"upperarm_L": (-22, -35, 0), "upperarm_R": (-20, 38, 0), "forearm_L": (0, 105, 0), "forearm_R": (0, -95, 0),
			"hand_L": (0, 30, 0), "hand_R": (0, -30, 0),
			"thigh_L": (-95, -22, 0), "shin_L": (168, 0, 0), "foot_L": (0, 0, 0),
			"thigh_R": (-92, 20, 0), "shin_R": (168, 0, 0), "foot_R": (0, 0, 0),
			"skirt_F": (-80, 0, 0), "skirt_B": (82, 0, 0), "skirt_L": (0, -80, 0), "skirt_R": (0, 80, 0)}
	die = rig.clip(arm, "die", [(0, stand, (0, 0, 0)), (4, jolt, (0, 0, 0.03)), (11, buckle, (0, 0.02, -0.305)),
								(18, fall, (0, 0.04, -0.55)), (27, heap, (0, 0.05, -0.735))], loop=False)
	# The last clip exported leaves its pose on the mesh, and the preview frames itself on that: end standing.
	clips.insert(len(clips) - 1, die)
	return clips
