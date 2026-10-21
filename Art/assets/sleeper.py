# A village neighbour in the long sleep: on their back on a straw mat, head on a folded cloth, a living moss
# blanket over the body with a fern and a few spore motes. The chest lifts and settles with each slow breath.
# make(elder=True) is grandmother asleep the next morning (sleeper_elder).
import math

from mathutils import Vector

from gse import geo, mat, pipeline, rig

DEFORM = ["hips", "spine", "chest", "neck", "head"]


def bones(chest_y, belly_y, hip_y):
	# Lying on the back, head toward -Y. The neck hangs off the hips so a rising chest leaves the head on its pillow.
	return [
		("root", (0, 0, 0), (0, 0, 0.2), None),
		("hips", (0, hip_y, 0.13), (0, belly_y, 0.13), "root"),
		("spine", (0, belly_y, 0.13), (0, chest_y, 0.14), "hips"),
		("chest", (0, chest_y, 0.14), (0, -0.73, 0.15), "spine"),
		("neck", (0, -0.75, 0.15), (0, -0.85, 0.17), "hips"),
		("head", (0, -0.85, 0.17), (0, -1.08, 0.20), "neck"),
	]


def clamp(x):
	return max(0.0, min(1.0, x))


# ---- materials ---------------------------------------------------------------------------

def straw(name, base, cord):
	# Woven straw: reeds across the width, darker binding cords running the length.
	m = mat.make(name, base, pattern="plain", rough=0.95, bump=0.25, var=0.18, var_scale=5.0)
	if m.get("gse_straw"):
		return m
	m["gse_straw"] = 1
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
	under = bsdf.inputs["Base Color"].links[0].from_socket
	old_bump = bsdf.inputs["Normal"].links[0].from_socket if bsdf.inputs["Normal"].links else None
	tc = nt.nodes.new("ShaderNodeTexCoord")

	def wave(scale, axis, profile, distortion):
		w = nt.nodes.new("ShaderNodeTexWave")
		w.wave_type = 'BANDS'
		w.bands_direction = axis
		w.wave_profile = profile
		w.inputs["Scale"].default_value = scale
		w.inputs["Distortion"].default_value = distortion
		w.inputs["Detail"].default_value = 1.0
		nt.links.new(tc.outputs["Object"], w.inputs["Vector"])
		return w.outputs["Fac"]

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

	reeds = wave(12.0, 'Y', 'SIN', 0.6)
	cords = ramp(wave(1.6, 'X', 'SAW', 0.0), 0.0, 0.07, 1.0, 0.0)
	c = mix(ramp(reeds, 0.2, 0.9, 0.0, 0.35), under, (base[0] * 0.62, base[1] * 0.58, base[2] * 0.52, 1.0))
	c = mix(cords, c, tuple(cord) + (1.0,))
	nt.links.new(c, bsdf.inputs["Base Color"])
	bump = nt.nodes.new("ShaderNodeBump")
	bump.inputs["Strength"].default_value = 0.5
	bump.inputs["Distance"].default_value = 0.01
	h = nt.nodes.new("ShaderNodeMath")
	h.operation = 'SUBTRACT'
	nt.links.new(reeds, h.inputs[0])
	nt.links.new(cords, h.inputs[1])
	nt.links.new(h.outputs[0], bump.inputs["Height"])
	if old_bump is not None:
		nt.links.new(old_bump, bump.inputs["Normal"])
	nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
	return m


