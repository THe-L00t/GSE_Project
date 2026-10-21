# Sun-im, Hayul's grandmother, awake: short, round and hunched, white hair in a low bun with a lacquered
# pin, a knitted cardigan over a blouse, an apron, floral monpe and white rubber shoes. A faint spore stain
# sits on the back of her left hand. Rigged: idle, talk, yawn, squat.
import math

from mathutils import Euler, Vector

from gse import geo, mat, pipeline, rig


def _side(bones):
	# Left-side bones (+X) and their mirrored right-side copies.
	out = []
	for side, sx in (("L", 1.0), ("R", -1.0)):
		for name, head, tail, parent in bones:
			p = parent if parent in ("chest", "hips") else parent + "_" + side
			out.append((name + "_" + side, (head[0] * sx, head[1], head[2]), (tail[0] * sx, tail[1], tail[2]), p))
	return out


BONES = [
	("root", (0, 0, 0), (0, 0, 0.2), None),
	("hips", (0, 0.03, 0.60), (0, 0.03, 0.72), "root"),
	("spine", (0, 0.03, 0.72), (0, 0.02, 0.86), "hips"),
	("chest", (0, 0.02, 0.86), (0, -0.08, 1.00), "spine"),
	("neck", (0, -0.08, 1.00), (0, -0.14, 1.08), "chest"),
	("head", (0, -0.14, 1.08), (0, -0.14, 1.32), "neck"),
] + _side([
	("shoulder", (0.04, -0.05, 0.955), (0.15, -0.05, 0.975), "chest"),
	("upperarm", (0.15, -0.05, 0.975), (0.25, -0.04, 0.775), "shoulder"),
	("forearm", (0.25, -0.04, 0.775), (0.275, -0.06, 0.59), "upperarm"),
	("hand", (0.275, -0.06, 0.59), (0.28, -0.07, 0.50), "forearm"),
]) + _side([
	("thigh", (0.10, 0.02, 0.60), (0.10, 0.02, 0.33), "hips"),
	("shin", (0.10, 0.02, 0.33), (0.10, 0.02, 0.08), "thigh"),
	("foot", (0.10, 0.02, 0.08), (0.10, -0.11, 0.03), "shin"),
])
DEFORM = [b[0] for b in BONES if b[0] != "root"]
REST = {b[0]: b for b in BONES}

HEAD_C = Vector((0.0, -0.165, 1.19))
HEAD_K = 0.95


def H(x, y, z):
	# A point on the head, given relative to its centre at the unscaled size.
	return tuple(HEAD_C + Vector((x, y, z)) * HEAD_K)


def HR(x, y, z):
	return (x * HEAD_K, y * HEAD_K, z * HEAD_K)


MOUTH = H(0.0, -0.118, -0.068)


# ---- small pose solver: forward kinematics as rig.key poses, and two-bone reach ----------------

def _q(r):
	return Euler([math.radians(a) for a in r], 'XYZ').to_quaternion()


def _deg(q):
	e = q.to_euler('XYZ')
	return (math.degrees(e.x), math.degrees(e.y), math.degrees(e.z))


def fk(pose, root=(0, 0, 0)):
	out = {}
	for name, head, tail, parent in BONES:
		q = _q(pose.get(name, (0, 0, 0)))
		if parent is None:
			out[name] = (Vector(head) + Vector(root), q)
		else:
			ph, pq = out[parent]
			out[name] = (ph + pq @ (Vector(head) - Vector(REST[parent][1])), pq @ q)
	return out


def carried(pose, bone, p, root=(0, 0, 0)):
	h, q = fk(pose, root)[bone]
	return h + q @ (Vector(p) - Vector(REST[bone][1]))


def _length(bone):
	return (Vector(REST[bone][2]) - Vector(REST[bone][1])).length


def aim(pose, bone, want, root=(0, 0, 0)):
	rest = (Vector(REST[bone][2]) - Vector(REST[bone][1])).normalized()
	pq = fk(pose, root)[REST[bone][3]][1]
	pose[bone] = _deg(rest.rotation_difference(pq.inverted() @ Vector(want).normalized()))
	return pose


