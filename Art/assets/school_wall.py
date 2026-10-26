# The old school wall at the gate of Ginkgo Town, where Hayul's mother left a few words in chalk: a 6 m cement-block
# wall on a footing, its east end fallen in, the school's name painted along the top long ago and faded, and on
# the front in white chalk "하율아", "약은 도시 쪽", "송전탑" with an arrow to a drawn tree. Ivy over the broken end,
# moss on the cap and at the foot. The front faces the camera.
import math

from mathutils import Vector

from assets.block_wall import blocks, loose_block, plate, wall_slab
from assets.ruin import flat_text
from gse import geo, mat, pipeline

L = 6.0
Z0 = 0.14
COURSE = 0.2
TOP = Z0 + 8 * COURSE
OUTLINE = [(-3.0, Z0), (3.0, Z0), (3.0, 0.94), (2.8, 0.94), (2.8, 1.14), (2.6, 1.14), (2.6, 1.34), (2.4, 1.34), (2.4, 1.54),
		   (2.2, 1.54), (2.2, TOP), (-3.0, TOP)]
CORES = [(2.9, 0.94), (2.7, 1.14), (2.5, 1.34), (2.3, 1.54)]
FRONT = -0.1035


def stroke(x0, z0, x1, z1, m, w=0.028):
	# One chalk line on the front face, from (x0, z0) to (x1, z1).
	length = math.hypot(x1 - x0, z1 - z0)
	angle = math.degrees(math.atan2(z1 - z0, x1 - x0))
	return plate(length + w, w, ((x0 + x1) * 0.5, FRONT, (z0 + z1) * 0.5), (90, -angle, 0), m, "chalk")


