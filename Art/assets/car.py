# An abandoned compact sedan, faded blue, one tyre gone flat, moss on the roof and weeds at the wheels.
from gse import geo, mat, pipeline


def build():
	paint = mat.get("paint_blue")
	metal = mat.get("metal")
	glass = mat.get("glass")
	rubber = mat.get("rubber")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	glow = mat.get("spore_glow")
	red = mat.get("paint_red")
	bone = mat.get("bone")

	parts = []
	body = geo.box((1.72, 3.80, 0.58), (0, 0, 0.56), mat=paint, bevel=0.07, bevel_segments=3, name="body")
	parts.append(body)
	hood = geo.box((1.62, 1.05, 0.10), (0, -1.30, 0.86), (-3, 0, 0), mat=paint, bevel=0.03, name="hood")
	trunk = geo.box((1.62, 0.70, 0.10), (0, 1.52, 0.86), (2, 0, 0), mat=paint, bevel=0.03, name="trunk")
	parts += [hood, trunk]

	# Cabin profile seen from the side, swept across the car's width.
	cabin = geo.polygon_slab([(-0.78, 0.0), (0.98, 0.0), (0.80, 0.52), (-0.36, 0.52)], 1.50,
							 loc=(-0.75, 0.0, 0.84), rot=(90, 0, 90), mat=glass, name="cabin")
	roof = geo.box((1.52, 1.12, 0.07), (0, 0.22, 1.39), mat=paint, bevel=0.03, name="roof")
	parts += [cabin, roof]
	# Pillars follow the cabin's slopes up to the roof edge.
	for x in (-0.76, 0.76):
		parts.append(geo.tube([(x, -0.78, 0.86), (x, -0.36, 1.37)], 0.035, mat=paint, segments=6, name="a_pillar"))
		parts.append(geo.tube([(x, 0.98, 0.86), (x, 0.80, 1.37)], 0.05, mat=paint, segments=6, name="c_pillar"))
		parts.append(geo.tube([(x, 0.22, 0.86), (x, 0.22, 1.37)], 0.03, mat=paint, segments=6, name="b_pillar"))
		parts.append(geo.box((0.02, 0.02, 0.40), (x * 1.135, 0.24, 0.60), mat=metal, name="door_seam"))

	parts.append(geo.box((1.80, 0.16, 0.16), (0, -1.94, 0.40), mat=metal, bevel=0.03, name="bumper_f"))
	parts.append(geo.box((1.80, 0.16, 0.16), (0, 1.94, 0.40), mat=metal, bevel=0.03, name="bumper_r"))
	parts.append(geo.box((1.10, 0.04, 0.16), (0, -1.91, 0.64), mat=metal, name="grille"))
	for x in (-0.62, 0.62):
		parts.append(geo.cylinder(0.12, 0.05, (x, -1.91, 0.66), (90, 0, 0), mat=bone, segments=12, name="headlight"))
		parts.append(geo.box((0.30, 0.04, 0.12), (x, 1.91, 0.66), mat=red, name="taillight"))

	# Wheels; the front left is flat, so the car leans.
	for (x, y, flat) in ((-0.86, -1.20, True), (0.86, -1.20, False), (-0.86, 1.22, False), (0.86, 1.22, False)):
		r = 0.30
		z = 0.24 if flat else r
		tyre = geo.cylinder(r, 0.24, (x, y, z), (0, 90, 0), mat=rubber, segments=16, name="tyre")
		if flat:
			tyre.scale = (0.75, 1.0, 1.0)
		parts.append(tyre)
		parts.append(geo.cylinder(r * 0.55, 0.26, (x, y, z), (0, 90, 0), mat=metal, segments=12, name="rim"))

	# Rust bloom along the sills.
	for x in (-0.87, 0.87):
		parts.append(geo.box((0.03, 2.2, 0.12), (x, 0.0, 0.32), mat=mat.get("rust"), name="sill_rust"))

	# Moss: a thick cushion on the roof, more on the hood and spilling down one side.
	m1 = geo.blob((0.62, 0.52, 0.12), (0.08, 0.20, 1.44), mat=moss, lumpy=0.25, seed=1, name="moss_roof")
	m2 = geo.blob((0.48, 0.40, 0.08), (-0.25, -1.20, 0.92), mat=moss_dark, lumpy=0.3, seed=2, name="moss_hood")
	m3 = geo.blob((0.10, 0.80, 0.22), (0.86, 0.50, 0.70), mat=moss, lumpy=0.3, seed=3, name="moss_side")
	m4 = geo.blob((0.35, 0.30, 0.06), (0.30, 1.50, 0.92), mat=moss_dark, lumpy=0.3, seed=4, name="moss_trunk")
	for m in (m1, m2, m3, m4):
		geo.tag(m, "pulse")
	parts += [m1, m2, m3, m4]

	# Weeds pushing up round the wheels and under the bumper.
	rnd = geo.rng(7)
	for (cx, cy) in ((-0.95, -1.2), (0.95, 1.25), (-0.7, 1.9), (0.5, -2.0), (-0.95, 0.4)):
		for k in range(4):
			h = rnd.uniform(0.25, 0.5)
			w = geo.cone(rnd.uniform(0.05, 0.09), h, (cx + rnd.uniform(-0.2, 0.2), cy + rnd.uniform(-0.2, 0.2), h * 0.5),
						 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=5, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	g = geo.blob(0.07, (-0.30, 0.45, 1.52), mat=glow, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for p in parts:
		if p.name.startswith(("body", "hood", "trunk", "roof")):
			geo.moss_mask(p, seed=3)

	result = pipeline.static("car", parts, size=512)
	return result