def level(pose, bone, root=(0, 0, 0)):
	# Undo the chain's rotation so this bone keeps its rest orientation (feet flat).
	pose[bone] = _deg(fk(pose, root)[REST[bone][3]][1].inverted())
	return pose


def reach(pose, upper, lower, target, pole, root=(0, 0, 0)):
	s = fk(pose, root)[upper][0]
	a, b = _length(upper), _length(lower)
	t = Vector(target)
	d = max(abs(a - b) + 1e-3, min(a + b - 1e-3, (t - s).length))
	n = (t - s).normalized()
	p = Vector(pole) - n * Vector(pole).dot(n)
	p.normalize()
	ca = (a * a + d * d - b * b) / (2.0 * a * d)
	e = s + n * (a * ca) + p * (a * math.sqrt(max(0.0, 1.0 - ca * ca)))
	aim(pose, upper, e - s, root)
	aim(pose, lower, s + n * d - e, root)
	return pose


# ---- materials ---------------------------------------------------------------------------

def floral(name, base, flowers, scale=18.0):
	# Cloth with a scatter of small flowers: two muted colours on some cells of a voronoi grid.
	m = mat.make(name, base, pattern="cloth", rough=0.9, bump=0.4)
	if m.get("gse_floral"):
		return m
	m["gse_floral"] = 1
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
	under = bsdf.inputs["Base Color"].links[0].from_socket
	tc = nt.nodes.new("ShaderNodeTexCoord")
	vor = nt.nodes.new("ShaderNodeTexVoronoi")
	vor.inputs["Scale"].default_value = scale
	vor.inputs["Randomness"].default_value = 0.8
	nt.links.new(tc.outputs["Object"], vor.inputs["Vector"])

	def ramp(src, lo, hi, a, b):
		r = nt.nodes.new("ShaderNodeMapRange")
		r.clamp = True
		nt.links.new(src, r.inputs[0])
		r.inputs[1].default_value = lo
		r.inputs[2].default_value = hi
		r.inputs[3].default_value = a
		r.inputs[4].default_value = b
		return r.outputs[0]

	def mix(fac, a, b):
		n = nt.nodes.new("ShaderNodeMix")
		n.data_type = 'RGBA'
		for sock, v in ((n.inputs[0], fac), (n.inputs[6], a), (n.inputs[7], b)):
			if isinstance(v, tuple):
				sock.default_value = v
			else:
				nt.links.new(v, sock)
		return n.outputs[2]

	sep = nt.nodes.new("ShaderNodeSeparateColor")
	nt.links.new(vor.outputs["Color"], sep.inputs[0])
	dots = ramp(vor.outputs["Distance"], 0.16, 0.24, 0.9, 0.0)
	keep = ramp(sep.outputs[1], 0.40, 0.45, 0.0, 1.0)
	fac = nt.nodes.new("ShaderNodeMath")
	fac.operation = 'MULTIPLY'
	nt.links.new(dots, fac.inputs[0])
	nt.links.new(keep, fac.inputs[1])
	pick = ramp(sep.outputs[0], 0.48, 0.52, 0.0, 1.0)
	fcol = mix(pick, tuple(flowers[0]) + (1.0,), tuple(flowers[1]) + (1.0,))
	nt.links.new(mix(fac.outputs[0], under, fcol), bsdf.inputs["Base Color"])
	return m