def materials(elder):
	from assets import grandma
	M = dict(
		skin=mat.get("skin"),
		eye=mat.make("eye", (0.06, 0.05, 0.05), pattern="plain", rough=0.3, grime=0.0, edge=0.0, bump=0.0),
		straw=straw("straw_mat", (0.50, 0.43, 0.29), (0.30, 0.24, 0.16)),
		binding=mat.make("straw_binding", (0.30, 0.24, 0.16), pattern="cloth", rough=0.95, bump=0.4),
		moss=mat.get("moss"),
		moss_dark=mat.get("moss_dark"),
		moss_light=mat.make("moss_light", "moss_light", pattern="moss", rough=0.95, bump=0.5),
		fern=mat.make("fern", (0.30, 0.44, 0.25), pattern="leaf", rough=0.8, bump=0.4),
		spore=mat.get("spore_glow"),
	)
	if elder:
		M.update(
			hair=mat.make("hair_combed", (0.80, 0.79, 0.76), pattern="hair", rough=0.7, bump=0.2),
			pin=mat.make("lacquer_pin", (0.34, 0.16, 0.12), pattern="plain", rough=0.35, bump=0.05, grime=0.1),
			blush=mat.make("blush", (0.62, 0.43, 0.39), pattern="skin", rough=0.75, bump=0.1, grime=0.1, edge=0.0),
			top=mat.get("cardigan"),
			band=mat.make("cardigan_band", (0.42, 0.34, 0.28), pattern="knit", rough=0.95, bump=0.5),
			collar=mat.make("blouse", (0.66, 0.62, 0.60), pattern="cloth", rough=0.9, bump=0.3),
			apron=mat.get("apron"),
			legs=grandma.floral("monpe_floral", (0.31, 0.25, 0.32), ((0.58, 0.44, 0.47), (0.64, 0.61, 0.50))),
			cuff=mat.make("monpe_cuff", (0.25, 0.20, 0.26), pattern="knit", rough=0.92, bump=0.4),
			shoe=mat.make("gomusin", (0.60, 0.60, 0.57), pattern="plain", rough=0.55, bump=0.1, grime=0.45),
			pillow=mat.make("pillow_cloth", (0.58, 0.55, 0.47), pattern="cloth", rough=0.9, bump=0.3),
			pillow_end=mat.make("pillow_end", (0.47, 0.25, 0.22), pattern="knit", rough=0.9, bump=0.3),
			cap=mat.make("mushroom_cap", (0.70, 0.62, 0.50), pattern="plain", rough=0.6, bump=0.15),
			stem=mat.make("mushroom_stem", (0.80, 0.77, 0.68), pattern="plain", rough=0.7, bump=0.1),
		)
	else:
		M.update(
			hair=mat.make("hair_grey", (0.22, 0.21, 0.20), pattern="hair", rough=0.7, bump=0.35),
			top=mat.get("cloth_grey"),
			band=mat.make("cloth_grey_dark", (0.27, 0.27, 0.26), pattern="cloth", rough=0.92, bump=0.45),
			collar=mat.make("cloth_grey_dark", (0.27, 0.27, 0.26), pattern="cloth", rough=0.92, bump=0.45),
			legs=mat.make("trousers_grey", (0.27, 0.27, 0.28), pattern="cloth", rough=0.9, bump=0.45),
			shoe=mat.get("rubber"),
			pillow=mat.make("pillow_cloth", (0.58, 0.55, 0.47), pattern="cloth", rough=0.9, bump=0.3),
			pillow_fold=mat.make("pillow_fold", (0.42, 0.45, 0.47), pattern="cloth", rough=0.9, bump=0.3),
		)
	return M


# ---- pieces ------------------------------------------------------------------------------

def straw_mat(M, half_x, y0, y1):
	# The mat is kept hard: a slightly wobbly woven top and a bound edge. No hidden underside.
	top = geo.grid(half_x * 2.0, y1 - y0, 8, 20, (0, (y0 + y1) * 0.5, 0.028), mat=M["straw"], name="mat")
	geo.jitter(top, 0.004, scale=2.5, seed=3, axes=(0.3, 0.3, 1.0))
	r = 0.05
	pts = []
	for cx, cy, a0 in ((half_x - r, y1 - r, 0), (-half_x + r, y1 - r, 90), (-half_x + r, y0 + r, 180), (half_x - r, y0 + r, 270)):
		for k in range(3):
			a = math.radians(a0 + k * 45.0)
			pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r, 0.022))
	edge_a, edge_b = [], []
	for i in range(len(pts)):
		p, q = Vector(pts[i]), Vector(pts[(i + 1) % len(pts)])
		n = max(1, int((q - p).length / 0.12))
		for k in range(n):
			edge_a.append(tuple(p + (q - p) * (k / n)))
	edge_a.append(edge_a[0])
	edge = geo.tube(edge_a, 0.022, mat=M["binding"], segments=6, caps=False, name="mat_edge")
	geo.jitter(edge, 0.003, scale=3.0, seed=5)
	for o in (top, edge):
		geo.tag(o, "hips")
	return [top, edge]


