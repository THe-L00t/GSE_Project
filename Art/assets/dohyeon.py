# Dohyeon, the collector of the ginkgo town: a man in his forties, a flat cap over short dark hair going grey
# at the temples, round glasses, stubble, a rust knit scarf, a long olive field coat to the knees with a belt
# and many pockets (a notebook and pencil in the chest one), and a leather satchel across his body with a
# rolled map poking out. Rigged: idle (a hand resting on the satchel), talk (gestures, a push of the glasses).
import math

from mathutils import Vector

from gse import geo, mat, pipeline, rig
from gse.pose import Poser, report_weights, sided

BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("hips", (0, 0.01, 0.92), (0, 0.01, 1.04), "root"),
	("spine", (0, 0.01, 1.04), (0, 0.0, 1.20), "hips"),
	("chest", (0, 0.0, 1.20), (0, -0.01, 1.40), "spine"),
	("neck", (0, -0.01, 1.40), (0, -0.02, 1.50), "chest"),
	("head", (0, -0.02, 1.50), (0, -0.02, 1.76), "neck"),
] + sided([
	("shoulder", (0.04, 0.0, 1.36), (0.19, 0.01, 1.37), "chest"),
	("upperarm", (0.19, 0.01, 1.37), (0.24, 0.02, 1.10), "shoulder"),
	("forearm", (0.24, 0.02, 1.10), (0.26, -0.02, 0.86), "upperarm"),
	("hand", (0.26, -0.02, 0.86), (0.265, -0.03, 0.76), "forearm"),
]) + sided([
	("thigh", (0.10, 0.01, 0.92), (0.10, 0.005, 0.50), "hips"),
	("shin", (0.10, 0.005, 0.50), (0.10, 0.015, 0.09), "thigh"),
	("foot", (0.10, 0.015, 0.09), (0.10, -0.13, 0.03), "shin"),
])
DEFORM = [b[0] for b in BONES if b[0] != "root"]
P = Poser(BONES)

HEAD_C = Vector((0.0, -0.035, 1.615))
GLASSES = (0.0, -0.17, 1.628)


def H(x, y, z):
	return tuple(HEAD_C + Vector((x, y, z)))


def materials():
	return dict(
		skin=mat.get("skin"),
		stubble=mat.make("stubble", (0.47, 0.39, 0.34), pattern="skin", rough=0.8, bump=0.2, grime=0.3, edge=0.0),
		lip=mat.make("lip", (0.50, 0.35, 0.31), pattern="skin", rough=0.7, bump=0.05, edge=0.0),
		hair=mat.make("hair_dohyeon", (0.15, 0.13, 0.12), pattern="hair", rough=0.7, bump=0.3),
		grey=mat.make("hair_greying", (0.48, 0.47, 0.45), pattern="hair", rough=0.7, bump=0.3),
		eye=mat.make("eye", (0.06, 0.05, 0.05), pattern="plain", rough=0.3, grime=0.0, edge=0.0, bump=0.0),
		frame=mat.make("glasses_frame", (0.12, 0.10, 0.09), pattern="plain", rough=0.35, grime=0.1, edge=0.15, bump=0.0),
		cap=mat.make("cap_tweed", (0.33, 0.30, 0.25), pattern="cloth", rough=0.92, bump=0.45),
		cap_band=mat.make("cap_band", (0.24, 0.21, 0.18), pattern="cloth", rough=0.9, bump=0.3),
		scarf=mat.make("scarf_rust", (0.52, 0.30, 0.22), pattern="knit", rough=0.95, bump=0.5),
		coat=mat.make("field_coat", (0.37, 0.36, 0.25), pattern="cloth", rough=0.9, bump=0.45),
		coat_dark=mat.make("field_coat_dark", (0.29, 0.28, 0.19), pattern="cloth", rough=0.9, bump=0.4),
		button=mat.make("button_horn", (0.22, 0.17, 0.13), pattern="plain", rough=0.45, bump=0.05),
		belt=mat.make("belt_leather", (0.25, 0.17, 0.11), pattern="plain", rough=0.6, bump=0.2, grime=0.4),
		buckle=mat.make("buckle", (0.45, 0.42, 0.36), pattern="rust", rough=0.5, bump=0.2, metallic=0.6),
		leather=mat.make("satchel_leather", (0.40, 0.27, 0.17), pattern="plain", rough=0.6, bump=0.25, grime=0.5, edge=0.3),
		strap=mat.make("satchel_strap", (0.31, 0.21, 0.14), pattern="plain", rough=0.65, bump=0.2, grime=0.4),
		paper=mat.get("paper"),
		pencil=mat.make("pencil", (0.70, 0.56, 0.26), pattern="plain", rough=0.5, bump=0.05),
		trousers=mat.make("trousers_dohyeon", (0.22, 0.22, 0.23), pattern="cloth", rough=0.9, bump=0.45),
		boots=mat.make("boots_dohyeon", (0.27, 0.20, 0.15), pattern="plain", rough=0.7, bump=0.3, grime=0.5, edge=0.25),
		sole=mat.make("sole", (0.12, 0.11, 0.10), pattern="plain", rough=0.9, bump=0.2),
	)