def materials():
	return dict(
		skin=mat.get("skin"),
		hair=mat.make("hair_combed", (0.80, 0.79, 0.76), pattern="hair", rough=0.7, bump=0.2),
		cardigan=mat.get("cardigan"),
		apron=mat.get("apron"),
		band=mat.make("cardigan_band", (0.42, 0.34, 0.28), pattern="knit", rough=0.95, bump=0.5),
		blouse=mat.make("blouse", (0.66, 0.62, 0.60), pattern="cloth", rough=0.9, bump=0.3),
		pocket=mat.make("apron_pocket", (0.55, 0.52, 0.46), pattern="cloth", rough=0.9, bump=0.35),
		monpe=floral("monpe_floral", (0.31, 0.25, 0.32), ((0.58, 0.44, 0.47), (0.64, 0.61, 0.50))),
		cuff=mat.make("monpe_cuff", (0.25, 0.20, 0.26), pattern="knit", rough=0.92, bump=0.4),
		shoe=mat.make("gomusin", (0.60, 0.60, 0.57), pattern="plain", rough=0.55, bump=0.1, grime=0.45),
		sock=mat.make("sock", (0.36, 0.33, 0.31), pattern="knit", rough=0.95, bump=0.3),
		eye=mat.make("eye", (0.06, 0.05, 0.05), pattern="plain", rough=0.3, grime=0.0, edge=0.0, bump=0.0),
		blush=mat.make("blush", (0.62, 0.43, 0.39), pattern="skin", rough=0.75, bump=0.1, grime=0.1, edge=0.0),
		pin=mat.make("lacquer_pin", (0.34, 0.16, 0.12), pattern="plain", rough=0.35, bump=0.05, grime=0.1),
		button=mat.make("button", (0.26, 0.20, 0.16), pattern="plain", rough=0.5, bump=0.05),
		stain=mat.get("spore_glow"),
	)


# ---- the apron: hugs the belly, lies on the thighs and follows them when she squats -------------

APRON_FRONT = [(0.745, -0.128), (0.68, -0.137), (0.60, -0.124), (0.50, -0.106), (0.40, -0.097)]


def apron_front(z):
	pts = APRON_FRONT
	if z >= pts[0][0]:
		return pts[0][1]
	for (z0, y0), (z1, y1) in zip(pts, pts[1:]):
		if z1 <= z <= z0:
			t = (z0 - z) / (z0 - z1)
			return y0 + (y1 - y0) * t
	return pts[-1][1]


def apron_point(u, t, back=0.5, top=0.745, bottom=0.40, thick=0.022):
	# u: -1..1 across, t: 0 (waist)..1 (hem), back: 0 (front face)..1 (back face).
	half = 0.17 + 0.012 * t
	z = top - t * (top - bottom) + 0.008 * math.sin(u * math.pi * 2.0 + 0.6) * t * t
	y = apron_front(z) + 0.06 * u * u + 0.005 * math.sin(u * math.pi * 3.0) * t + back * thick
	return Vector((u * half, y, z))


def apron_panel(m):
	obj = geo.box((1.0, 1.0, 1.0), mat=m, subdiv=8, name="apron")
	for v in obj.data.vertices:
		v.co = apron_point(v.co.x * 2.0, 0.5 - v.co.z, v.co.y + 0.5)
	obj.data.update()
	return obj


def apron_hem(m):
	# A rolled hem down both sides and along the bottom, so the edges read clean.
	pts = [apron_point(-1.0, i / 6.0) for i in range(7)]
	pts += [apron_point(-1.0 + i / 6.0, 1.0) for i in range(1, 12)]
	pts += [apron_point(1.0, 1.0 - i / 6.0) for i in range(1, 7)]
	return geo.tube([tuple(p) for p in pts], 0.013, mat=m, segments=6, name="apron_hem")


def apron_weights(obj):
	def hips(co):
		return max(0.0, min(1.0, (co.z - 0.56) / 0.12))
	def left(co):
		return max(0.0, min(1.0, 0.5 + co.x / 0.14))
	geo.tag_fn(obj, "hips", lambda co, n: hips(co))
	geo.tag_fn(obj, "thigh_L", lambda co, n: (1.0 - hips(co)) * left(co))
	geo.tag_fn(obj, "thigh_R", lambda co, n: (1.0 - hips(co)) * (1.0 - left(co)))
	return obj


