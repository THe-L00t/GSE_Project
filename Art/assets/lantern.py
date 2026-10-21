# A stone lantern: a square plinth, an octagonal base and pillar, a lotus rest, a firebox with four
# openings round a warm glowing core, a roof stone with lifted corners and a knob, moss on the roof and foot.
import math

from gse import geo, mat, pipeline


def squared(obj, lift=0.0, eave=0.0):
	# Pushes a 16-sided lathe out onto a square, lifting the eave at the corners.
	for v in obj.data.vertices:
		x, y = v.co.x, v.co.y
		r = math.hypot(x, y)
		if r < 1e-5:
			continue
		a = math.atan2(y, x)
		f = 1.0 / max(abs(math.cos(a)), abs(math.sin(a)))
		v.co.x = x * f
		v.co.y = y * f
		if lift > 0.0 and r > eave:
			c = (f - 1.0) / (math.sqrt(2.0) - 1.0)
			v.co.z += lift * c * c
	obj.data.update()
	return obj


def build():
	granite = mat.make("lantern_granite", (0.41, 0.42, 0.39), pattern="concrete", rough=0.93, bump=0.45)
	granite_dark = mat.make("lantern_granite_dark", (0.33, 0.34, 0.32), pattern="concrete", rough=0.93, bump=0.45)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	lamp = mat.get("lamp_glow")
	glow = mat.get("spore_glow")
	rnd = geo.rng(3)

	parts = []
	stone = []

	# Plinth and the octagonal lower base.
	plinth = geo.box((1.0, 1.0, 0.18), (0, 0, 0.09), mat=granite_dark, bevel=0.03, bevel_segments=2, name="plinth")
	geo.jitter(plinth, 0.012, scale=3.0, seed=1)
	stone.append(plinth)
	stone.append(geo.lathe([(0.40, 0.18), (0.41, 0.25), (0.33, 0.33), (0.30, 0.36)], (0, 0, 0), (0, 0, 22.5), mat=granite, segments=8, smooth=False, name="base"))
	stone.append(geo.lathe([(0.15, 0.36), (0.15, 1.08)], (0, 0, 0), (0, 0, 22.5), mat=granite, segments=8, smooth=False, name="pillar"))
	stone.append(geo.lathe([(0.17, 0.70), (0.17, 0.74)], (0, 0, 0), (0, 0, 22.5), mat=granite, segments=8, smooth=False, name="pillar_band"))

	# The lotus rest: an octagon flaring up, petals round its rim.
	stone.append(geo.lathe([(0.18, 1.08), (0.30, 1.16), (0.37, 1.22), (0.37, 1.27)], (0, 0, 0), (0, 0, 22.5), mat=granite, segments=8, smooth=False, name="rest"))
	for k in range(8):
		a = math.tau * k / 8.0
		stone.append(geo.blob((0.10, 0.075, 0.03), (math.cos(a) * 0.29, math.sin(a) * 0.29, 1.165), (0, -34, math.degrees(a)),
							  mat=granite, subdivisions=1, name="petal"))

	# Firebox: sill and lintel bands, four corner posts, the openings between them.
	stone.append(geo.box((0.54, 0.54, 0.06), (0, 0, 1.30), mat=granite, bevel=0.01, bevel_segments=1, name="fire_sill"))
	stone.append(geo.box((0.54, 0.54, 0.06), (0, 0, 1.59), mat=granite, bevel=0.01, bevel_segments=1, name="fire_lintel"))
	for sx in (-1.0, 1.0):
		for sy in (-1.0, 1.0):
			stone.append(geo.box((0.14, 0.14, 0.26), (sx * 0.19, sy * 0.19, 1.445), mat=granite, name="fire_post"))
	core = geo.box((0.34, 0.34, 0.24), (0, 0, 1.445), mat=lamp, name="flame_core")
	geo.tag(core, "pulse")
	parts.append(core)

	# Roof stone with lifted corners, a cap block and the knob.
	roof = geo.lathe([(0.28, 1.62), (0.50, 1.66), (0.53, 1.70), (0.52, 1.74), (0.36, 1.84), (0.16, 1.91), (0.0, 1.93)],
					 (0, 0, 0), (0, 0, 0), mat=granite, segments=16, smooth=False, name="roof")
	squared(roof, lift=0.07, eave=0.45)
	stone.append(roof)
	stone.append(geo.box((0.22, 0.22, 0.08), (0, 0, 1.95), mat=granite, bevel=0.01, bevel_segments=1, name="cap"))
	stone.append(geo.lathe([(0.07, 1.99), (0.10, 2.03), (0.095, 2.08), (0.05, 2.11), (0.02, 2.14), (0.0, 2.15)], mat=granite, segments=10, name="knob"))

	# Moss on the roof stone and the plinth, lichen on the base, ferns at the foot.
	m = geo.blob((0.36, 0.30, 0.07), (0.10, 0.06, 1.86), (0, 10, 0), mat=moss, subdivisions=2, lumpy=0.4, seed=2, name="moss_roof")
	geo.tag(m, "pulse")
	parts.append(m)
	parts.append(geo.blob((0.22, 0.10, 0.06), (0.36, -0.36, 1.73), (0, 0, -45), mat=moss_dark, subdivisions=2, lumpy=0.4, seed=3, name="moss_eave"))
	parts.append(geo.blob((0.14, 0.12, 0.05), (-0.30, 0.30, 1.74), (0, 0, 45), mat=moss, subdivisions=1, lumpy=0.4, seed=4, name="moss_eave2"))
	parts.append(geo.blob((0.20, 0.16, 0.06), (-0.22, 0.16, 1.27), mat=moss_dark, subdivisions=1, lumpy=0.4, seed=5, name="moss_rest"))
	parts.append(geo.blob((0.42, 0.34, 0.08), (-0.25, -0.22, 0.17), mat=moss, subdivisions=2, lumpy=0.4, seed=6, name="moss_plinth"))
	parts.append(geo.blob((0.55, 0.30, 0.12), (0.30, 0.48, 0.03), mat=moss_dark, subdivisions=2, lumpy=0.35, seed=7, name="moss_foot"))
	parts.append(geo.blob((0.45, 0.25, 0.10), (-0.52, -0.25, 0.02), mat=moss, subdivisions=2, lumpy=0.35, seed=8, name="moss_foot2"))
	for (cx, cy) in ((0.52, -0.40), (-0.55, 0.38), (0.45, 0.55)):
		for k in range(4):
			h = rnd.uniform(0.2, 0.38)
			w = geo.cone(rnd.uniform(0.04, 0.07), h, (cx + rnd.uniform(-0.08, 0.08), cy + rnd.uniform(-0.08, 0.08), h * 0.5),
						 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=5, name="fern")
			geo.tag(w, "sway", "foliage")
			parts.append(w)
	g = geo.blob(0.04, (-0.30, -0.18, 0.27), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(stone):
		if p.name.startswith(("plinth", "base", "pillar", "rest", "roof", "fire")):
			geo.moss_mask(p, seed=i)
	parts += stone

	return pipeline.static("lantern", parts, size=512)