def tube_body(points, radii, sx, sz, zc, m, name):
	# A swept body part along Y, wider than it is deep, its axis at height zc.
	t = geo.tube([(0, y, 0) for y in points], radii, mat=m, segments=20, name=name)
	t.scale = (sx, 1.0, sz)
	t.location = (0, 0, zc)
	geo.apply_transform(t)
	return t


def body_weights(obj, hip_y, belly_y, neck_y):
	# Along the body by Y; spine and chest only take their front half, so the back stays down on the mat.
	def front(co):
		return clamp((co.z - 0.07) / 0.12)
	def upper(co):
		return clamp((belly_y + 0.04 - co.y) / 0.08)
	def chest(co):
		return upper(co) * clamp((co.y - neck_y) / 0.04 + 0.5) * front(co)
	def spine(co):
		return clamp((hip_y + 0.05 - co.y) / 0.10) * (1.0 - upper(co)) * front(co)
	def neck(co):
		return clamp((neck_y - co.y) / 0.05)
	geo.tag_fn(obj, "chest", lambda co, n: chest(co))
	geo.tag_fn(obj, "spine", lambda co, n: spine(co))
	geo.tag_fn(obj, "neck", lambda co, n: neck(co))
	geo.tag_fn(obj, "hips", lambda co, n: clamp(1.0 - chest(co) - spine(co) - neck(co)) + 0.001)
	return obj


def frond(base, angle, length, rise, m):
	# A fern frond: an arching stem with paired leaflets shrinking to the tip.
	d = Vector((math.cos(math.radians(angle)), math.sin(math.radians(angle)), 0.0))
	side = Vector((-d.y, d.x, 0.0))
	pts = [Vector(base) + d * length * (i / 6.0) + Vector((0, 0, rise * math.sin(math.pi * (i / 6.0) * 0.8))) for i in range(7)]
	objs = [geo.tube([tuple(p) for p in pts], [0.010, 0.009, 0.008, 0.007, 0.006, 0.005, 0.004], mat=m, segments=5, name="frond")]
	for i in range(1, 7):
		s = i / 6.0
		w = 0.05 * (1.0 - s * 0.6)
		for k in (-1.0, 1.0):
			c = pts[i] + side * k * w * 0.8 - d * 0.01
			objs.append(geo.blob((w, 0.018, 0.012), tuple(c), (0, 0, angle + 90.0 + k * 12.0), mat=m, name="leaflet"))
	return objs


def blanket(profile, m, seed):
	# A thick, lumpy layer draped along the body: a flattened swept shape lifted to follow the body's top.
	ys = [y for y, _ in profile]
	radii = [0.08] + [0.12] * (len(ys) - 2) + [0.08]
	obj = geo.tube([(0, y, 0) for y in ys], radii, mat=m, segments=16, name="blanket")
	obj.scale = (2.2, 1.0, 0.42)
	geo.apply_transform(obj)
	obj = geo.subdivide(obj, 2)
	for v in obj.data.vertices:
		v.co.z += lerp_profile(profile, v.co.y) - 0.09 * min(1.0, abs(v.co.x) / 0.26) ** 3
	obj.data.update()
	obj = geo.jitter(obj, 0.03, scale=2.2, seed=seed, axes=(1.0, 0.6, 0.5))
	return geo.jitter(obj, 0.012, scale=7.0, seed=seed + 1)


def lerp_profile(profile, y):
	if y <= profile[0][0]:
		return profile[0][1]
	for (y0, z0), (y1, z1) in zip(profile, profile[1:]):
		if y0 <= y <= y1:
			return z0 + (z1 - z0) * (y - y0) / (y1 - y0)
	return profile[-1][1]


def moss_weights(obj, hip_y, belly_y, neck_y):
	# Moss lying on the body rides the chest and belly fully (it sits on their front).
	def upper(co):
		return clamp((belly_y + 0.04 - co.y) / 0.08)
	def chest(co):
		return upper(co) * clamp((co.y - neck_y) / 0.04 + 0.5)
	def spine(co):
		return clamp((hip_y + 0.05 - co.y) / 0.10) * (1.0 - upper(co))
	geo.tag_fn(obj, "chest", lambda co, n: chest(co))
	geo.tag_fn(obj, "spine", lambda co, n: spine(co))
	geo.tag_fn(obj, "hips", lambda co, n: clamp(1.0 - chest(co) - spine(co)) + 0.001)
	return obj