def build():
	M = materials()
	parts = []

	def part(obj, *tags):
		geo.tag(obj, *tags)
		parts.append(obj)
		return obj

	# Head: a little large for the diorama read, a kind round face, big ears, white hair pulled back to a low bun.
	part(geo.sphere(HR(0.13, 0.125, 0.14), H(0, 0, 0), mat=M["skin"], segments=24, rings=16, name="head"), "head")
	part(geo.blob(HR(0.141, 0.136, 0.112), H(0, 0.036, 0.072), mat=M["hair"], lumpy=0.012, seed=4, name="hair"), "head")
	part(geo.blob(HR(0.118, 0.08, 0.10), H(0, 0.09, -0.012), mat=M["hair"], lumpy=0.015, seed=6, name="hair_back"), "head")
	part(geo.blob((0.072, 0.052, 0.058), H(0, 0.168, -0.062), mat=M["hair"], lumpy=0.04, seed=9, name="bun"), "head")
	part(geo.cylinder(0.011, 0.22, H(0, 0.178, -0.058), (0, 90, 4), mat=M["pin"], segments=8, name="binyeo"), "head")
	part(geo.sphere(0.02, H(0.115, 0.178, -0.05), mat=M["pin"], segments=10, rings=8, name="binyeo_knob"), "head")
	for sx in (1.0, -1.0):
		part(geo.sphere(HR(0.017, 0.012, 0.014), H(0.05 * sx, -0.108, 0.012), mat=M["eye"], segments=8, rings=6, name="eye"), "head")
		part(geo.sphere(HR(0.026, 0.034, 0.046), H(0.132 * sx, 0.005, -0.008), (0, 0, -12 * sx), mat=M["skin"], segments=10, rings=8, name="ear"), "head")
		part(geo.sphere(HR(0.03, 0.016, 0.022), H(0.072 * sx, -0.096, -0.035), mat=M["blush"], segments=8, rings=6, name="cheek"), "head")
	part(geo.sphere(HR(0.021, 0.024, 0.024), H(0, -0.132, -0.02), mat=M["skin"], segments=10, rings=8, name="nose"), "head")
	part(geo.tube([(0, -0.09, 0.985), (0, -0.158, 1.10)], 0.05, mat=M["skin"], segments=12, name="neck"), "neck")

	# Blouse collar round the neck and showing in the cardigan's V.
	ring = [(0.072 * math.cos(a / 12.0 * math.tau), -0.11 + 0.064 * math.sin(a / 12.0 * math.tau), 1.012 - 0.02 * math.sin(a / 12.0 * math.tau)) for a in range(13)]
	part(geo.tube(ring, 0.02, mat=M["blouse"], segments=8, name="collar"), "chest")
	for sx in (1.0, -1.0):
		part(geo.blob((0.032, 0.012, 0.024), (0.032 * sx, -0.182, 0.995), (0, 0, 20 * sx), mat=M["blouse"], name="collar_tip"), "chest")
	part(geo.blob((0.045, 0.02, 0.055), (0, -0.168, 0.945), (12, 0, 0), mat=M["blouse"], name="blouse_v"), "chest")

	# The cardigan body: a swept, rounded trunk leaning forward, with a hump on the upper back.
	trunk = geo.tube([(0, 0.045, 0.575), (0, 0.045, 0.68), (0, 0.035, 0.78), (0, 0.005, 0.875), (0, -0.04, 0.95), (0, -0.085, 1.005), (0, -0.125, 1.04)],
					 [0.13, 0.163, 0.155, 0.143, 0.125, 0.095, 0.055], mat=M["cardigan"], segments=22, name="trunk")
	trunk.scale = (1.15, 1.0, 1.0)
	geo.apply_transform(trunk)
	geo.tag_fn(trunk, "hips", lambda co, n: 1.0 if co.z < 0.72 else 0.0)
	geo.tag_fn(trunk, "spine", lambda co, n: 1.0 if 0.72 <= co.z < 0.86 else 0.0)
	geo.tag_fn(trunk, "chest", lambda co, n: 1.0 if co.z >= 0.86 else 0.0)
	parts.append(trunk)
	part(geo.blob((0.14, 0.105, 0.115), (0, 0.06, 0.925), mat=M["cardigan"], lumpy=0.04, seed=3, name="hump"), "chest")
	hem = [(0.155 * math.cos(a / 16.0 * math.tau), 0.045 + 0.135 * math.sin(a / 16.0 * math.tau), 0.592) for a in range(17)]
	part(geo.tube(hem, 0.014, mat=M["band"], segments=6, name="hem"), "hips")
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.blob((0.072, 0.07, 0.06), (0.125 * sx, -0.045, 0.955), mat=M["cardigan"], name="shoulder"), "shoulder_" + side)
		part(geo.tube([(0.05 * sx, -0.18, 0.995), (0.02 * sx, -0.162, 0.92), (0.02 * sx, -0.14, 0.85), (0.02 * sx, -0.124, 0.765)], 0.013,
					  mat=M["band"], segments=6, name="placket"), "chest")
	for z, y in ((0.905, -0.159), (0.845, -0.139), (0.785, -0.128)):
		part(geo.sphere(0.012, (-0.02, y, z), mat=M["button"], segments=8, rings=6, name="button"), "spine" if z < 0.86 else "chest")

	# Apron: waist band, ties in a bow at the back, the panel and its pocket.
	band = [(0.183 * math.cos(a / 18.0 * math.tau), 0.04 + 0.16 * math.sin(a / 18.0 * math.tau), 0.742) for a in range(19)]
	part(geo.tube(band, 0.016, mat=M["apron"], segments=6, name="apron_band"), "hips")
	for sx in (1.0, -1.0):
		part(geo.blob((0.038, 0.018, 0.026), (0.036 * sx, 0.21, 0.748), (0, 20 * sx, 0), mat=M["apron"], name="bow"), "hips")
		part(geo.blob((0.018, 0.012, 0.055), (0.022 * sx, 0.207, 0.69), (0, 10 * sx, 0), mat=M["apron"], name="bow_tail"), "hips")
	parts.append(apron_weights(apron_panel(M["apron"])))
	parts.append(apron_weights(apron_hem(M["apron"])))
	parts.append(apron_weights(geo.blob((0.062, 0.012, 0.046), (0.06, apron_front(0.52) - 0.003, 0.52), mat=M["pocket"], name="apron_pocket")))

	# Arms in loose cardigan sleeves with knitted cuffs; the stain on the back of the left hand.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.14 * sx, -0.05, 0.985), (0.25 * sx, -0.04, 0.775)], [0.06, 0.053], mat=M["cardigan"], segments=12, name="sleeve_up"), "upperarm_" + side)
		part(geo.tube([(0.25 * sx, -0.04, 0.775), (0.271 * sx, -0.057, 0.625)], [0.053, 0.05], mat=M["cardigan"], segments=12, name="sleeve_fore"), "forearm_" + side)
		part(geo.tube([(0.270 * sx, -0.055, 0.635), (0.275 * sx, -0.06, 0.597)], 0.053, mat=M["band"], segments=12, name="cuff"), "forearm_" + side)
		part(geo.blob((0.044, 0.036, 0.056), (0.28 * sx, -0.065, 0.543), mat=M["skin"], name="hand"), "hand_" + side)
		part(geo.blob((0.017, 0.016, 0.027), (0.262 * sx, -0.098, 0.558), (14, 0, 0), mat=M["skin"], name="thumb"), "hand_" + side)
	part(geo.blob((0.007, 0.018, 0.015), (0.321, -0.062, 0.556), mat=M["stain"], name="stain"), "hand_L", "pulse")

	# Floral monpe, baggy and gathered at the ankles, and white rubber shoes with turned-up toes.
	for side, sx in (("L", 1.0), ("R", -1.0)):
		part(geo.tube([(0.10 * sx, 0.025, 0.64), (0.10 * sx, 0.02, 0.33)], [0.092, 0.084], mat=M["monpe"], segments=14, name="thigh"), "thigh_" + side)
		part(geo.tube([(0.10 * sx, 0.02, 0.33), (0.10 * sx, 0.02, 0.16), (0.10 * sx, 0.02, 0.105)], [0.084, 0.078, 0.052], mat=M["monpe"], segments=14, name="shin"), "shin_" + side)
		part(geo.tube([(0.10 * sx, 0.02, 0.118), (0.10 * sx, 0.02, 0.088)], 0.054, mat=M["cuff"], segments=12, name="ankle"), "shin_" + side)
		part(geo.blob((0.04, 0.07, 0.03), (0.10 * sx, 0.0, 0.062), mat=M["sock"], name="sock"), "foot_" + side)
		part(geo.blob((0.05, 0.115, 0.034), (0.10 * sx, -0.035, 0.034), mat=M["shoe"], name="shoe"), "foot_" + side)
		part(geo.blob((0.017, 0.032, 0.016), (0.10 * sx, -0.146, 0.052), (-34, 0, 0), mat=M["shoe"], name="shoe_toe"), "foot_" + side)

	res = pipeline.organic("grandma", parts, voxel=0.0095, tris=5000, size=512, smooth=2)
	low = res["mesh"]

	arm = rig.armature("grandma_rig", BONES)
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)
	_check_weights(low)

	res["armature"] = arm
	res["clips"] = animations(arm)
	res["preview_frames"] = [("idle", 0), ("idle", 45), ("talk", 8), ("talk", 34), ("yawn", 12), ("yawn", 24), ("yawn", 38), ("squat", 0)]
	res["preview_views"] = (45.0,)
	res["preview_height"] = 1.75
	return res


