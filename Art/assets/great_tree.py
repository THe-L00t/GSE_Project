# The great tree at the heart of the sleeping city, which breathes the spores in: a vast fissured trunk on
# buttress roots that spread across the ground like held hands, great limbs under a broad dome of round clumps
# a little bluer than any other tree, and at the ends of its branches the lights the deer bring, opened into
# flowers: spore teal, and here and there one that is warm. Moss carpets its feet.
# About 17 m tall and 20 m across. Only the low grass sways; the canopy is too high for the shader's sway.
import math

from mathutils import Vector

from assets.ginkgo import carpet, clump
from gse import geo, mat, pipeline


def build():
	bark = mat.make("great_bark", (0.22, 0.19, 0.17), pattern="bark", rough=0.95, bump=0.85)
	bark_moss = mat.make("great_bark_moss", (0.18, 0.25, 0.19), pattern="moss", rough=0.95, bump=0.7)
	leaf_deep = mat.make("great_leaf_deep", (0.11, 0.21, 0.18), pattern="leaf", rough=0.82, bump=0.5)
	leaf = mat.make("great_leaf", (0.16, 0.29, 0.24), pattern="leaf", rough=0.8, bump=0.45)
	leaf_light = mat.make("great_leaf_light", (0.24, 0.39, 0.30), pattern="leaf", rough=0.78, bump=0.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	grass = mat.get("leaf")
	glow = mat.get("spore_glow")
	warm = mat.get("lamp_glow")
	rnd = geo.rng(97)

	wood = []

	# The trunk, flared hard at the foot, fissured and mossy on its shaded side.
	wood.append(geo.lathe([(2.4, -0.1), (2.0, 0.3), (1.65, 1.0), (1.42, 2.2), (1.3, 3.8), (1.18, 5.5), (1.02, 7.2), (0.84, 8.8), (0.6, 10.0)],
						  mat=bark, segments=26, name="trunk"))
	wood.append(geo.blob((0.75, 0.5, 2.6), (-0.75, 1.05, 2.6), mat=bark_moss, lumpy=0.3, seed=3, name="trunk_moss"))
	wood.append(geo.blob((0.6, 0.45, 1.6), (0.9, 0.85, 5.4), mat=bark_moss, lumpy=0.3, seed=4, name="trunk_moss"))

	# Buttress roots: a tall fin against the trunk that runs out low, arching a little before it dives.
	for k in range(9):
		a = k / 9.0 * math.tau + rnd.uniform(-0.18, 0.18)
		reach = rnd.uniform(4.6, 6.6)
		lift = rnd.uniform(0.25, 0.6)
		d = Vector((math.cos(a), math.sin(a), 0.0))
		side = Vector((-d.y, d.x, 0.0)) * rnd.uniform(-0.6, 0.6)
		pts = [Vector((0, 0, 3.0)), d * 1.9 + Vector((0, 0, 1.1)), d * (reach * 0.55) + side * 0.5 + Vector((0, 0, lift + 0.15)),
			   d * reach + side + Vector((0, 0, -0.2))]
		wood.append(geo.tube([tuple(p) for p in pts], [0.95, 0.7, 0.45, 0.22], mat=bark, segments=10, name="root"))
		wood.append(geo.blob((1.5, 0.32, 1.1), tuple(d * 2.1 + Vector((0, 0, 0.55))), (0, 0, math.degrees(a)), mat=bark, lumpy=0.15, seed=k + 20,
							 name="root_fin"))
		if k % 3 == 0:
			wood.append(geo.blob((0.8, 0.45, 0.18), tuple(d * (reach * 0.55) + side * 0.5 + Vector((0, 0, lift + 0.15))),
								 mat=moss_dark, lumpy=0.35, seed=k + 10, name="root_moss"))

	# Great limbs sweep out from high on the trunk under the dome, each with a branch on toward its rim.
	for k in range(7):
		a = k / 7.0 * math.tau + 0.4 + rnd.uniform(-0.2, 0.2)
		d = Vector((math.cos(a), math.sin(a), 0.0))
		z0 = rnd.uniform(6.6, 8.8)
		out = rnd.uniform(4.8, 6.4)
		up = rnd.uniform(3.0, 4.6)
		pts = [Vector((0, 0, z0)), d * (out * 0.4) + Vector((0, 0, z0 + up * 0.55)), d * out + Vector((0, 0, z0 + up))]
		wood.append(geo.tube([tuple(p) for p in pts], [0.55, 0.4, 0.22], mat=bark, segments=10, name="limb"))
		twig = pts[1] + (pts[2] - pts[1]) * 0.6
		turn = Vector((-d.y, d.x, 0.0)) * rnd.uniform(-1.6, 1.6)
		wood.append(geo.tube([tuple(twig), tuple(twig + d * 1.6 + turn + Vector((0, 0, 1.4)))], [0.2, 0.08], mat=bark, segments=8,
							 name="branch"))
	wood.append(geo.tube([(0, 0, 9.4), (0.2, -0.1, 12.0), (0.0, 0.0, 14.6)], [0.45, 0.3, 0.15], mat=bark, segments=10, name="leader"))

	# The dome: a ring of clumps low and wide, a ring above it, a crown on top, all a little bluer than a
	# common tree's.
	leaves = [geo.blob((6.2, 6.0, 3.0), (0.0, 0.0, 13.2), mat=leaf_deep, lumpy=0.12, seed=40, name="core")]
	spec = []
	for k in range(9):
		a = k / 9.0 * math.tau + 0.2
		r = 6.4 + 0.5 * (k % 2)
		spec.append(((math.cos(a) * r, math.sin(a) * r, 11.4 + 0.4 * (k % 3)), (2.7, 2.6, 2.0), leaf_deep, leaf, 41 + k))
	for k in range(7):
		a = k / 7.0 * math.tau + 0.65
		spec.append(((math.cos(a) * 3.9, math.sin(a) * 3.9, 14.0 + 0.3 * (k % 2)), (2.6, 2.5, 2.1), leaf, leaf_light, 51 + k))
	for k in range(3):
		a = k / 3.0 * math.tau + 0.1
		spec.append(((math.cos(a) * 1.4, math.sin(a) * 1.4, 15.6), (2.2, 2.1, 1.7), leaf_light, leaf_light, 61 + k))
	for (c, r, m, top, seed) in spec:
		leaves += clump(c, r, m, seed, 4, top)

	hard = []

	# The lights the deer bring, opened into flowers at the rim of the dome: spore teal, a few warm.
	for k in range(42):
		a = rnd.uniform(0.0, math.tau)
		tier = rnd.random()
		if tier < 0.6:
			r, z = rnd.uniform(7.4, 9.0), rnd.uniform(10.2, 12.6)
		else:
			r, z = rnd.uniform(4.8, 6.2), rnd.uniform(13.2, 15.6)
		p = (math.cos(a) * r, math.sin(a) * r, z)
		g = geo.sphere(rnd.uniform(0.13, 0.22), p, mat=warm if k % 9 == 4 else glow, segments=6, rings=4, name="blossom")
		geo.tag(g, "pulse")
		hard.append(g)

	# Its feet in a moss carpet, grass along the roots, and spores glowing low among them.
	hard.append(carpet(6.4, (moss, moss_dark), seed=5, rings=8, segments=44, piles=7, wobble=0.25))
	for k in range(26):
		a = rnd.uniform(0.0, math.tau)
		rr = rnd.uniform(2.2, 6.0)
		h = rnd.uniform(0.25, 0.55)
		g = geo.cone(rnd.uniform(0.05, 0.08), h, (math.cos(a) * rr, math.sin(a) * rr, h * 0.5),
					 (rnd.uniform(-18, 18), rnd.uniform(-18, 18), 0), mat=grass, segments=4, name="grass")
		geo.tag(g, "sway", "foliage")
		hard.append(g)
	for k in range(8):
		a = rnd.uniform(0.0, math.tau)
		rr = rnd.uniform(2.0, 5.0)
		g = geo.sphere(0.07, (math.cos(a) * rr, math.sin(a) * rr, rnd.uniform(0.4, 1.6)), mat=glow, segments=6, rings=4, name="spore")
		geo.tag(g, "pulse")
		hard.append(g)

	res = pipeline.mixed("great_tree", hard, wood + leaves, voxel=0.09, tris=6500, size=1024)
	res["preview_views"] = (45.0, 225.0)
	return res