def mushroom(p, size, M):
	x, y, z = p
	stem = geo.cylinder(0.011 * size, 0.04 * size, (x, y, z + 0.018 * size), mat=M["stem"], segments=8, name="stem")
	cap = geo.blob((0.026 * size, 0.026 * size, 0.014 * size), (x, y, z + 0.042 * size), mat=M["cap"], name="cap")
	return [stem, cap]


# ---- the two sleepers ----------------------------------------------------------------------

def make(name, elder):
	M = materials(elder)
	hard = []
	soft = []

	def part(obj, *tags):
		geo.tag(obj, *tags)
		soft.append(obj)
		return obj

	if elder:
		hip_y, belly_y, chest_y = -0.30, -0.46, -0.58
		hard += straw_mat(M, 0.40, -1.20, 0.55)
	else:
		hip_y, belly_y, chest_y = -0.10, -0.30, -0.46
		hard += straw_mat(M, 0.43, -1.20, 0.90)
	neck_y = -0.75

	# Pillow: a folded cloth for the neighbour, grandmother's round pillow with coloured ends.
	if elder:
		part(geo.cylinder(0.068, 0.40, (0, -1.0, 0.095), (0, 90, 0), mat=M["pillow"], segments=16, name="pillow"), "hips")
		for sx in (1.0, -1.0):
			part(geo.cylinder(0.071, 0.02, (0.20 * sx, -1.0, 0.095), (0, 90, 0), mat=M["pillow_end"], segments=16, name="pillow_end"), "hips")
	else:
		part(geo.box((0.44, 0.26, 0.05), (0, -1.0, 0.052), mat=M["pillow"], bevel=0.02, name="pillow"), "hips")
		part(geo.box((0.40, 0.22, 0.04), (0.01, -1.005, 0.095), (0, 0, 2), mat=M["pillow_fold"], bevel=0.016, name="pillow_fold"), "hips")

	# Head on the pillow, face up, eyes closed.
	hc = Vector((0, -0.955, 0.215 if not elder else 0.235))
	hr = (0.125, 0.14, 0.12) if not elder else (0.122, 0.132, 0.117)
	part(geo.sphere(hr, tuple(hc), mat=M["skin"], segments=22, rings=14, name="head"), "head")
	face_z = hc.z + hr[2]
	for sx in (1.0, -1.0):
		part(geo.sphere((0.02, 0.005, 0.006), (0.046 * sx, hc.y - 0.012, face_z - 0.008), (0, 0, -8 * sx), mat=M["eye"], segments=8, rings=4, name="eye"), "head")
		part(geo.sphere((0.024, 0.034, 0.03), (hr[0] * sx, hc.y + 0.005, hc.z - 0.005), mat=M["skin"], segments=10, rings=8, name="ear"), "head")
		if elder:
			part(geo.sphere((0.028, 0.02, 0.012), (0.07 * sx, hc.y + 0.03, face_z - 0.03), mat=M["blush"], segments=8, rings=6, name="cheek"), "head")
	part(geo.sphere((0.02, 0.023, 0.021), (0, hc.y + 0.022, face_z + 0.004), mat=M["skin"], segments=10, rings=8, name="nose"), "head")
	if elder:
		part(geo.blob((0.128, 0.128, 0.125), (0, hc.y - 0.02, hc.z - 0.02), mat=M["hair"], lumpy=0.012, seed=4, name="hair"), "head")
		part(geo.blob((0.07, 0.06, 0.05), (0.10, hc.y - 0.03, 0.105), mat=M["hair"], lumpy=0.04, seed=9, name="bun"), "head")
		part(geo.cylinder(0.011, 0.20, (0.10, hc.y - 0.03, 0.12), (90, 0, 10), mat=M["pin"], segments=8, name="binyeo"), "head")
		part(geo.sphere(0.019, (0.118, hc.y - 0.13, 0.12), mat=M["pin"], segments=10, rings=8, name="binyeo_knob"), "head")
	else:
		part(geo.blob((0.132, 0.145, 0.135), (0, hc.y - 0.035, hc.z - 0.03), mat=M["hair"], lumpy=0.04, seed=2, name="hair"), "head")
	part(geo.tube([(0, -0.86, 0.17), (0, -0.74, 0.15)], 0.052, mat=M["skin"], segments=12, name="neck"), "neck")
	ring = [(0.075 * math.cos(a / 12.0 * math.tau), -0.775, 0.15 + 0.065 * math.sin(a / 12.0 * math.tau)) for a in range(13)]
	part(geo.tube(ring, 0.02, mat=M["collar"], segments=8, name="collar"), "neck")

	# Body: shoulders to hips in everyday clothes, lying a little flattened.
	if elder:
		trunk = tube_body([-0.80, -0.75, -0.68, -0.58, -0.46, -0.36, -0.26], [0.06, 0.11, 0.135, 0.145, 0.15, 0.145, 0.125], 1.3, 0.85, 0.135, M["top"], "trunk")
	else:
		trunk = tube_body([-0.80, -0.74, -0.66, -0.50, -0.34, -0.20, -0.08], [0.06, 0.11, 0.135, 0.14, 0.13, 0.13, 0.12], 1.35, 0.82, 0.135, M["top"], "trunk")
	soft.append(body_weights(trunk, hip_y, belly_y, neck_y))
	if elder:
		band = geo.tube([(0.2 * math.cos(a / 16.0 * math.tau), -0.37, 0.135 + 0.125 * math.sin(a / 16.0 * math.tau)) for a in range(17)], 0.016, mat=M["apron"], segments=6, name="apron_band")
		soft.append(body_weights(band, hip_y, belly_y, neck_y))
		apron = geo.box((0.34, 0.30, 0.02), (0, -0.20, 0.215), mat=M["apron"], bevel=0.008, name="apron")
		geo.tag(apron, "hips")
		soft.append(apron)

	# Arms at the sides on the mat, palms down.
	for sx in (1.0, -1.0):
		if elder:
			sh, el, wr, hd = (0.18, -0.73, 0.13), (0.23, -0.53, 0.09), (0.245, -0.36, 0.065), (0.25, -0.30, 0.05)
		else:
			sh, el, wr, hd = (0.19, -0.69, 0.13), (0.245, -0.43, 0.09), (0.265, -0.19, 0.07), (0.27, -0.125, 0.055)
		part(geo.tube([(sh[0] * sx, sh[1], sh[2]), (el[0] * sx, el[1], el[2])], [0.056, 0.05], mat=M["top"], segments=12, name="upper"), "hips")
		part(geo.tube([(el[0] * sx, el[1], el[2]), (wr[0] * sx, wr[1], wr[2])], [0.05, 0.046], mat=M["top"], segments=12, name="fore"), "hips")
		part(geo.tube([(wr[0] * sx, wr[1] - 0.02, wr[2] + 0.002), (wr[0] * sx, wr[1] + 0.015, wr[2])], 0.048, mat=M["band"], segments=12, name="cuff"), "hips")
		part(geo.blob((0.042, 0.06, 0.024), (hd[0] * sx, hd[1], hd[2]), mat=M["skin"], name="hand"), "hips")
		part(geo.blob((0.016, 0.026, 0.016), (hd[0] * sx - 0.035 * sx, hd[1] - 0.005, hd[2] + 0.004), mat=M["skin"], name="thumb"), "hips")
	if elder:
		# Her left hand: the blue stain on its back, moss already at the fingertips.
		part(geo.blob((0.02, 0.016, 0.006), (0.25, -0.305, 0.074), mat=M["spore"], name="stain"), "hips", "pulse")
		for i, (dx, dy) in enumerate(((-0.02, 0.058), (0.0, 0.064), (0.02, 0.058), (0.035, 0.045))):
			part(geo.blob(0.016, (0.25 + dx, -0.30 + dy, 0.056), mat=M["moss_light"], lumpy=0.3, seed=40 + i, subdivisions=2, name="tip_moss"), "hips")

	# Legs, and the feet poking out at the end, toes up.
	for sx in (1.0, -1.0):
		if elder:
			thigh = [(0.10 * sx, -0.32, 0.11), (0.11 * sx, -0.05, 0.10)]
			shin = [(0.11 * sx, -0.05, 0.10), (0.12 * sx, 0.12, 0.09), (0.12 * sx, 0.16, 0.085)]
			part(geo.tube(thigh, [0.088, 0.08], mat=M["legs"], segments=14, name="thigh"), "hips")
			part(geo.tube(shin, [0.08, 0.072, 0.05], mat=M["legs"], segments=14, name="shin"), "hips")
			part(geo.tube([(0.12 * sx, 0.15, 0.085), (0.12 * sx, 0.18, 0.085)], 0.052, mat=M["cuff"], segments=12, name="ankle"), "hips")
			part(geo.blob((0.046, 0.042, 0.088), (0.13 * sx, 0.205, 0.095), (-12, 14 * sx, 0), mat=M["shoe"], name="shoe"), "hips")
			part(geo.blob((0.016, 0.016, 0.03), (0.135 * sx, 0.222, 0.18), (-30, 14 * sx, 0), mat=M["shoe"], name="shoe_toe"), "hips")
		else:
			thigh = [(0.10 * sx, -0.14, 0.11), (0.12 * sx, 0.17, 0.10)]
			shin = [(0.12 * sx, 0.17, 0.10), (0.13 * sx, 0.53, 0.085)]
			part(geo.tube(thigh, [0.088, 0.077], mat=M["legs"], segments=14, name="thigh"), "hips")
			part(geo.tube(shin, [0.074, 0.06], mat=M["legs"], segments=14, name="shin"), "hips")
			part(geo.blob((0.05, 0.05, 0.10), (0.145 * sx, 0.585, 0.11), (-10, 16 * sx, 0), mat=M["shoe"], name="shoe"), "hips")

	# The moss blanket: a living layer following the body from chest to shins, thick cushions on it, spilling onto the mat.
	if elder:
		profile = [(-0.76, 0.24), (-0.62, 0.255), (-0.46, 0.258), (-0.32, 0.25), (-0.15, 0.215), (0.0, 0.19), (0.13, 0.16)]
	else:
		profile = [(-0.64, 0.232), (-0.50, 0.245), (-0.30, 0.235), (-0.10, 0.212), (0.15, 0.188), (0.38, 0.16), (0.50, 0.145)]
	soft.append(moss_weights(blanket(profile, M["moss"], 7), hip_y, belly_y, neck_y))
	# Clumps of moss on the blanket (they ride the breath where they lie), and lumpy spills down onto the mat.
	rnd = geo.rng(17 if elder else 5)
	tones = ["moss_light", "moss_light", "moss", "moss", "moss_dark"]
	motes = [(0.10, -0.42), (-0.14, -0.05), (0.12, 0.30)]
	if elder:
		motes = [(0.10, -0.55), (-0.15, -0.20), (0.13, 0.04)]
	y_lo, y_hi = profile[0][0] + 0.04, profile[-1][0] - 0.04
	placed = 0
	while placed < (24 if elder else 18):
		x, y = rnd.uniform(-0.21, 0.21), rnd.uniform(y_lo, y_hi)
		if any(abs(x - mx) < 0.07 and abs(y - my) < 0.07 for mx, my in motes):
			continue
		r = rnd.uniform(0.035, 0.07)
		clump = geo.blob((r, r * rnd.uniform(0.9, 1.3), r * 0.65), (x, y, lerp_profile(profile, y) + 0.035), (0, 0, rnd.uniform(0, 180)),
						 mat=M[rnd.choice(tones)], lumpy=0.35, seed=30 + placed, name="clump")
		soft.append(moss_weights(clump, hip_y, belly_y, neck_y))
		placed += 1
	if elder:
		spills = [(-0.29, -0.70, -0.14, 0.06), (0.30, -0.74, -0.44, 0.055), (0.0, 0.28, 0.46, 0.04), (-0.22, -1.10, -0.98, 0.045)]
	else:
		spills = [(-0.30, -0.32, 0.12, 0.055), (0.29, -0.02, 0.26, 0.045)]
	for i, (x, y0, y1, h) in enumerate(spills):
		n = max(2, int((y1 - y0) / 0.09))
		for k in range(n):
			y = y0 + (y1 - y0) * (k + 0.5) / n
			r = rnd.uniform(0.06, 0.09)
			part(geo.blob((r * 1.1, r, h), (x + rnd.uniform(-0.03, 0.03), y, 0.03 + h * 0.8), (0, 0, rnd.uniform(0, 180)),
						  mat=M[rnd.choice(tones[1:])], lumpy=0.4, seed=60 + i * 10 + k, name="spill"), "hips")

	# A fern rising beside the body, spore motes resting in the moss, and (for her) a few tiny mushrooms.
	fern_base = (-0.30, -0.02, 0.09) if not elder else (-0.31, -0.22, 0.09)
	for (a, ln, rise) in ((60, 0.32, 0.18), (105, 0.30, 0.22), (150, 0.28, 0.16), (195, 0.30, 0.15), (245, 0.28, 0.15), (20, 0.24, 0.13)):
		for o in frond(fern_base, a, ln, rise, M["fern"]):
			part(o, "hips", "sway", "foliage")
	for (x, y) in motes:
		mote = geo.tag(geo.sphere(0.024, (x, y, lerp_profile(profile, y) + 0.068), mat=M["spore"], segments=10, rings=8, name="mote"), "pulse")
		soft.append(moss_weights(mote, hip_y, belly_y, neck_y))
	if elder:
		for (x, y, size) in ((0.15, -0.38, 1.0), (0.19, -0.33, 0.7), (-0.17, -0.02, 0.9)):
			for o in mushroom((x, y, lerp_profile(profile, y) + 0.035), size, M):
				soft.append(moss_weights(o, hip_y, belly_y, neck_y))
		for (p, size) in (((0.30, -0.56, 0.065), 0.8), ((-0.22, -1.04, 0.06), 0.7)):
			for o in mushroom(p, size, M):
				part(o, "hips")

	res = pipeline.mixed(name, hard, soft, voxel=0.0105, tris=4300, size=512)
	low = res["mesh"]
	arm = rig.armature(name + "_rig", bones(chest_y, belly_y, hip_y))
	for b in arm.data.bones:
		b.use_deform = b.name in DEFORM
	rig.bind(low, arm)
	rig.smooth_groups(low, DEFORM, iterations=3, factor=0.5)
	_check_weights(low, name)

	res["armature"] = arm
	frames = 165 if elder else 135
	res["clips"] = [breathe(arm, frames, 0.8 if elder else 1.0)]
	res["preview_frames"] = [("breathe", 0), ("breathe", int(frames * 0.4))]
	res["preview_height"] = 2.3
	return res