def build():
	block = blocks("school_wall_block", (0.50, 0.50, 0.47), z0=Z0)
	loose = mat.make("school_wall_loose", (0.50, 0.50, 0.47), pattern="concrete", rough=0.93, bump=0.35)
	footing_m = mat.make("school_wall_footing", (0.40, 0.40, 0.38), pattern="concrete", rough=0.93, bump=0.4)
	cap_m = mat.make("school_wall_cap", (0.55, 0.54, 0.50), pattern="concrete", rough=0.92, bump=0.4)
	hole = mat.make("school_wall_core", (0.08, 0.075, 0.07), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	paint = mat.make("school_wall_paint", (0.34, 0.52, 0.60), pattern="plaster", rough=0.9, bump=0.05, edge=0.0)
	chalk = mat.make("school_wall_chalk", (0.90, 0.90, 0.85), pattern="plain", rough=0.98, var=0.08, grime=0.15, edge=0.0, bump=0.0)
	vine = mat.make("school_wall_vine", (0.22, 0.25, 0.17), pattern="bark", rough=0.9, bump=0.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	grass = mat.make("school_wall_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	glow = mat.get("spore_glow")
	rnd = geo.rng(52)

	parts = []
	man_made = []

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			man_made.append(o)
		return o

	add(geo.box((L, 0.30, Z0), (0, 0, Z0 * 0.5), mat=footing_m, bevel=0.015, bevel_segments=1, name="footing"))
	add(wall_slab(OUTLINE, 0.2, 0.1, block))
	for (x, z) in CORES:
		parts.append(plate(0.12, 0.11, (x, 0.0, z + 0.002), (0, 0, 0), hole, "core"))
	cap = geo.box((5.2, 0.24, 0.06), (-0.4, 0, TOP + 0.03), mat=cap_m, subdiv=1, name="cap")
	geo.jitter(cap, 0.008, 3.0, seed=1, axes=(0.0, 1.0, 1.0))
	add(cap)

	# What fell from the broken end lies at its foot.
	for (loc, rot, length) in (((2.55, -0.42, 0.085), (4, -3, 22), 0.39), ((2.95, -0.36, 0.10), (90, 0, -18), 0.39),
							   ((2.75, 0.38, 0.09), (-5, 2, -12), 0.39)):
		ps = loose_block(Vector(loc), rot, loose, hole, length)
		parts += ps
		man_made.append(ps[0])

	# The school's name, painted along the top long ago and faded.
	parts.append(geo.decimate(flat_text("은행초등학교", 0.26, (-1.55, FRONT + 0.0005, 1.47), (90, 0, 0), paint), 600))

	# Her chalk: Hayul's name, where the medicine was said to be, and the way: the pylon, an arrow, the tree.
	for (body, size, x, z, tilt) in (("하율아", 0.32, -1.80, 1.06, 3), ("약은 도시 쪽", 0.21, -1.62, 0.74, 2), ("송전탑", 0.21, -1.95, 0.42, -2)):
		parts.append(geo.decimate(flat_text(body, size, (x, FRONT, z), (90, -tilt, 0), chalk, angle=12.0), 400))
	parts.append(stroke(-1.55, 0.44, -0.50, 0.50, chalk, 0.034))
	parts.append(stroke(-0.68, 0.61, -0.50, 0.50, chalk, 0.034))
	parts.append(stroke(-0.68, 0.40, -0.50, 0.50, chalk, 0.034))
	# The tree she drew: a round crown of short strokes on a trunk.
	cx, cz, r = 0.10, 0.86, 0.32
	for k in range(11):
		a0 = k / 11.0 * math.tau
		a1 = (k + 1) / 11.0 * math.tau
		rr0 = r * (1.0 + 0.12 * math.sin(k * 2.3))
		rr1 = r * (1.0 + 0.12 * math.sin((k + 1) * 2.3))
		parts.append(stroke(cx + math.cos(a0) * rr0, cz + math.sin(a0) * rr0 * 0.85, cx + math.cos(a1) * rr1, cz + math.sin(a1) * rr1 * 0.85, chalk, 0.032))
	parts.append(stroke(cx - 0.05, 0.60, cx - 0.08, 0.28, chalk, 0.032))
	parts.append(stroke(cx + 0.05, 0.60, cx + 0.09, 0.28, chalk, 0.032))
	parts.append(stroke(cx - 0.26, 0.27, cx + 0.28, 0.28, chalk, 0.032))

	# Ivy over the broken east end, a runner over the top onto the back.
	stems = [
		([(2.65, -0.115, 0.14), (2.55, -0.115, 0.50), (2.70, -0.115, 0.85), (2.52, -0.115, 1.20), (2.30, -0.11, 1.56),
		  (2.25, 0.0, 1.62), (2.28, 0.115, 1.40)], 0.022),
		([(2.60, -0.115, 0.55), (2.85, -0.115, 0.78), (2.95, -0.115, 0.92)], 0.014),
	]
	for (pts, rad) in stems:
		parts.append(geo.tube(pts, [rad] * (len(pts) - 1) + [rad * 0.5], mat=vine, segments=4, name="ivy"))
		for k in range(1, len(pts)):
			p0, p1 = pts[k - 1], pts[k]
			for j in range(2):
				t = (j + 0.5) / 2.0
				y = p0[1] + (p1[1] - p0[1]) * t
				face_y = 0.13 if y > 0.05 else (-0.13 if y < -0.05 else y)
				cc = (p0[0] + (p1[0] - p0[0]) * t + rnd.uniform(-0.08, 0.08), face_y, p0[2] + (p1[2] - p0[2]) * t + rnd.uniform(-0.05, 0.05))
				lf = geo.blob((0.08, 0.016, 0.07), cc, (rnd.uniform(-15, 15), 0, rnd.uniform(-30, 30)) if abs(face_y) > 0.1 else (90, 0, rnd.uniform(0, 90)),
							  mat=leaf if (k + j) % 2 else leaf_dark, subdivisions=1, name="ivy_leaf")
				geo.tag(lf, "sway", "foliage")
				parts.append(lf)
	for (loc, radii, m, seed) in (((2.62, -0.14, 0.40), (0.26, 0.05, 0.30), leaf_dark, 1), ((2.66, -0.14, 0.92), (0.22, 0.05, 0.24), leaf, 2)):
		b = geo.blob(radii, loc, mat=m, subdivisions=2, lumpy=0.3, seed=seed, name="ivy_mass")
		geo.tag(b, "sway", "foliage")
		parts.append(b)

	# Moss on the cap and creeping up from the footing.
	for (loc, radii, m, seed) in (((-2.55, 0.0, TOP + 0.07), (0.34, 0.12, 0.05), moss, 1), ((-0.90, 0.02, TOP + 0.07), (0.22, 0.10, 0.04), moss_dark, 2),
								  ((1.70, 0.0, TOP + 0.07), (0.28, 0.11, 0.05), moss, 3)):
		b = geo.blob(radii, loc, mat=m, subdivisions=2, lumpy=0.35, seed=seed, name="moss_top")
		if seed == 1:
			geo.tag(b, "pulse")
		parts.append(b)
	for (loc, radii, m, seed) in (((-2.40, -0.17, 0.05), (0.42, 0.14, 0.16), moss, 6), ((1.30, -0.17, 0.05), (0.36, 0.13, 0.15), moss_dark, 7),
								  ((-0.60, 0.17, 0.05), (0.40, 0.14, 0.17), moss_dark, 8)):
		parts.append(geo.blob(radii, loc, mat=m, subdivisions=2, lumpy=0.35, seed=seed, name="moss_base"))

	for (cx2, cy, n) in ((-2.85, -0.20, 5), (-1.20, -0.20, 4), (0.70, -0.20, 5), (2.20, -0.45, 5), (-2.0, 0.20, 5), (1.0, 0.20, 5)):
		for k in range(n):
			hgt = rnd.uniform(0.16, 0.40)
			a = math.tau * k / n + rnd.uniform(-0.4, 0.4)
			lean = rnd.uniform(10, 28)
			w = geo.cone(rnd.uniform(0.024, 0.036), hgt, (cx2 + math.cos(a) * 0.03, cy + math.sin(a) * 0.02, hgt * 0.45),
						 (-math.sin(a) * lean, math.cos(a) * lean, 0), mat=(grass, leaf)[k % 2], segments=3, name="grass")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	g = geo.blob(0.04, (1.10, -0.05, 1.20), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 5)

	res = pipeline.static("school_wall", parts, size=1024)
	res["preview_views"] = (45.0, 225.0)
	return res
