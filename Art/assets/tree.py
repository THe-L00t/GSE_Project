# A village zelkova: a short flared trunk splits into limbs under a canopy of chunky leaf clumps,
# built like a model-railroad tree. Two spore motes drift under the leaves.
import math

from mathutils import Vector

from gse import geo, mat, pipeline


def clump(center, radii, m, seed, subs=3, top=None, lumpy=0.2):
	# One clump: a lumpy mass with smaller lumps on its upper and outer face.
	rnd = geo.rng(seed)
	c = Vector(center)
	rx, ry, rz = radii
	out = [geo.blob(radii, center, mat=m, lumpy=lumpy, seed=seed, name="clump")]
	for k in range(subs):
		a = rnd.uniform(0.0, math.tau)
		e = rnd.uniform(0.2, 1.1)
		d = Vector((math.cos(a) * math.cos(e) * rx, math.sin(a) * math.cos(e) * ry, math.sin(e) * rz)) * 0.72
		s = rnd.uniform(0.4, 0.55)
		out.append(geo.blob((rx * s, ry * s, rz * s * 0.9), c + d, mat=top or m, lumpy=0.25, seed=seed * 10 + k, name="lump"))
	return out


def build():
	bark = mat.get("bark")
	moss = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	leaf_light = mat.make("tree_leaf_light", (0.23, 0.35, 0.21), pattern="leaf", rough=0.8, bump=0.45)
	glow = mat.get("spore_glow")

	wood = []
	trunk = geo.lathe([(0.36, -0.05), (0.30, 0.12), (0.24, 0.35), (0.215, 0.7), (0.205, 1.2), (0.19, 1.6), (0.15, 1.85)],
					  mat=bark, segments=14, name="trunk")
	wood.append(trunk)
	rnd = geo.rng(5)
	for k in range(5):
		a = k / 5.0 * math.tau + rnd.uniform(-0.3, 0.3)
		r = rnd.uniform(0.55, 0.72)
		wood.append(geo.tube([(0.0, 0.0, 0.42), (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.12), (math.cos(a) * r, math.sin(a) * r, -0.04)],
							 [0.13, 0.09, 0.04], mat=bark, segments=8, name="root"))
	# Zelkova limbs fan out from low on the trunk, a vase under the crown.
	limbs = [
		[(0.0, 0.0, 1.45), (0.5, 0.12, 2.25), (1.1, 0.3, 2.95)],
		[(0.0, 0.0, 1.5), (-0.48, 0.22, 2.3), (-1.1, 0.42, 3.0)],
		[(0.0, 0.0, 1.45), (0.18, -0.5, 2.25), (0.4, -1.1, 2.9)],
		[(0.0, 0.0, 1.55), (-0.22, 0.5, 2.4), (-0.4, 1.1, 3.1)],
		[(0.0, 0.0, 1.6), (0.06, -0.02, 2.7), (0.0, 0.0, 4.3)],
		[(0.0, 0.0, 1.3), (-0.5, -0.45, 2.15), (-0.95, -0.9, 2.75)],
	]
	for pts in limbs:
		wood.append(geo.tube(pts, [0.13, 0.1, 0.08], mat=bark, segments=10, name="limb"))
	wood.append(geo.tube([(0.3, 0.08, 2.0), (0.75, -0.5, 2.45), (1.0, -0.85, 2.7)], [0.07, 0.055, 0.045], mat=bark, segments=8, name="twig"))
	wood.append(geo.blob((0.17, 0.1, 0.42), (-0.06, 0.17, 0.62), mat=moss, lumpy=0.3, seed=3, name="trunk_moss"))
	wood.append(geo.blob((0.3, 0.22, 0.07), (-0.25, 0.32, 0.03), mat=moss, lumpy=0.3, seed=4, name="root_moss"))

	leaves = []
	spec = [
		((1.2, 0.3, 3.15), (0.95, 0.9, 0.7), leaf_dark, None, 11),
		((-1.15, 0.45, 3.25), (0.95, 0.9, 0.7), leaf_dark, None, 12),
		((0.45, -1.15, 3.0), (0.9, 0.85, 0.68), leaf_dark, None, 13),
		((-0.4, 1.2, 3.35), (0.9, 0.85, 0.7), leaf_dark, None, 14),
		((-1.0, -0.95, 2.9), (0.72, 0.68, 0.52), leaf_dark, leaf, 19),
		((0.7, -0.3, 4.0), (0.95, 0.9, 0.72), leaf, leaf_light, 15),
		((-0.7, -0.15, 4.1), (0.9, 0.85, 0.7), leaf, leaf_light, 16),
		((0.25, 0.8, 4.15), (0.9, 0.85, 0.72), leaf, leaf_light, 17),
		((0.0, 0.0, 4.92), (0.9, 0.85, 0.62), leaf, leaf_light, 18),
		((-0.35, -0.55, 3.6), (0.8, 0.75, 0.62), leaf_dark, leaf, 20),
	]
	for (c, r, m, top, seed) in spec:
		leaves += clump(c, r, m, seed, 3, top)
	for p in leaves:
		geo.tag(p, "sway", "foliage")

	motes = []
	for (p, r) in (((1.3, -0.75, 2.45), 0.07), ((-1.2, 1.05, 2.6), 0.06)):
		g = geo.blob(r, p, mat=glow, subdivisions=2, name="mote")
		geo.tag(g, "pulse", "sway")
		motes.append(g)

	return pipeline.mixed("tree", motes, wood + leaves, voxel=0.035, tris=3700, size=512)