def _check_weights(obj, name):
	names = {g.index: g.name for g in obj.vertex_groups}
	bare = 0
	for v in obj.data.vertices:
		if not any(names.get(ge.group) in DEFORM and ge.weight > 0.0 for ge in v.groups):
			bare += 1
	print("GSE_WEIGHTS %s unweighted=%d of %d" % (name, bare, len(obj.data.vertices)))


def _lift(arm, bone, frame, offset):
	pb = arm.pose.bones[bone]
	pb.location = pb.bone.matrix_local.to_3x3().inverted() @ Vector(offset)
	pb.keyframe_insert("location", frame=frame)


def breathe(arm, frames, depth):
	# A slow breath: the belly and chest lift (bone translation) while the chest tilts up toward the collar.
	peak = int(frames * 0.4)
	hold = int(frames * 0.5)
	out = {"spine": (0, 0, 0), "chest": (0, 0, 0), "head": (0, 0, 0)}
	inn = {"spine": (-1.0 * depth, 0, 0), "chest": (-3.0 * depth, 0, 0), "head": (-0.8 * depth, 0, 0)}
	keys = [(0, out, (0, 0, 0)), (peak, inn, (0, 0, 0)), (hold, inn, (0, 0, 0)), (frames, out, (0, 0, 0))]
	clip = rig.clip(arm, "breathe", keys)
	for f, lift_s, lift_c in ((0, 0.0, 0.0), (peak, 0.010, 0.020), (hold, 0.010, 0.020), (frames, 0.0, 0.0)):
		_lift(arm, "spine", f, (0, 0, lift_s * depth))
		_lift(arm, "chest", f, (0, 0, lift_c * depth))
	return clip


def build():
	return make("sleeper", False)