def _check_weights(obj):
	names = {g.index: g.name for g in obj.vertex_groups}
	bare = 0
	for v in obj.data.vertices:
		if not any(names.get(ge.group) in DEFORM and ge.weight > 0.0 for ge in v.groups):
			bare += 1
	print("GSE_WEIGHTS grandma unweighted=%d of %d" % (bare, len(obj.data.vertices)))


# ---- poses ---------------------------------------------------------------------------------

def hands_behind(pose, root=(0, 0, 0), lift=0.0, sides=("L", "R")):
	# Hands clasped on the small of the back, elbows out a little: the way she stands.
	for side in sides:
		sx = 1.0 if side == "L" else -1.0
		reach(pose, "upperarm_" + side, "forearm_" + side, carried(pose, "hips", (0.05 * sx, 0.215, 0.72 + lift), root), (sx * 0.6, 0.6, -1.0), root)
		aim(pose, "hand_" + side, (-sx, 0.3, -0.3), root)
	return pose


def hand_at_mouth(pose, root=(0, 0, 0)):
	mouth = carried(pose, "head", MOUTH, root)
	reach(pose, "upperarm_R", "forearm_R", mouth + Vector((-0.02, -0.05, -0.085)), (-0.6, 0.3, -1.0), root)
	aim(pose, "hand_R", (0.15, -0.25, 1.0), root)
	return pose


