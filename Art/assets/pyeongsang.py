# Pyeongsang: a low wooden platform bench left in the yard, a folded flower blanket and a bamboo basket
# still on it, one plank gone and grass coming up through the gap.
import math

from gse import geo, mat, pipeline


def build():
	planks = mat.make("pyeong_plank", (0.40, 0.37, 0.32), pattern="wood", rough=0.92, bump=0.4, axis='X')
	frame = mat.get("wood_dark")
	blanket = mat.make("pyeong_blanket", (0.47, 0.31, 0.32), pattern="cloth", rough=0.95, bump=0.45)
	flower = mat.make("pyeong_flower", (0.66, 0.58, 0.47), pattern="cloth", rough=0.95, bump=0.2)
	leafy = mat.make("pyeong_print_leaf", (0.30, 0.42, 0.38), pattern="cloth", rough=0.95, bump=0.2)
	weave = mat.make("pyeong_weave", (0.55, 0.46, 0.31), pattern="knit", rough=0.9, bump=0.7, var=0.2)
	kettle_mat = mat.make("pyeong_kettle", (0.62, 0.52, 0.30), pattern="rust", rough=0.55, bump=0.25, amount=0.35)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	ginkgo = mat.get("ginkgo")
	dry = mat.make("pyeong_dry_leaf", (0.48, 0.36, 0.22), pattern="leaf", rough=0.85, bump=0.3)

	parts = []
	man_made = []
	rnd = geo.rng(5)
	W, D, TOP = 1.80, 1.40, 0.45
	th = 0.04

	# Legs and the frame under the planks.
	for (x, y) in ((-0.80, -0.60), (0.80, -0.60), (-0.80, 0.60), (0.80, 0.60)):
		leg = geo.box((0.09, 0.09, TOP - th - 0.01), (x, y, (TOP - th) * 0.5), (0, 0, rnd.uniform(-4, 4)), mat=frame, bevel=0.012, bevel_segments=1, name="leg")
		parts.append(leg)
		man_made.append(leg)
	for y in (-0.60, 0.60):
		r = geo.box((1.72, 0.07, 0.08), (0, y, TOP - th - 0.04), mat=frame, bevel=0.01, bevel_segments=1, name="rail")
		parts.append(r)
		man_made.append(r)
	for x in (-0.80, 0.0, 0.80):
		r = geo.box((0.07, 1.30, 0.07), (x, 0, TOP - th - 0.035), mat=frame, bevel=0.01, bevel_segments=1, name="rail")
		parts.append(r)
		man_made.append(r)
	# A low stretcher between the legs, half sunk in grass.
	st = geo.box((1.62, 0.05, 0.05), (0, -0.60, 0.12), mat=frame, bevel=0.008, bevel_segments=1, name="stretcher")
	parts.append(st)
	man_made.append(st)

	# Planks across the short side; the fourth from the left is missing and the sixth is split and sagging.
	n = 9
	pw = W / n
	gap_x = -W * 0.5 + pw * 3.5
	for i in range(n):
		x = -W * 0.5 + pw * (i + 0.5)
		if i == 3:
			continue
		p = geo.box((pw - 0.012, D, th), (x, 0, TOP - th * 0.5), (rnd.uniform(-1.2, 1.2), 0, rnd.uniform(-0.6, 0.6)), mat=planks, bevel=0.008, bevel_segments=1, name="plank", subdiv=0)
		if i == 5:
			geo.apply_modifiers(p)
			geo.subdivide(p, 2, simple=True)
			for v in p.data.vertices:
				if v.co.y < 0.0:
					t = -v.co.y / (D * 0.5)
					v.co.z -= 0.05 * t * t
			p.data.update()
		parts.append(p)
		man_made.append(p)
	# The missing plank lies on the grass in front, one end on the stretcher.
	fallen = geo.box((pw - 0.02, D * 0.7, th), (0.15, -0.92, 0.10), (14, 0, 72), mat=planks, bevel=0.008, bevel_segments=1, name="plank_fallen")
	parts.append(fallen)
	man_made.append(fallen)

	# Folded flower blanket: three layers, the top one a little askew, a corner turned back.
	bx, by = -0.42, 0.20
	for k in range(3):
		lay = geo.box((0.62 - k * 0.01, 0.46, 0.055), (bx + rnd.uniform(-0.01, 0.01), by + k * 0.006, TOP + 0.03 + k * 0.05), (0, 0, k * 2.5),
					  mat=blanket, bevel=0.024, bevel_segments=2, name="blanket")
		parts.append(lay)
	top_z = TOP + 0.03 + 2 * 0.05 + 0.029
	# The big printed flowers of a mink blanket: five petals round a pale heart, leaves between.
	for (fx, fy, fr) in ((-0.15, -0.07, 0.075), (0.15, 0.08, 0.07), (0.20, -0.14, 0.045)):
		for k in range(5):
			a = k / 5.0 * math.tau + fx * 7.0
			parts.append(geo.cylinder(fr * 0.45, 0.006, (bx + fx + math.cos(a) * fr * 0.6, by + fy + 0.012 + math.sin(a) * fr * 0.6, top_z), mat=flower, segments=6, name="petal"))
		parts.append(geo.cylinder(fr * 0.3, 0.008, (bx + fx, by + fy + 0.012, top_z + 0.001), mat=dry, segments=6, name="heart"))
	for (fx, fy, fr) in ((0.02, 0.02, 0.05), (-0.24, 0.12, 0.04), (0.04, -0.15, 0.04), (0.25, 0.16, 0.035)):
		lp = geo.cylinder(fr, 0.006, (bx + fx, by + fy + 0.012, top_z), (0, 0, fx * 300), mat=leafy, segments=6, name="print_leaf")
		lp.scale = (1.0, 0.5, 1.0)
		parts.append(lp)
	for (fx, fz, fr) in ((-0.12, 0.0, 0.035), (0.14, 0.05, 0.04)):
		f = geo.cylinder(fr, 0.006, (bx + fx, by - 0.231, TOP + 0.07 + fz), (90, 0, 0), mat=flower, segments=8, name="print_side")
		parts.append(f)
	corner = geo.prism((0.20, 0.20, 0.03), (bx + 0.25, by - 0.17, top_z + 0.004), (0, 0, 45), mat=blanket, name="blanket_corner")
	parts.append(corner)

	# Bamboo basket, shallow and tipped on its rim, a few dry leaves still in it.
	kx, ky = 0.44, -0.18
	basket = geo.lathe([(0.13, 0.0), (0.19, 0.025), (0.26, 0.085), (0.29, 0.13), (0.275, 0.13), (0.24, 0.09), (0.17, 0.035), (0.0, 0.025)],
					   (kx, ky, TOP + 0.005), (6, -4, 0), mat=weave, segments=16, name="basket", close_bottom=True, close_top=False)
	parts.append(basket)
	man_made.append(basket)
	rim = [(kx + 0.283 * math.cos(a / 14.0 * math.tau), ky + 0.283 * math.sin(a / 14.0 * math.tau), TOP + 0.135 + 0.014 * math.sin(a / 14.0 * math.tau)) for a in range(15)]
	parts.append(geo.tube(rim, 0.014, mat=frame, segments=4, name="basket_rim"))
	for k in range(5):
		a = rnd.uniform(0, math.tau)
		d = rnd.uniform(0.0, 0.16)
		lf = geo.blob((0.05, 0.03, 0.006), (kx + math.cos(a) * d, ky + math.sin(a) * d, TOP + 0.045 + d * 0.15), (rnd.uniform(-10, 10), rnd.uniform(-10, 10), rnd.uniform(0, 180)),
					  mat=dry if k % 2 else ginkgo, subdivisions=1, name="dry_leaf")
		geo.tag(lf, "sway", "foliage")
		parts.append(lf)

	# The yellow aluminium kettle that always sat at the corner.
	ex, ey = 0.66, 0.40
	kettle = geo.lathe([(0.10, 0.0), (0.125, 0.02), (0.13, 0.06), (0.115, 0.11), (0.07, 0.135), (0.07, 0.145), (0.03, 0.15), (0.025, 0.17), (0.0, 0.172)],
					   (ex, ey, TOP), (0, 0, 0), mat=kettle_mat, segments=12, name="kettle")
	parts.append(kettle)
	man_made.append(kettle)
	sd = (-0.62, -0.78)
	parts.append(geo.tube([(ex + sd[0] * 0.10, ey + sd[1] * 0.10, TOP + 0.06), (ex + sd[0] * 0.17, ey + sd[1] * 0.17, TOP + 0.10), (ex + sd[0] * 0.21, ey + sd[1] * 0.21, TOP + 0.15)],
						  [0.022, 0.017, 0.013], mat=kettle_mat, segments=6, name="spout"))
	hp = (-sd[1], sd[0])
	parts.append(geo.tube([(ex + hp[0] * 0.075, ey + hp[1] * 0.075, TOP + 0.13), (ex + hp[0] * 0.09, ey + hp[1] * 0.09, TOP + 0.22),
						   (ex - hp[0] * 0.09, ey - hp[1] * 0.09, TOP + 0.22), (ex - hp[0] * 0.075, ey - hp[1] * 0.075, TOP + 0.13)], 0.009, mat=frame, segments=4, name="handle"))

	# Leaves and moss on the planks: moss in the seams and the shady corner, leaves blown in.
	for (loc, rad, m, seed) in (((-0.74, 0.50, TOP + 0.01), (0.14, 0.18, 0.035), moss, 1), ((0.78, -0.55, TOP + 0.01), (0.12, 0.10, 0.03), moss_dark, 2),
								((-0.25, -0.40, TOP + 0.005), (0.07, 0.20, 0.02), moss, 3), ((0.25, 0.55, TOP + 0.005), (0.10, 0.12, 0.025), moss_dark, 4),
								((-0.80, -0.62, 0.30), (0.07, 0.07, 0.16), moss, 5), ((0.80, 0.62, 0.22), (0.07, 0.07, 0.12), moss_dark, 6),
								((-0.20, 0.0, TOP - 0.005), (0.05, 0.62, 0.02), moss_dark, 7)):
		b = geo.blob(rad, loc, mat=m, lumpy=0.3, seed=seed, subdivisions=2, name="moss")
		geo.tag(b, "pulse")
		parts.append(b)
	for k in range(10):
		x = rnd.uniform(-0.85, 0.85)
		y = rnd.uniform(-0.65, 0.65)
		if abs(x - gap_x) < 0.12:
			continue
		lf = geo.blob((0.05, 0.03, 0.006), (x, y, TOP + 0.006), (0, 0, rnd.uniform(0, 180)), mat=(ginkgo, dry, leaf)[k % 3], subdivisions=1, name="leaf_litter")
		geo.tag(lf, "sway", "foliage")
		parts.append(lf)

	# Grass up through the missing plank and round the legs.
	gx = gap_x
	for k in range(8):
		hgt = rnd.uniform(0.62, 0.88)
		w = geo.cone(rnd.uniform(0.04, 0.06), hgt, (gx + rnd.uniform(-0.05, 0.05), rnd.uniform(-0.5, 0.5), hgt * 0.5),
					 (rnd.uniform(-10, 10), rnd.uniform(-10, 10), 0), mat=leaf if k % 2 else leaf_dark, segments=4, name="grass")
		geo.tag(w, "sway", "foliage")
		parts.append(w)
	for (cx, cy) in ((-0.82, -0.66), (0.82, -0.66), (-0.84, 0.64), (0.84, 0.64), (0.0, -0.70), (-0.50, 0.72), (0.95, 0.0)):
		for k in range(3):
			hgt = rnd.uniform(0.18, 0.38)
			w = geo.cone(rnd.uniform(0.03, 0.055), hgt, (cx + rnd.uniform(-0.09, 0.09), cy + rnd.uniform(-0.07, 0.07), hgt * 0.5),
						 (rnd.uniform(-18, 18), rnd.uniform(-18, 18), 0), mat=leaf if k % 2 else leaf_dark, segments=4, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 1)

	return pipeline.static("pyeongsang", parts, size=512)
