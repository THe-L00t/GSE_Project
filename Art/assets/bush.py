# A round village shrub of clumped leaves with a few tiny pale flowers. The berry bushes reuse the
# same leaf mass (same seeds, same remesh), so a picked berry bush swaps to this without a jump.
import math

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

from gse import geo, mat, pipeline

CLUMPS = [
	((0.0, 0.0, 0.2), (0.5, 0.46, 0.23), "leaf_dark", 1),
	((0.0, 0.0, 0.38), (0.44, 0.4, 0.34), "leaf", 2),
	((0.27, -0.17, 0.34), (0.29, 0.27, 0.26), "leaf", 3),
	((-0.25, 0.15, 0.33), (0.29, 0.27, 0.25), "leaf_dark", 4),
	((-0.16, -0.27, 0.33), (0.27, 0.25, 0.24), "leaf", 5),
	((0.12, 0.25, 0.44), (0.27, 0.25, 0.23), "leaf", 6),
	((0.05, -0.04, 0.58), (0.27, 0.26, 0.19), "bush_leaf_light", 7),
]


def leaves():
	mats = {"leaf": mat.get("leaf"), "leaf_dark": mat.get("leaf_dark"),
			"bush_leaf_light": mat.make("bush_leaf_light", (0.23, 0.35, 0.21), pattern="leaf", rough=0.8, bump=0.45)}
	out = []
	for (c, r, m, seed) in CLUMPS:
		c = (c[0] * 1.07, c[1] * 1.07, c[2])
		r = (r[0] * 1.07, r[1] * 1.07, r[2])
		out.append(geo.blob(r, c, mat=mats[m], lumpy=0.22, seed=seed, name="clump"))
		# Smaller lumps on the upper face give the clumped, cauliflower read.
		rnd = geo.rng(seed + 50)
		for k in range(3):
			a = k / 3.0 * math.tau + rnd.uniform(-0.6, 0.6)
			e = rnd.uniform(0.25, 1.0)
			d = Vector((math.cos(a) * math.cos(e) * r[0], math.sin(a) * math.cos(e) * r[1], math.sin(e) * r[2])) * 0.7
			s = rnd.uniform(0.38, 0.5)
			out.append(geo.blob((r[0] * s, r[1] * s, r[2] * s), Vector(c) + d, mat=mats[m], lumpy=0.3, seed=seed * 10 + k, name="lump"))
	for b in out:
		geo.tag(b, "sway", "foliage")
	return out


def surface(objs, direction, centre=(0.0, 0.0, 0.35)):
	# The first point on the leaf mass seen from outside along a direction, and its normal.
	bpy.context.view_layer.update()
	dg = bpy.context.evaluated_depsgraph_get()
	d = Vector(direction).normalized()
	origin = Vector(centre) + d * 2.0
	best = None
	for o in objs:
		tree = BVHTree.FromObject(o, dg)
		inv = o.matrix_world.inverted()
		hit = tree.ray_cast(inv @ origin, (inv.to_3x3() @ -d).normalized())
		if hit[0] is None:
			continue
		p = o.matrix_world @ hit[0]
		n = (o.matrix_world.to_3x3().inverted().transposed() @ hit[1]).normalized()
		dist = (p - origin).length
		if best is None or dist < best[2]:
			best = (p, n, dist)
	return best[0], best[1]


def spots(count, seed, z_lo=0.15, z_hi=0.9):
	# Directions spread round the bush, biased to the upper half.
	rnd = geo.rng(seed)
	out = []
	for k in range(count):
		a = k / float(count) * math.tau + rnd.uniform(-0.35, 0.35)
		e = rnd.uniform(z_lo, z_hi)
		out.append((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e)))
	return out


def flowers(soft):
	petal = mat.make("bush_flower", (0.86, 0.84, 0.74), pattern="plain", rough=0.7, bump=0.05, grime=0.1)
	eye = mat.make("bush_flower_eye", (0.74, 0.62, 0.30), pattern="plain", rough=0.7, bump=0.05, grime=0.1)
	out = []
	for d in [(0.75, -0.55, 0.55), (-0.3, -0.8, 0.5), (0.2, 0.3, 0.95), (-0.85, 0.2, 0.45)]:
		p, n = surface(soft, d)
		q = n.to_track_quat('Z', 'Y').to_euler()
		rot = tuple(math.degrees(x) for x in q)
		f = geo.blob((0.045, 0.045, 0.016), p + n * 0.004, rot, mat=petal, subdivisions=2, name="flower")
		e = geo.blob((0.016, 0.016, 0.01), p + n * 0.016, rot, mat=eye, subdivisions=1, name="flower_eye")
		for o in (f, e):
			geo.tag(o, "sway")
			out.append(o)
	return out


def berries(soft, m, seed=11, clusters=9):
	# Clusters of three or four glossy berries pressed into the leaves.
	rnd = geo.rng(seed)
	out = []
	for d in spots(clusters, seed, 0.05, 0.85):
		p, n = surface(soft, d)
		t1 = n.cross(Vector((0.0, 0.0, 1.0)) if abs(n.z) < 0.9 else Vector((1.0, 0.0, 0.0))).normalized()
		t2 = n.cross(t1)
		for k in range(rnd.choice((3, 3, 4))):
			a = k / 3.0 * math.tau + rnd.uniform(-0.3, 0.3)
			r = rnd.uniform(0.038, 0.048)
			c = p + (t1 * math.cos(a) + t2 * math.sin(a)) * 0.05 + n * r * 0.45
			b = geo.blob(r, c, (rnd.uniform(0, 360), rnd.uniform(0, 360), 0.0), mat=m, subdivisions=1, name="berry")
			geo.tag(b, "sway")
			out.append(b)
	return out


def finish(name, soft, hard):
	return pipeline.mixed(name, hard, soft, voxel=0.018, tris=1000, size=256)


def build():
	soft = leaves()
	return finish("bush", soft, flowers(soft))
