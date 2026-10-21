# Hayul: a neutral traveller in a patched work coat and green-teal scarf, carrying the pack grandmother
# packed. Rigged and animated; the pipe is held at the grip_R bone.
import math

from gse import geo, mat, pipeline, rig

BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("hips", (0, 0, 0.90), (0, 0, 1.02), "root"),
	("spine", (0, 0, 1.02), (0, 0, 1.20), "hips"),
	("chest", (0, 0, 1.20), (0, 0, 1.38), "spine"),
	("neck", (0, 0, 1.38), (0, 0, 1.47), "chest"),
	("head", (0, 0, 1.47), (0, 0, 1.78), "neck"),
	("shoulder_L", (0.06, 0, 1.34), (0.19, 0.0, 1.35), "chest"),
	("upperarm_L", (0.19, 0, 1.35), (0.26, 0.0, 1.10), "shoulder_L"),
	("forearm_L", (0.26, 0, 1.10), (0.29, -0.03, 0.87), "upperarm_L"),
	("hand_L", (0.29, -0.03, 0.87), (0.30, -0.05, 0.77), "forearm_L"),
	("shoulder_R", (-0.06, 0, 1.34), (-0.19, 0.0, 1.35), "chest"),
	("upperarm_R", (-0.19, 0, 1.35), (-0.26, 0.0, 1.10), "shoulder_R"),
	("forearm_R", (-0.26, 0, 1.10), (-0.29, -0.03, 0.87), "upperarm_R"),
	("hand_R", (-0.29, -0.03, 0.87), (-0.30, -0.05, 0.77), "forearm_R"),
	("grip_R", (-0.30, -0.06, 0.81), (-0.30, -0.30, 0.81), "hand_R"),
	("thigh_L", (0.10, 0, 0.90), (0.11, 0.0, 0.49), "hips"),
	("shin_L", (0.11, 0, 0.49), (0.11, 0.02, 0.09), "thigh_L"),
	("foot_L", (0.11, 0.02, 0.09), (0.11, -0.13, 0.03), "shin_L"),
	("thigh_R", (-0.10, 0, 0.90), (-0.11, 0.0, 0.49), "hips"),
	("shin_R", (-0.11, 0, 0.49), (-0.11, 0.02, 0.09), "thigh_R"),
	("foot_R", (-0.11, 0.02, 0.09), (-0.11, -0.13, 0.03), "shin_R"),
	("pack", (0, 0.20, 1.02), (0, 0.22, 1.30), "chest"),
	("scarf_tail", (0.07, 0.10, 1.38), (0.10, 0.20, 1.12), "chest"),
]
DEFORM = [b[0] for b in BONES if b[0] not in ("root", "grip_R")]


def body_bone(z):
	if z < 1.02:
		return "hips"
	if z < 1.20:
		return "spine"
	return "chest"


