# The transmission tower on the ridge past Ginkgo Town, from the top of which the great tree first shows: a
# lattice of galvanised angle iron on four concrete footings, legs leaning in to a short mast, two cross arms with
# their insulator strings, one line snapped and hanging to the ground. The land has taken the bottom of it: a vine
# climbs one leg, moss lies on the footings and the lowest braces, and a few spores glow in it.
import math

from mathutils import Vector

from gse import geo, mat, pipeline

TOP = 10.2         # where the legs meet the mast
MAST = 12.6
BASE_HALF = 1.75
TOP_HALF = 0.42
LEVELS = (0.35, 2.3, 4.3, 6.3, 8.3, TOP)


def leg_point(sx, sy, z):
	half = BASE_HALF + (TOP_HALF - BASE_HALF) * (z / TOP)
	return (sx * half, sy * half, z)


def bar(a, b, m, r=0.045, name="brace"):
	return geo.tube([a, b], r, mat=m, segments=4, name=name)


def build():
	steel = mat.make("pylon_steel", (0.42, 0.44, 0.43), pattern="rust", rough=0.62, bump=0.3, amount=0.55)
	dark = mat.make("pylon_steel_dark", (0.30, 0.31, 0.30), pattern="rust", rough=0.66, bump=0.3, amount=0.7)
	concrete = mat.make("pylon_concrete", (0.46, 0.45, 0.42), rough=0.9, bump=0.35, grime=0.55, edge=0.25)
	insulator = mat.make("pylon_insulator", (0.52, 0.56, 0.54), rough=0.35, bump=0.05, grime=0.4, edge=0.1)
	cable = mat.make("pylon_cable", (0.14, 0.14, 0.13), rough=0.6, bump=0.0, edge=0.05)
	vine = mat.get("bark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	glow = mat.get("spore_glow")
	rnd = geo.rng(53)

	parts = []
	frame = []
	corners = ((1, 1), (-1, 1), (-1, -1), (1, -1))

	# Four legs on concrete footings, leaning in to the mast.
	for (sx, sy) in corners:
		frame.append(geo.tube([leg_point(sx, sy, 0.3), leg_point(sx, sy, TOP * 0.5), leg_point(sx, sy, TOP)], [0.085, 0.07, 0.06],
							  mat=steel, segments=4, name="leg"))
		parts.append(geo.box((0.62, 0.62, 0.42), (sx * BASE_HALF, sy * BASE_HALF, 0.15), mat=concrete, bevel=0.03, bevel_segments=1,
							 name="footing"))

	# A ring of braces at every level, and an X across each face between levels.
	for i, z in enumerate(LEVELS):
		for k in range(4):
			a = corners[k]
			b = corners[(k + 1) % 4]
			frame.append(bar(leg_point(a[0], a[1], z), leg_point(b[0], b[1], z), steel if i % 2 == 0 else dark, 0.04, "ring"))
		if i == 0:
			continue
		z0 = LEVELS[i - 1]
		for k in range(4):
			a = corners[k]
			b = corners[(k + 1) % 4]
			frame.append(bar(leg_point(a[0], a[1], z0), leg_point(b[0], b[1], z), dark, 0.032, "diagonal"))
			frame.append(bar(leg_point(b[0], b[1], z0), leg_point(a[0], a[1], z), dark, 0.032, "diagonal"))

	# The mast, and two cross arms out over where the lines ran.
	frame.append(geo.tube([(0, 0, TOP - 0.2), (0, 0, MAST)], [0.12, 0.07], mat=steel, segments=4, name="mast"))
	for (z, reach) in ((TOP - 0.6, 3.1), (TOP + 1.2, 2.2)):
		for side in (-1.0, 1.0):
			tip = (side * reach, 0.0, z)
			frame.append(bar((side * TOP_HALF, 0.0, z), tip, steel, 0.05, "arm"))
			frame.append(bar((side * TOP_HALF, 0.0, z + 0.8), tip, dark, 0.03, "arm_stay"))
			# A string of insulator discs hanging from the arm's end.
			for d in range(4):
				parts.append(geo.cylinder(0.11, 0.05, (tip[0], 0.0, z - 0.18 - 0.13 * d), mat=insulator, segments=8, name="insulator"))
			parts.append(geo.tube([(tip[0], 0.0, z), (tip[0], 0.0, z - 0.75)], 0.012, mat=cable, segments=3, name="string"))

	# One line snapped and fell: it hangs from the lower arm and lies along the ground.
	drop = [(3.1, 0.0, TOP - 1.35), (3.5, 0.25, TOP - 4.0), (3.8, 0.55, 3.0), (4.1, 0.9, 0.6), (5.2, 1.6, 0.04), (6.6, 2.1, 0.03)]
	parts.append(geo.tube(drop, 0.025, mat=cable, segments=4, name="fallen_line"))

	# A vine climbs the front leg, leafing out as it goes.
	pts = []
	n = 14
	for k in range(n + 1):
		t = k / n
		z = 0.3 + 7.2 * t
		lp = Vector(leg_point(1, -1, z))
		a = 4.0 * math.pi * t
		pts.append(tuple(lp + Vector((math.cos(a) * 0.12, math.sin(a) * 0.12, 0.0))))
	parts.append(geo.tube(pts, [0.05 * (1.0 - 0.7 * k / n) for k in range(n + 1)], mat=vine, segments=4, name="vine"))
	for k in range(1, n + 1):
		t = k / n
		p = Vector(pts[k])
		size = rnd.uniform(0.28, 0.45) * (1.0 - 0.45 * t)
		c = geo.blob((size, size * 0.8, size * 0.9), tuple(p + Vector((rnd.uniform(-0.15, 0.15), rnd.uniform(-0.15, 0.15), 0.0))),
					 mat=leaf if rnd.random() < 0.6 else leaf_dark, subdivisions=1, lumpy=0.4, seed=k + 5, name="vine_leaves")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	# Moss over the footings and on the lowest braces; grass and a few glowing spores round the feet.
	for i, (sx, sy) in enumerate(corners):
		pad = geo.blob((0.42, 0.40, 0.14), (sx * BASE_HALF, sy * BASE_HALF, 0.36), mat=moss if i % 2 == 0 else moss_dark,
					   subdivisions=2, lumpy=0.35, seed=i + 11, name="moss")
		geo.tag(pad, "pulse")
		parts.append(pad)
		for j in range(4):
			h = rnd.uniform(0.25, 0.5)
			a = rnd.uniform(0.0, 2.0 * math.pi)
			rr = rnd.uniform(0.4, 0.7)
			g = geo.cone(rnd.uniform(0.04, 0.06), h, (sx * BASE_HALF + math.cos(a) * rr, sy * BASE_HALF + math.sin(a) * rr, h * 0.5),
						 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=4, name="grass")
			geo.tag(g, "sway", "foliage")
			parts.append(g)
	for k in range(2):
		a = corners[k]
		parts.append(geo.blob((0.5, 0.12, 0.08), (a[0] * BASE_HALF * 0.86, a[1] * BASE_HALF * 0.86, 0.48),
							  mat=moss_dark, subdivisions=1, lumpy=0.3, seed=k + 21, name="brace_moss"))
	for k in range(5):
		a = rnd.uniform(0.0, 2.0 * math.pi)
		g = geo.blob(0.05, (math.cos(a) * 1.9, math.sin(a) * 1.9, rnd.uniform(0.35, 1.2)), mat=glow, subdivisions=1, name="spore")
		geo.tag(g, "pulse")
		parts.append(g)

	for i, p in enumerate(frame):
		geo.moss_mask(p, seed=i + 2)
	parts += frame

	res = pipeline.static("pylon", parts, size=512)
	res["preview_views"] = (45.0, 135.0)
	return res
