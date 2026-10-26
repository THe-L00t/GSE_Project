# Yeoul, the only child of the ginkgo town, born beside sleeping parents and grown up in the spores, which
# never make her drowsy: a big head with a dark bob and a wreath of leaves and small cream flowers, a teal-green
# hooded raincoat (hood down) over a cream shirt, brown shorts, mustard rubber boots, and a little wicker
# basket of herbs in her left hand. A few spores float round her and glow. Rigged: idle (looking about), talk
# (pointing things out).
import math

from mathutils import Vector

from gse import geo, mat, pipeline, rig
from gse.pose import Poser, report_weights, sided

BONES = [
	("root", (0, 0, 0), (0, 0, 0.15), None),
	("hips", (0, 0.0, 0.62), (0, 0.0, 0.70), "root"),
	("spine", (0, 0.0, 0.70), (0, 0.0, 0.80), "hips"),
	("chest", (0, 0.0, 0.80), (0, -0.005, 0.93), "spine"),
	("neck", (0, -0.005, 0.93), (0, -0.01, 0.99), "chest"),
	("head", (0, -0.01, 0.99), (0, -0.01, 1.22), "neck"),
] + sided([
	("shoulder", (0.03, 0.0, 0.895), (0.12, 0.005, 0.90), "chest"),
	("upperarm", (0.12, 0.005, 0.90), (0.17, 0.01, 0.73), "shoulder"),
	("forearm", (0.17, 0.01, 0.73), (0.205, -0.01, 0.585), "upperarm"),
	("hand", (0.205, -0.01, 0.585), (0.21, -0.015, 0.52), "forearm"),
]) + sided([
	("thigh", (0.065, 0.0, 0.62), (0.065, 0.0, 0.34), "hips"),
	("shin", (0.065, 0.0, 0.34), (0.065, 0.01, 0.065), "thigh"),
	("foot", (0.065, 0.01, 0.065), (0.065, -0.09, 0.025), "shin"),
])
DEFORM = [b[0] for b in BONES if b[0] != "root"]
P = Poser(BONES)

HEAD_C = Vector((0.0, -0.025, 1.10))


def H(x, y, z):
	return tuple(HEAD_C + Vector((x, y, z)))


def materials():
	return dict(
		skin=mat.get("skin"),
		hair=mat.make("hair_yeoul", (0.13, 0.11, 0.10), pattern="hair", rough=0.7, bump=0.3),
		eye=mat.make("eye", (0.06, 0.05, 0.05), pattern="plain", rough=0.3, grime=0.0, edge=0.0, bump=0.0),
		blush=mat.make("blush_child", (0.72, 0.48, 0.43), pattern="skin", rough=0.75, bump=0.1, grime=0.1, edge=0.0),
		lip=mat.make("lip_child", (0.64, 0.42, 0.39), pattern="skin", rough=0.7, bump=0.05, edge=0.0),
		wreath=mat.make("wreath_leaf", (0.30, 0.45, 0.23), pattern="leaf", rough=0.8, bump=0.4),
		wreath_dark=mat.make("wreath_leaf_dark", (0.20, 0.34, 0.19), pattern="leaf", rough=0.8, bump=0.4),
		flower=mat.make("flower_cream", (0.87, 0.83, 0.69), pattern="plain", rough=0.6, bump=0.05, grime=0.1),
		coat=mat.make("raincoat", (0.30, 0.45, 0.40), pattern="cloth", rough=0.75, bump=0.3),
		coat_dark=mat.make("raincoat_dark", (0.22, 0.35, 0.31), pattern="cloth", rough=0.78, bump=0.3),
		shirt=mat.make("shirt_cream", (0.78, 0.74, 0.64), pattern="cloth", rough=0.9, bump=0.3),
		toggle=mat.get("wood"),
		shorts=mat.make("shorts_brown", (0.37, 0.28, 0.20), pattern="cloth", rough=0.9, bump=0.4),
		boots=mat.make("rain_boots", (0.72, 0.57, 0.24), pattern="plain", rough=0.45, bump=0.1, grime=0.45, edge=0.2),
		boot_dark=mat.make("rain_boots_dark", (0.46, 0.35, 0.16), pattern="plain", rough=0.6, bump=0.1, grime=0.4),
		wicker=mat.make("wicker", (0.57, 0.45, 0.28), pattern="hair", rough=0.9, bump=0.6),
		herb=mat.get("leaf"),
		herb_dark=mat.get("leaf_dark"),
		glow=mat.get("spore_glow"),
	)