def gesture(pose, wrist, hand_dir, root=(0, 0, 0)):
	reach(pose, "upperarm_R", "forearm_R", carried(pose, "chest", wrist, root), (-0.25, 0.5, -1.0), root)
	aim(pose, "hand_R", hand_dir, root)
	return pose


def squat_pose(breath=0.0):
	root = (0, 0.13, -0.335)
	pose = {"spine": (20, 0, 0), "chest": (12 - breath * 2.5, 0, 0), "neck": (-8, 0, 0), "head": (-22 + breath * 2.0, 0, 0),
			"shoulder_L": (0, -breath * 2.0, 0), "shoulder_R": (0, breath * 2.0, 0)}
	for side, sx in (("L", 1.0), ("R", -1.0)):
		reach(pose, "thigh_" + side, "shin_" + side, REST["shin_" + side][2], (0.4 * sx, -1.0, 0.15), root)
		level(pose, "foot_" + side, root)
	for side, sx in (("L", 1.0), ("R", -1.0)):
		knee = fk(pose, root)["shin_" + side][0]
		reach(pose, "upperarm_" + side, "forearm_" + side, knee + Vector((-0.01 * sx, -0.10, 0.05)), (sx, 0.2, -1.0), root)
		aim(pose, "hand_" + side, (-0.1 * sx, -0.35, -1.0), root)
	return pose, root