def build():
	coat = mat.get("coat")
	scarf = mat.get("scarf")
	trousers = mat.get("trousers")
	boots = mat.get("boots")
	skin = mat.get("skin")
	hair = mat.get("hair_dark")
	canvas = mat.get("canvas")
	leather = mat.get("wood_dark")
	patch = mat.make("coat_patch", (0.38, 0.33, 0.27), pattern="cloth", rough=0.92, bump=0.5)
	eye = mat.make("eye", (0.06, 0.05, 0.05), pattern="plain", rough=0.3, grime=0.0, edge=0.0, bump=0.0)
	blanket = mat.make("blanket", (0.45, 0.30, 0.26), pattern="knit", rough=0.95, bump=0.5)

	parts = []

	def part(obj, bone):
		geo.tag(obj, bone)
		parts.append(obj)
		return obj

	# Head: slightly large for the diorama read, hair cut short and a little untidy.
	part(geo.sphere((0.135, 0.13, 0.15), (0, -0.01, 1.60), mat=skin, segments=20, rings=14, name="head"), "head")
	part(geo.blob((0.148, 0.145, 0.12), (0, 0.012, 1.665), mat=hair, lumpy=0.12, seed=2, name="hair"), "head")
	part(geo.blob((0.12, 0.06, 0.05), (0.02, -0.10, 1.70), (0, 0, 8), mat=hair, lumpy=0.2, seed=5, name="fringe"), "head")
	part(geo.blob((0.06, 0.05, 0.08), (0, 0.12, 1.58), mat=hair, lumpy=0.15, seed=3, name="nape"), "head")
	for x in (-0.048, 0.048):
		part(geo.sphere((0.016, 0.01, 0.02), (x, -0.135, 1.605), mat=eye, segments=8, rings=6, name="eye"), "head")
		part(geo.sphere((0.022, 0.03, 0.035), (x * 2.85, 0.0, 1.595), mat=skin, segments=8, rings=6, name="ear"), "head")
	part(geo.sphere((0.02, 0.02, 0.025), (0, -0.145, 1.57), mat=skin, segments=8, rings=6, name="nose"), "head")
	part(geo.cylinder(0.055, 0.12, (0, 0, 1.44), mat=skin, segments=10, name="neck"), "neck")

	# Scarf wound twice round the neck, one end hanging down the back.
	ring = [(0.13 * math.cos(a / 10.0 * math.tau), 0.11 * math.sin(a / 10.0 * math.tau), 1.405 + 0.012 * (a % 2)) for a in range(11)]
	part(geo.tube(ring, 0.05, mat=scarf, segments=8, name="scarf_ring"), "neck")
	tail = geo.tube([(0.06, 0.10, 1.38), (0.08, 0.17, 1.28), (0.10, 0.19, 1.16)], [0.045, 0.042, 0.04], mat=scarf, segments=6, name="scarf_tail")
	tail.scale = (1.0, 0.45, 1.0)
	part(tail, "scarf_tail")

	# Work coat: a lathe from hem to collar, flattened front to back, with patched pockets.
	coat_body = geo.lathe([(0.205, 0.74), (0.215, 0.80), (0.20, 0.96), (0.195, 1.08), (0.205, 1.24), (0.19, 1.33), (0.12, 1.39), (0.07, 1.42)],
						  mat=coat, segments=20, name="coat")
	coat_body.scale = (1.0, 0.78, 1.0)
	geo.apply_transform(coat_body)
	geo.tag_fn(coat_body, "hips", lambda co, n: 1.0 if co.z < 1.02 else 0.0)
	geo.tag_fn(coat_body, "spine", lambda co, n: 1.0 if 1.02 <= co.z < 1.20 else 0.0)
	geo.tag_fn(coat_body, "chest", lambda co, n: 1.0 if co.z >= 1.20 else 0.0)
	parts.append(coat_body)
	part(geo.cylinder(0.20, 0.045, (0, 0, 0.98), mat=leather, segments=20, name="belt"), "hips").scale = (1.04, 0.82, 1.0)
	part(geo.box((0.10, 0.03, 0.10), (0.11, -0.155, 0.88), mat=patch, bevel=0.01, name="pocket_L"), "hips")
	part(geo.box((0.10, 0.03, 0.10), (-0.11, -0.155, 0.88), mat=patch, bevel=0.01, name="pocket_R"), "hips")
	part(geo.box((0.08, 0.03, 0.07), (0.08, -0.150, 1.22), (0, 0, 10), mat=patch, bevel=0.01, name="chest_patch"), "chest")
	part(geo.box((0.03, 0.02, 0.50), (0.0, -0.157, 1.08), mat=leather, name="placket"), "spine")

	# Sleeves and hands, each segment rigid to its bone.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.17 * sx, 0, 1.35), (0.26 * sx, 0, 1.10)], [0.072, 0.064], mat=coat, segments=10, name="upper_" + side), "upperarm_" + side)
		part(geo.tube([(0.26 * sx, 0, 1.10), (0.29 * sx, -0.03, 0.88)], [0.064, 0.058], mat=coat, segments=10, name="fore_" + side), "forearm_" + side)
		part(geo.cylinder(0.062, 0.04, (0.288 * sx, -0.028, 0.89), (8 * -sx, 0, 0), mat=patch, segments=10, name="cuff_" + side), "forearm_" + side)
		part(geo.blob((0.048, 0.04, 0.06), (0.30 * sx, -0.05, 0.82), mat=skin, name="hand_" + side), "hand_" + side)
		part(geo.blob((0.02, 0.018, 0.03), (0.275 * sx, -0.085, 0.84), mat=skin, name="thumb_" + side), "hand_" + side)

	# Trousers tucked into boots.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.10 * sx, 0, 0.84), (0.11 * sx, 0, 0.49)], [0.088, 0.072], mat=trousers, segments=10, name="thigh_" + side), "thigh_" + side)
		part(geo.tube([(0.11 * sx, 0, 0.49), (0.11 * sx, 0.015, 0.20)], [0.072, 0.064], mat=trousers, segments=10, name="shin_" + side), "shin_" + side)
		part(geo.box((0.06, 0.04, 0.05), (0.11 * sx, -0.07, 0.40), mat=patch, bevel=0.01, name="knee_patch_" + side), "shin_" + side)
		part(geo.cylinder(0.072, 0.18, (0.11 * sx, 0.01, 0.15), mat=boots, segments=12, name="boot_" + side), "shin_" + side)
		part(geo.blob((0.075, 0.13, 0.055), (0.11 * sx, -0.045, 0.05), mat=boots, name="toe_" + side), "foot_" + side)

	# The pack: canvas body, a flap, a rolled blanket on top and straps over the shoulders.
	part(geo.box((0.34, 0.20, 0.40), (0, 0.24, 1.10), mat=canvas, bevel=0.05, bevel_segments=3, name="pack_body"), "pack")
	part(geo.box((0.35, 0.10, 0.18), (0, 0.17, 1.24), (12, 0, 0), mat=canvas, bevel=0.03, name="pack_flap"), "pack")
	part(geo.box((0.20, 0.06, 0.14), (0, 0.355, 1.02), mat=patch, bevel=0.02, name="pack_pocket"), "pack")
	part(geo.cylinder(0.075, 0.42, (0, 0.24, 1.36), (0, 90, 0), mat=blanket, segments=12, name="bedroll"), "pack")
	for sx in (1.0, -1.0):
		part(geo.tube([(0.11 * sx, 0.14, 1.30), (0.12 * sx, 0.0, 1.38), (0.12 * sx, -0.16, 1.24), (0.12 * sx, -0.15, 1.06)],
					  0.016, mat=leather, segments=5, name="strap"), "chest")
	part(geo.cylinder(0.035, 0.07, (0.15, 0.30, 0.96), mat=mat.get("metal"), segments=10, name="cup"), "pack")

	for p in parts:
		geo.tag(p, "bob")

	res = pipeline.organic("player", parts, voxel=0.011, tris=5200, size=512, smooth=2)
	low = res["mesh"]

	arm = rig.armature("player_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("run", 0), ("run", 5), ("attack", 3), ("roll", 4), ("pickup", 12), ("eat", 12), ("lie", 20)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 2.3
	return res


def animations(arm):
	clips = []
	m = rig.mirror

	idle_a = {"chest": (2, 0, 0), "upperarm_L": (0, -3, 0), "upperarm_R": (0, 3, 0), "forearm_L": (-6, 0, 0), "forearm_R": (-6, 0, 0)}
	idle_b = {"chest": (-1, 0, 0), "head": (2, 0, 2), "upperarm_L": (0, -5, 0), "upperarm_R": (0, 5, 0), "forearm_L": (-9, 0, 0), "forearm_R": (-9, 0, 0), "scarf_tail": (6, 0, 0)}
	clips.append(rig.clip(arm, "idle", [(0, idle_a, (0, 0, 0)), (30, idle_b, (0, 0, -0.008)), (60, idle_a, (0, 0, 0))]))

	# A brisk jog: 20 frames carry two strides, matching 4.2 m/s.
	contact = {"thigh_L": (-38, 0, 0), "shin_L": (12, 0, 0), "foot_L": (-8, 0, 0),
			   "thigh_R": (28, 0, 0), "shin_R": (48, 0, 0), "foot_R": (20, 0, 0),
			   "upperarm_L": (30, -6, 0), "forearm_L": (-35, 0, 0), "upperarm_R": (-32, 6, 0), "forearm_R": (-55, 0, 0),
			   "spine": (8, 0, 6), "chest": (2, 0, -10), "head": (-4, 0, 4), "pack": (-4, 0, 0), "scarf_tail": (20, 0, 0)}
	passing = {"thigh_L": (-5, 0, 0), "shin_L": (15, 0, 0),
			   "thigh_R": (-25, 0, 0), "shin_R": (85, 0, 0), "foot_R": (10, 0, 0),
			   "upperarm_L": (5, -6, 0), "forearm_L": (-40, 0, 0), "upperarm_R": (-5, 6, 0), "forearm_R": (-45, 0, 0),
			   "spine": (9, 0, 0), "chest": (0, 0, 0), "pack": (6, 0, 0), "scarf_tail": (35, 0, 0)}
	clips.append(rig.clip(arm, "run", [(0, contact, (0, 0, -0.035)), (5, passing, (0, 0, 0.03)),
									   (10, m(contact), (0, 0, -0.035)), (15, m(passing), (0, 0, 0.03)), (20, contact, (0, 0, -0.035))]))

	# The roll: tuck, a full turn about the hips, and up again, in 0.28 s.
	tuck = {"spine": (40, 0, 0), "chest": (30, 0, 0), "head": (35, 0, 0), "thigh_L": (-110, 0, 0), "thigh_R": (-110, 0, 0),
			"shin_L": (130, 0, 0), "shin_R": (130, 0, 0), "upperarm_L": (-60, 0, 0), "upperarm_R": (-60, 0, 0),
			"forearm_L": (-80, 0, 0), "forearm_R": (-80, 0, 0)}
	def rolled(angle):
		p = dict(tuck)
		p["hips"] = (angle, 0, 0)
		return p
	clips.append(rig.clip(arm, "roll", [(0, {}, (0, 0, 0)), (2, rolled(90), (0, -0.1, -0.35)), (4, rolled(180), (0, -0.25, -0.30)),
										(6, rolled(270), (0, -0.35, -0.30)), (8, rolled(355), (0, -0.4, -0.12)), (9, {}, (0, 0, 0))], loop=False))

	# Swing: the pipe comes from over the right shoulder down across the front.
	wind = {"upperarm_R": (-150, 20, 0), "forearm_R": (-40, 0, 0), "chest": (-6, 0, 20), "spine": (0, 0, 8), "upperarm_L": (-20, 0, 0)}
	strike = {"upperarm_R": (-55, -10, 0), "forearm_R": (-10, 0, 0), "hand_R": (30, 0, 0), "chest": (14, 0, -25), "spine": (8, 0, -10),
			  "thigh_R": (-20, 0, 0), "shin_R": (20, 0, 0), "upperarm_L": (15, 0, 0)}
	follow = {"upperarm_R": (-15, -30, 0), "forearm_R": (-20, 0, 0), "chest": (18, 0, -30), "spine": (10, 0, -12), "head": (5, 0, 10)}
	clips.append(rig.clip(arm, "attack", [(0, idle_a, (0, 0, 0)), (2, wind, (0, 0, 0.01)), (4, strike, (0, 0, -0.04)),
										  (6, follow, (0, 0, -0.05)), (10, idle_a, (0, 0, 0))], loop=False))

	flinch = {"spine": (-12, 0, 6), "chest": (-10, 0, 0), "head": (-14, 0, -6), "upperarm_L": (-30, -25, 0), "upperarm_R": (-30, 25, 0),
			  "forearm_L": (-50, 0, 0), "forearm_R": (-50, 0, 0)}
	clips.append(rig.clip(arm, "hit", [(0, idle_a, (0, 0, 0)), (2, flinch, (0, 0.08, -0.02)), (9, idle_a, (0, 0, 0))], loop=False))

	crouch = {"spine": (35, 0, 0), "chest": (18, 0, 0), "head": (15, 0, 0), "thigh_L": (-75, 0, 0), "thigh_R": (-40, 0, 0),
			  "shin_L": (95, 0, 0), "shin_R": (80, 0, 0), "foot_L": (-20, 0, 0), "upperarm_R": (-55, 10, 0), "forearm_R": (-20, 0, 0),
			  "upperarm_L": (-10, -10, 0)}
	clips.append(rig.clip(arm, "pickup", [(0, idle_a, (0, 0, 0)), (8, crouch, (0, 0, -0.30)), (16, crouch, (0, 0, -0.30)), (26, idle_a, (0, 0, 0))], loop=False))

	bite = {"upperarm_R": (-60, 0, 25), "forearm_R": (-120, 0, 0), "head": (10, 0, 0), "upperarm_L": (-35, 0, -15), "forearm_L": (-70, 0, 0)}
	chew = dict(bite)
	chew["head"] = (2, 0, 0)
	clips.append(rig.clip(arm, "eat", [(0, idle_a, (0, 0, 0)), (6, bite, (0, 0, 0)), (12, chew, (0, 0, 0)), (18, bite, (0, 0, 0)),
									   (24, chew, (0, 0, 0)), (32, idle_a, (0, 0, 0))], loop=False))

	# Lying down on the back: for sleep and for being overcome.
	lie = {"hips": (-88, 0, 0), "head": (-10, 0, 0), "upperarm_L": (0, -20, 0), "upperarm_R": (0, 20, 0), "thigh_L": (0, -6, 0), "thigh_R": (0, 6, 0)}
	sink = {"spine": (20, 0, 0), "thigh_L": (-50, 0, 0), "thigh_R": (-50, 0, 0), "shin_L": (70, 0, 0), "shin_R": (70, 0, 0)}
	clips.append(rig.clip(arm, "lie", [(0, idle_a, (0, 0, 0)), (8, sink, (0, 0, -0.35)), (20, lie, (0, 0.55, -0.78))], loop=False))
	return clips