def zones(obj):
	geo.tag_fn(obj, "hips", lambda co, n: 1.0 if co.z < 0.70 else 0.0)
	geo.tag_fn(obj, "spine", lambda co, n: 1.0 if 0.70 <= co.z < 0.80 else 0.0)
	geo.tag_fn(obj, "chest", lambda co, n: 1.0 if co.z >= 0.80 else 0.0)
	return obj


def ring(c, rx, ry, n=16, dz=0.0):
	return [(c[0] + rx * math.cos(i / float(n) * math.tau), c[1] + ry * math.sin(i / float(n) * math.tau),
			 c[2] + dz * math.sin(i / float(n) * math.tau)) for i in range(n + 1)]


def build():
	M = materials()
	parts = []

	def part(obj, *tags):
		geo.tag(obj, *tags)
		parts.append(obj)
		return obj

	# Head: big and round, a dark bob framing the face with straight bangs, big eyes, pink cheeks.
	part(geo.sphere((0.115, 0.11, 0.118), H(0, 0, 0), mat=M["skin"], segments=24, rings=16, name="head"), "head")
	part(geo.blob((0.126, 0.122, 0.105), H(0, 0.025, 0.035), mat=M["hair"], lumpy=0.015, seed=3, name="hair"), "head")
	part(geo.blob((0.11, 0.06, 0.08), H(0, 0.075, -0.035), mat=M["hair"], lumpy=0.02, seed=5, name="hair_back"), "head")
	part(geo.blob((0.1, 0.04, 0.052), H(0, -0.084, 0.083), mat=M["hair"], lumpy=0.02, seed=7, name="bangs"), "head")
	for sx in (1.0, -1.0):
		part(geo.blob((0.035, 0.075, 0.075), H(0.1 * sx, 0.02, -0.04), mat=M["hair"], lumpy=0.02, seed=8, name="bob"), "head")
		part(geo.sphere((0.017, 0.009, 0.02), H(0.044 * sx, -0.104, -0.005), mat=M["eye"], segments=8, rings=6, name="eye"), "head")
		part(geo.sphere((0.026, 0.012, 0.018), H(0.064 * sx, -0.09, -0.035), mat=M["blush"], segments=8, rings=6, name="cheek"), "head")
	part(geo.sphere((0.013, 0.014, 0.014), H(0, -0.112, -0.025), mat=M["skin"], segments=8, rings=6, name="nose"), "head")
	part(geo.blob((0.018, 0.006, 0.005), H(0, -0.1, -0.06), mat=M["lip"], name="mouth"), "head")

	# A wreath of leaves with small cream flowers, and one spore bud glowing in it.
	wc = H(0, 0.0, 0.085)
	part(geo.tube(ring(wc, 0.122, 0.118, 20, 0.02), 0.013, mat=M["wreath_dark"], segments=6, name="wreath"), "head")
	for k in range(10):
		a = k / 10.0 * math.tau + 0.15
		p = (wc[0] + 0.124 * math.cos(a), wc[1] + 0.12 * math.sin(a), wc[2] + 0.02 * math.sin(a) + 0.008)
		part(geo.blob((0.032, 0.014, 0.016), p, (0, 0, math.degrees(a) + 90.0), mat=M["wreath"], name="leaf"), "head")
	for a in (-2.2, -1.75, -1.0, -0.5):
		p = (wc[0] + 0.128 * math.cos(a), wc[1] + 0.124 * math.sin(a), wc[2] + 0.02 * math.sin(a) + 0.014)
		part(geo.sphere(0.017, p, mat=M["flower"], segments=8, rings=6, name="flower"), "head")
	bud = (wc[0] + 0.13 * math.cos(-0.15), wc[1] + 0.126 * math.sin(-0.15), wc[2] + 0.014)
	part(geo.sphere(0.016, bud, mat=M["glow"], segments=8, rings=6, name="bud"), "head", "pulse")
	part(geo.tube([(0, -0.005, 0.92), (0, -0.015, 1.02)], 0.04, mat=M["skin"], segments=12, name="neck"), "neck")

	# The raincoat: a little A-line, hood lying on her back, collar, wooden toggles, a pouch pocket.
	body = geo.tube([(0, 0.005, 0.55), (0, 0.005, 0.66), (0, 0.0, 0.79), (0, -0.005, 0.88), (0, -0.008, 0.945)],
					[0.135, 0.125, 0.115, 0.11, 0.055], mat=M["coat"], segments=22, name="body")
	body.scale = (1.1, 0.9, 1.0)
	geo.apply_transform(body)
	parts.append(zones(body))
	part(geo.blob((0.105, 0.065, 0.07), (0, 0.105, 0.915), (14, 0, 0), mat=M["coat"], lumpy=0.05, seed=2, name="hood"), "chest")
	part(geo.tube(ring((0, 0.098, 0.925), 0.07, 0.04, 14), 0.012, mat=M["coat_dark"], segments=6, name="hood_rim"), "chest")
	part(geo.tube(ring((0, -0.006, 0.928), 0.072, 0.066, 16), 0.017, mat=M["coat_dark"], segments=6, name="collar"), "chest")
	for sx in (1.0, -1.0):
		part(geo.blob((0.055, 0.05, 0.048), (0.095 * sx, 0.0, 0.885), mat=M["coat"], name="shoulder"), "shoulder_" + ("L" if sx > 0 else "R"))
		part(geo.blob((0.024, 0.01, 0.018), (0.024 * sx, -0.07, 0.945), (0, 0, 22 * sx), mat=M["shirt"], name="shirt_collar"), "chest")
	for z, y in ((0.86, -0.106), (0.78, -0.108), (0.70, -0.114)):
		part(geo.cylinder(0.009, 0.04, (0.0, y, z), (0, 90, 0), mat=M["toggle"], segments=6, name="toggle"), "chest" if z >= 0.80 else "spine")
	part(geo.box((0.12, 0.025, 0.07), (0.0, -0.118, 0.635), mat=M["coat_dark"], bevel=0.008, name="pocket"), "hips")
	part(geo.tube(ring((0, 0.005, 0.556), 0.148, 0.122, 18), 0.011, mat=M["coat_dark"], segments=6, name="hem"), "hips")

	# Sleeves with the shirt cuffs showing, small hands.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.12 * sx, 0.005, 0.915), (0.17 * sx, 0.01, 0.73)], [0.046, 0.041], mat=M["coat"], segments=12, name="sleeve_up"), "upperarm_" + side)
		part(geo.tube([(0.17 * sx, 0.01, 0.73), (0.2 * sx, -0.007, 0.612)], [0.041, 0.037], mat=M["coat"], segments=12, name="sleeve_fore"), "forearm_" + side)
		part(geo.tube([(0.199 * sx, -0.006, 0.622), (0.204 * sx, -0.01, 0.588)], 0.034, mat=M["shirt"], segments=12, name="cuff"), "forearm_" + side)
		part(geo.blob((0.028, 0.024, 0.036), (0.21 * sx, -0.014, 0.55), mat=M["skin"], name="hand"), "hand_" + side)
		part(geo.blob((0.011, 0.011, 0.018), (0.197 * sx, -0.036, 0.562), (14, 0, 0), mat=M["skin"], name="thumb"), "hand_" + side)

	# Brown shorts, bare knees, mustard rubber boots.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.065 * sx, 0.0, 0.64), (0.065 * sx, 0.0, 0.40)], [0.062, 0.056], mat=M["shorts"], segments=14, name="shorts"), "thigh_" + side)
		part(geo.tube([(0.065 * sx, 0.0, 0.41), (0.065 * sx, 0.004, 0.29)], 0.04, mat=M["skin"], segments=12, name="knee"), "shin_" + side)
		part(geo.tube([(0.065 * sx, 0.006, 0.30), (0.065 * sx, 0.01, 0.06)], [0.05, 0.045], mat=M["boots"], segments=14, name="boot_shaft"), "shin_" + side)
		part(geo.tube(ring((0.065 * sx, 0.006, 0.298), 0.05, 0.05, 14), 0.009, mat=M["boot_dark"], segments=5, name="boot_rim"), "shin_" + side)
		part(geo.blob((0.046, 0.085, 0.036), (0.065 * sx, -0.025, 0.036), mat=M["boots"], name="boot"), "foot_" + side)
		part(geo.box((0.08, 0.16, 0.018), (0.065 * sx, -0.025, 0.009), mat=M["boot_dark"], bevel=0.008, name="sole"), "foot_" + side)

	# The herb basket in her left hand.
	b = Vector((0.225, -0.014, 0.37))
	part(geo.lathe([(0.055, 0.0), (0.07, 0.02), (0.078, 0.06), (0.08, 0.095)], tuple(b), mat=M["wicker"], segments=14, name="basket"), "hand_L")
	handle = [tuple(b + Vector((0.0, 0.075 * math.cos(t), 0.085 + 0.1 * math.sin(t)))) for t in (0.0, 0.5, 1.0, 1.5708, 2.14, 2.64, 3.1416)]
	part(geo.tube(handle, 0.0095, mat=M["wicker"], segments=5, name="handle"), "hand_L")
	part(geo.blob((0.035, 0.03, 0.025), tuple(b + Vector((-0.02, -0.016, 0.1))), mat=M["herb"], lumpy=0.2, seed=11, name="herb"), "hand_L")
	part(geo.blob((0.03, 0.03, 0.03), tuple(b + Vector((0.02, 0.024, 0.105))), mat=M["herb_dark"], lumpy=0.2, seed=12, name="herb"), "hand_L")
	part(geo.sphere(0.015, tuple(b + Vector((0.005, -0.036, 0.11))), mat=M["flower"], segments=8, rings=6, name="herb_flower"), "hand_L")
	part(geo.tube([tuple(b + Vector((0.015, 0.03, 0.09))), tuple(b + Vector((0.03, 0.045, 0.16)))], 0.008, mat=M["herb"], segments=5, name="stalk"), "hand_L")
	part(geo.sphere(0.014, tuple(b + Vector((0.032, 0.047, 0.168))), mat=M["glow"], segments=8, rings=6, name="stalk_bud"), "hand_L", "pulse")

	# Spores that drift round her and do not make her sleep.
	for p, bone in (((-0.24, -0.06, 1.02), "chest"), ((0.17, 0.03, 1.3), "head"), ((0.31, -0.07, 0.66), "hand_L")):
		part(geo.sphere(0.017, p, mat=M["glow"], segments=8, rings=6, name="spore"), bone, "pulse")

	res = pipeline.organic("yeoul", parts, voxel=0.0085, tris=5000, size=512, smooth=2)
	low = res["mesh"]

	arm = rig.armature("yeoul_rig", BONES)
	for bn in arm.data.bones:
		bn.use_deform = bn.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)
	report_weights("yeoul", low, DEFORM)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("idle", 50), ("idle", 72), ("talk", 10), ("talk", 30), ("talk", 42)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 1.6
	return res