def animations(arm):
	clips = []
	still = (0, 0, 0)

	# Idle: hands behind her back, slow breaths, a slight sway.
	def idle(breath, sway):
		pose = {"spine": (2 - breath * 1.0, sway * 1.2, 0), "chest": (4 - breath * 2.5, sway * 0.8, 0), "neck": (-2, 0, 0),
				"head": (-3 + breath * 1.5, -sway * 1.5, sway * 2.0),
				"shoulder_L": (0, -breath * 2.5, 0), "shoulder_R": (0, breath * 2.5, 0)}
		return hands_behind(pose)
	idle_a = idle(0.0, 0.0)
	clips.append(rig.clip(arm, "idle", [(0, idle_a, still), (24, idle(1.0, 0.6), still), (45, idle(0.0, 1.0), still),
										(69, idle(1.0, 0.4), still), (90, idle_a, still)]))

	# Talk: left hand stays behind, the right hand fusses in front, the head nods along: "did ye eat?"
	def talk(nod, flick, lean):
		pose = {"spine": (3, 0, 4), "chest": (6 + lean, 0, 3), "neck": (-2, 0, 0), "head": (-6 + nod, 0, -5)}
		hands_behind(pose, sides=("L",))
		if flick:
			gesture(pose, (-0.09, -0.31, 0.83), (0.05, -1.0, 0.1))
		else:
			gesture(pose, (-0.08, -0.27, 0.89), (0.0, -0.4, 1.0))
		return pose
	talk_a = talk(0, False, 0)
	clips.append(rig.clip(arm, "talk", [(0, talk_a, still), (8, talk(9, True, 2), still), (16, talk(-1, False, 0), still),
										(24, talk(10, True, 3), still), (34, talk(0, False, 1), still),
										(46, talk(5, False, 2), still), (60, talk_a, still)]))

	# Yawn: the hand goes to her mouth, the head tips back, a stretch of the sore back, then a sleepy slump.
	cover = {"chest": (-2, 0, 0), "head": (-9, 0, 3)}
	hands_behind(cover, sides=("L",))
	hand_at_mouth(cover)
	gape = {"spine": (-3, 0, 0), "chest": (-7, 0, 0), "neck": (-4, 0, 0), "head": (-16, 0, 4), "shoulder_L": (0, -7, 0), "shoulder_R": (0, 7, 0)}
	hands_behind(gape, sides=("L",))
	hand_at_mouth(gape)
	stretch = {"spine": (-6, 0, 0), "chest": (-10, 0, 0), "head": (-8, 0, 0), "shoulder_L": (0, -4, 0), "shoulder_R": (0, 4, 0)}
	hands_behind(stretch, lift=0.03)
	slump = {"spine": (5, 0, 0), "chest": (9, 0, 0), "neck": (4, 0, 0), "head": (8, 0, -4)}
	hands_behind(slump)
	clips.append(rig.clip(arm, "yawn", [(0, idle_a, still), (12, cover, still), (24, gape, still),
										(38, stretch, still), (50, slump, still), (60, idle_a, still)], loop=False))

	# Squat: down on her heels, forearms over her knees, breathing; she sits like this more as evening comes.
	sq_a, root_a = squat_pose(0.0)
	sq_b, root_b = squat_pose(1.0)
	clips.append(rig.clip(arm, "squat", [(0, sq_a, root_a), (40, sq_b, root_b), (90, sq_a, root_a)]))
	return clips