def zones(obj):
	# Body pieces that run up the trunk follow hips, spine or chest by height.
	geo.tag_fn(obj, "hips", lambda co, n: 1.0 if co.z < 1.04 else 0.0)
	geo.tag_fn(obj, "spine", lambda co, n: 1.0 if 1.04 <= co.z < 1.20 else 0.0)
	geo.tag_fn(obj, "chest", lambda co, n: 1.0 if co.z >= 1.20 else 0.0)
	return obj


def skirt_weights(obj):
	# The coat below the belt: held by the hips at the top, by each thigh lower down.
	def hips(co):
		return max(0.0, min(1.0, (co.z - 0.84) / 0.14))
	def left(co):
		return max(0.0, min(1.0, 0.5 + co.x / 0.14))
	geo.tag_fn(obj, "hips", lambda co, n: hips(co))
	geo.tag_fn(obj, "thigh_L", lambda co, n: (1.0 - hips(co)) * left(co))
	geo.tag_fn(obj, "thigh_R", lambda co, n: (1.0 - hips(co)) * (1.0 - left(co)))
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

	# Head: a long kind face, square stubbled jaw, short dark hair grey at the temples, a flat cap pulled forward.
	part(geo.sphere((0.112, 0.118, 0.13), H(0, 0, 0), mat=M["skin"], segments=24, rings=16, name="head"), "head")
	part(geo.blob((0.095, 0.075, 0.055), H(0, -0.035, -0.08), mat=M["stubble"], name="jaw"), "head")
	part(geo.blob((0.118, 0.115, 0.105), H(0, 0.035, 0.03), mat=M["hair"], lumpy=0.02, seed=4, name="hair"), "head")
	for sx in (1.0, -1.0):
		part(geo.blob((0.012, 0.035, 0.03), H(0.108 * sx, 0.005, 0.0), mat=M["grey"], name="temple"), "head")
		part(geo.sphere((0.02, 0.028, 0.04), H(0.113 * sx, 0.005, -0.005), (0, 0, -10 * sx), mat=M["skin"], segments=10, rings=8, name="ear"), "head")
		part(geo.sphere((0.013, 0.008, 0.01), H(0.042 * sx, -0.11, 0.01), mat=M["eye"], segments=8, rings=6, name="eye"), "head")
		part(geo.blob((0.024, 0.009, 0.007), H(0.043 * sx, -0.112, 0.042), (0, 8 * sx, 0), mat=M["hair"], name="brow"), "head")
	part(geo.blob((0.018, 0.026, 0.03), H(0, -0.124, -0.02), mat=M["skin"], name="nose"), "head")
	part(geo.blob((0.026, 0.008, 0.006), H(0, -0.112, -0.062), mat=M["lip"], name="mouth"), "head")
	part(geo.blob((0.13, 0.138, 0.052), H(0, 0.012, 0.118), (8, 0, 0), mat=M["cap"], lumpy=0.03, seed=2, name="cap"), "head")
	part(geo.tube(ring(H(0, 0.0, 0.088), 0.115, 0.122, 18), 0.012, mat=M["cap_band"], segments=6, name="cap_band"), "head")
	part(geo.blob((0.098, 0.062, 0.014), H(0, -0.125, 0.092), (-10, 0, 0), mat=M["cap"], name="brim"), "head")

	# Round glasses: two rims, a bridge, arms back to the ears.
	g = Vector(GLASSES)
	for sx in (1.0, -1.0):
		c = g + Vector((0.043 * sx, 0.0, 0.0))
		rim = [(c.x + 0.031 * math.cos(i / 14.0 * math.tau), c.y, c.z + 0.027 * math.sin(i / 14.0 * math.tau)) for i in range(15)]
		part(geo.tube(rim, 0.0085, mat=M["frame"], segments=5, name="rim"), "head")
		part(geo.tube([tuple(c + Vector((0.031 * sx, 0.0, 0.004))), H(0.108 * sx, -0.045, 0.012)], 0.0085, mat=M["frame"], segments=5, name="arm"), "head")
	part(geo.tube([tuple(g + Vector((-0.013, 0.0, 0.006))), tuple(g + Vector((0.0, -0.004, 0.01))), tuple(g + Vector((0.013, 0.0, 0.006)))], 0.0085,
				  mat=M["frame"], segments=5, name="bridge"), "head")
	part(geo.tube([(0, -0.015, 1.40), (0, -0.03, 1.52)], 0.052, mat=M["skin"], segments=12, name="neck"), "neck")

	# Knit scarf wound once round the neck, the end hanging down his front.
	part(geo.tube(ring((0, -0.015, 1.445), 0.08, 0.075, 16, 0.01), 0.032, mat=M["scarf"], segments=8, name="scarf"), "chest")
	part(geo.tube([(0.035, -0.09, 1.44), (0.05, -0.135, 1.36), (0.055, -0.15, 1.27)], [0.032, 0.03, 0.028], mat=M["scarf"], segments=8, name="scarf_end"), "chest")

	# The coat: trunk with a turned-down collar and lapels, chest pockets, a belt, then the long skirt to the knees.
	trunk = geo.tube([(0, 0.01, 0.92), (0, 0.015, 1.04), (0, 0.01, 1.18), (0, 0.0, 1.30), (0, -0.005, 1.38), (0, -0.01, 1.43)],
					 [0.165, 0.16, 0.165, 0.17, 0.14, 0.07], mat=M["coat"], segments=22, name="trunk")
	trunk.scale = (1.18, 0.82, 1.0)
	geo.apply_transform(trunk)
	parts.append(zones(trunk))
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.blob((0.08, 0.075, 0.07), (0.155 * sx, 0.0, 1.355), mat=M["coat"], name="shoulder"), "shoulder_" + side)
		part(geo.blob((0.05, 0.016, 0.1), (0.055 * sx, -0.128, 1.33), (0, 0, 22 * sx), mat=M["coat_dark"], name="lapel"), "chest")
		part(geo.box((0.075, 0.03, 0.085), (0.088 * sx, -0.118, 1.225), (0, 0, 18 * sx), mat=M["coat"], bevel=0.008, name="chest_pocket"), "chest")
		part(geo.box((0.082, 0.026, 0.032), (0.088 * sx, -0.124, 1.27), (0, 0, 18 * sx), mat=M["coat_dark"], bevel=0.006, name="pocket_flap"), "chest")
	part(geo.tube(ring((0, 0.0, 1.405), 0.105, 0.09, 16), 0.022, mat=M["coat_dark"], segments=6, name="collar"), "chest")
	part(geo.box((0.05, 0.016, 0.07), (0.098, -0.128, 1.305), (0, 0, 18), mat=M["paper"], name="notebook"), "chest")
	part(geo.cylinder(0.0085, 0.1, (0.067, -0.136, 1.315), (0, 6, 0), mat=M["pencil"], segments=6, name="pencil"), "chest")
	for z in (1.31, 1.20, 1.09):
		part(geo.sphere(0.013, (-0.03, -0.128 if z > 1.15 else -0.123, z), mat=M["button"], segments=8, rings=6, name="button"),
			 "chest" if z >= 1.20 else "spine")
	part(geo.tube(ring((0, 0.015, 1.0), 0.2, 0.144, 18), 0.017, mat=M["belt"], segments=6, name="belt"), "hips")
	part(geo.box((0.05, 0.014, 0.042), (-0.03, -0.132, 1.0), mat=M["buckle"], bevel=0.005, name="buckle"), "hips")
	skirt = geo.tube([(0, 0.015, 1.0), (0, 0.02, 0.75), (0, 0.025, 0.50)], [0.168, 0.186, 0.205], mat=M["coat"], segments=22, name="skirt")
	skirt.scale = (1.17, 0.85, 1.0)
	geo.apply_transform(skirt)
	parts.append(skirt_weights(skirt))
	parts.append(skirt_weights(geo.tube(ring((0, 0.025, 0.505), 0.24, 0.174, 20), 0.014, mat=M["coat_dark"], segments=6, name="hem")))
	parts.append(skirt_weights(geo.tube([(-0.03, -0.142, 0.98), (-0.035, -0.152, 0.75), (-0.04, -0.158, 0.52)], 0.012, mat=M["coat_dark"],
										segments=6, name="placket")))
	parts.append(skirt_weights(geo.box((0.1, 0.03, 0.11), (-0.13, -0.122, 0.8), (0, 0, -28), mat=M["coat"], bevel=0.008, name="hip_pocket")))
	parts.append(skirt_weights(geo.box((0.108, 0.026, 0.035), (-0.13, -0.128, 0.86), (0, 0, -28), mat=M["coat_dark"], bevel=0.006, name="hip_flap")))

	# Sleeves with darker cuffs, bare hands.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.19 * sx, 0.01, 1.385), (0.24 * sx, 0.02, 1.10)], [0.064, 0.058], mat=M["coat"], segments=12, name="sleeve_up"), "upperarm_" + side)
		part(geo.tube([(0.24 * sx, 0.02, 1.10), (0.258 * sx, -0.016, 0.89)], [0.058, 0.052], mat=M["coat"], segments=12, name="sleeve_fore"), "forearm_" + side)
		part(geo.tube([(0.257 * sx, -0.014, 0.905), (0.261 * sx, -0.022, 0.868)], 0.056, mat=M["coat_dark"], segments=12, name="cuff"), "forearm_" + side)
		part(geo.blob((0.038, 0.03, 0.056), (0.265 * sx, -0.03, 0.81), mat=M["skin"], name="hand"), "hand_" + side)
		part(geo.blob((0.015, 0.015, 0.026), (0.248 * sx, -0.062, 0.83), (14, 0, 0), mat=M["skin"], name="thumb"), "hand_" + side)

	# Dark trousers below the coat and worn lace-up boots.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.10 * sx, 0.01, 0.95), (0.10 * sx, 0.005, 0.50)], [0.085, 0.07], mat=M["trousers"], segments=14, name="thigh"), "thigh_" + side)
		part(geo.tube([(0.10 * sx, 0.005, 0.50), (0.10 * sx, 0.012, 0.22)], [0.068, 0.062], mat=M["trousers"], segments=14, name="shin"), "shin_" + side)
		part(geo.tube([(0.10 * sx, 0.012, 0.25), (0.10 * sx, 0.015, 0.07)], [0.064, 0.06], mat=M["boots"], segments=14, name="boot_shaft"), "shin_" + side)
		part(geo.blob((0.056, 0.12, 0.048), (0.10 * sx, -0.035, 0.055), mat=M["boots"], name="boot"), "foot_" + side)
		part(geo.box((0.1, 0.215, 0.022), (0.10 * sx, -0.035, 0.011), mat=M["sole"], bevel=0.009, name="sole"), "foot_" + side)

	# The satchel across his body: strap over the right shoulder, bag on the left front of the hip, a rolled map.
	bag = Vector((0.14, -0.178, 0.83))
	part(geo.box((0.22, 0.07, 0.18), tuple(bag), (0, 0, 18), mat=M["leather"], bevel=0.02, name="satchel"), "hips")
	part(geo.box((0.226, 0.022, 0.12), tuple(bag + Vector((0.011, -0.035, 0.035))), (0, 0, 18), mat=M["leather"], bevel=0.01, name="satchel_flap"), "hips")
	part(geo.box((0.03, 0.012, 0.06), tuple(bag + Vector((0.014, -0.052, 0.0))), (0, 0, 18), mat=M["strap"], name="satchel_tab"), "hips")
	part(geo.cylinder(0.024, 0.17, tuple(bag + Vector((-0.06, 0.012, 0.11))), (0, -14, 18), mat=M["paper"], segments=10, name="map"), "hips")
	strap = geo.tube([(0.05, -0.19, 0.92), (0.02, -0.15, 1.04), (-0.05, -0.14, 1.20), (-0.11, -0.115, 1.33), (-0.155, -0.045, 1.425),
					  (-0.16, 0.04, 1.445), (-0.125, 0.125, 1.34), (-0.03, 0.15, 1.18), (0.08, 0.15, 1.04), (0.14, 0.13, 0.95)],
					 0.014, mat=M["strap"], segments=6, name="strap")
	parts.append(zones(strap))

	res = pipeline.organic("dohyeon", parts, voxel=0.0095, tris=5500, size=512, smooth=2)
	low = res["mesh"]

	arm = rig.armature("dohyeon_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)
	report_weights("dohyeon", low, DEFORM)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("idle", 60), ("talk", 10), ("talk", 22), ("talk", 42)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 2.2
	return res


# ---- poses ---------------------------------------------------------------------------------

def hand_on_bag(pose, root=(0, 0, 0)):
	# Left hand resting on the satchel's flap, as if guarding what's inside.
	P.reach(pose, "upperarm_L", "forearm_L", P.carried(pose, "hips", (0.2, -0.235, 0.975), root), (0.7, 0.5, -1.0), root)
	P.aim(pose, "hand_L", (-0.2, -0.35, -1.0), root)
	return pose


def gesture(pose, wrist, hand_dir, root=(0, 0, 0)):
	P.reach(pose, "upperarm_R", "forearm_R", P.carried(pose, "chest", wrist, root), (-0.3, 0.5, -1.0), root)
	P.aim(pose, "hand_R", hand_dir, root)
	return pose


def push_glasses(pose, root=(0, 0, 0)):
	bridge = P.carried(pose, "head", GLASSES, root)
	P.reach(pose, "upperarm_R", "forearm_R", bridge + Vector((-0.015, -0.06, -0.1)), (-0.5, 0.4, -1.0), root)
	P.aim(pose, "hand_R", (0.12, -0.35, 1.0), root)
	return pose


def animations(arm):
	clips = []
	still = (0, 0, 0)

	# Idle: slow breaths, the left hand on the satchel, a look round now and then.
	def idle(breath, look):
		pose = {"spine": (1 - breath * 0.8, 0, 0), "chest": (2 - breath * 2.0, 0, 0), "neck": (0, 0, look * 6),
				"head": (-2 + breath * 1.2, 0, look * 10), "shoulder_L": (0, -breath * 2.0, 0), "shoulder_R": (0, breath * 2.0, 0),
				"upperarm_R": (-3 + breath * 1.5, 0, 0)}
		return hand_on_bag(pose)
	idle_a = idle(0.0, 0.0)
	clips.append(rig.clip(arm, "idle", [(0, idle_a, still), (30, idle(1.0, 0.0), still), (60, idle(0.0, 0.8), still),
										(84, idle(1.0, 0.7), still), (100, idle(0.3, -0.2), still), (120, idle_a, still)]))

	# Talk: the right hand explains in front of him, the head nods, and once he pushes his glasses up.
	def talk(nod, which, lean):
		pose = {"spine": (2, 0, -3), "chest": (4 + lean, 0, -3), "neck": (0, 0, 0), "head": (-4 + nod, 0, 4)}
		hand_on_bag(pose)
		if which == 1:
			gesture(pose, (-0.12, -0.32, 1.13), (0.1, -1.0, 0.35))
		elif which == 2:
			gesture(pose, (-0.09, -0.27, 1.21), (0.0, -0.45, 1.0))
		elif which == 3:
			push_glasses(pose)
		return pose
	talk_a = talk(0, 0, 0)
	clips.append(rig.clip(arm, "talk", [(0, talk_a, still), (10, talk(8, 1, 2), still), (22, talk(-1, 2, 0), still),
										(32, talk(9, 1, 3), still), (42, talk(-3, 3, 0), still), (52, talk(4, 2, 1), still),
										(64, talk_a, still)]))
	return clips