# ---- poses ---------------------------------------------------------------------------------

def basket_level(pose, root=(0, 0, 0)):
	# Whatever her body does, the basket hangs straight from her hand.
	P.level(pose, "hand_L", root)
	return pose


def gesture(pose, wrist, hand_dir, root=(0, 0, 0)):
	P.reach(pose, "upperarm_R", "forearm_R", P.carried(pose, "chest", wrist, root), (-0.3, 0.5, -1.0), root)
	P.aim(pose, "hand_R", hand_dir, root)
	return pose


def animations(arm):
	clips = []
	still = (0, 0, 0)

	# Idle: quick small breaths, looking about, head tipped in curiosity, a glance up at the spores.
	def idle(breath, look, tilt, up):
		pose = {"spine": (1 - breath * 1.0, 0, 0), "chest": (2 - breath * 2.5, 0, 0), "neck": (0, 0, look * 8),
				"head": (-2 + breath * 1.5 - up * 14, tilt * 10, look * 12), "shoulder_L": (0, -breath * 2.5, 0),
				"shoulder_R": (0, breath * 2.5, 0), "upperarm_R": (-4 + breath * 3, 0, 0)}
		return basket_level(pose)
	idle_a = idle(0.0, 0.0, 0.0, 0.0)
	clips.append(rig.clip(arm, "idle", [(0, idle_a, still), (25, idle(1.0, 0.4, 0.0, 0.0), still), (50, idle(0.0, 0.6, 1.0, 0.0), still),
										(72, idle(1.0, -0.3, -0.3, 1.0), still), (88, idle(0.3, -0.1, 0.0, 0.4), still), (100, idle_a, still)]))

	# Talk: she points things out (that plant, the sky), nods, opens her hand, points again to the side.
	def talk(nod, which, tilt):
		pose = {"spine": (2, 0, -4), "chest": (3, 0, -4), "head": (-4 + nod, tilt * 9, 5)}
		if which == 1:
			gesture(pose, (-0.13, -0.29, 1.0), (0.0, -1.0, 0.35))
		elif which == 2:
			gesture(pose, (-0.09, -0.18, 0.79), (0.0, -0.5, 1.0))
		elif which == 3:
			gesture(pose, (-0.27, -0.2, 0.98), (-0.7, -0.7, 0.3))
		return basket_level(pose)
	talk_a = talk(0, 0, 0)
	clips.append(rig.clip(arm, "talk", [(0, talk_a, still), (10, talk(6, 1, 0.5), still), (20, talk(-2, 1, 0.0), still),
										(30, talk(7, 2, -0.6), still), (42, talk(0, 3, 0.4), still), (54, talk_a, still)]))
	return clips
