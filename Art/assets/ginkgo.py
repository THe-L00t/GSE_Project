# The great ginkgo at the town gate: a massive flared trunk with a faded straw rope (geumjul) tied
# round it, rising limbs under a broad golden oval crown, and a thin carpet of fallen leaves.
# Gold all year as the town's emblem, so the leaves sway but are not seasonal foliage.
import math

import bmesh
import bpy
from mathutils import Vector, noise

from gse import core, geo, mat, pipeline


def clump(center, radii, m, seed, subs=3, top=None):
	rnd = geo.rng(seed)
	c = Vector(center)
	rx, ry, rz = radii
	out = [geo.blob(radii, center, mat=m, lumpy=0.2, seed=seed, name="clump")]
	for k in range(subs):
		a = rnd.uniform(0.0, math.tau)
		e = rnd.uniform(0.2, 1.1)
		d = Vector((math.cos(a) * math.cos(e) * rx, math.sin(a) * math.cos(e) * ry, math.sin(e) * rz)) * 0.72
		s = rnd.uniform(0.34, 0.5)
		out.append(geo.blob((rx * s, ry * s, rz * s * 0.9), c + d, mat=top or m, lumpy=0.3, seed=seed * 10 + k, name="lump"))
	return out


def carpet(radius, mats, loc=(0.0, 0.0), rings=7, segments=40, seed=0, piles=6, wobble=0.22):
	# Fallen leaves: a very flat lumpy disc with a ragged, drier rim tucked just under the ground.
	bm = bmesh.new()
	off = Vector((seed * 5.1, seed * 2.3, 0.0))
	rnd = geo.rng(seed)
	heaps = []
	for _ in range(piles):
		a, d = rnd.uniform(0, math.tau), rnd.uniform(0.3, 0.75) * radius
		heaps.append((Vector((math.cos(a) * d, math.sin(a) * d, 0.0)), rnd.uniform(0.12, 0.2) * radius))
	def height(p, t):
		if t > 0.999:
			return -0.006
		h = 0.028 * (1.0 - t ** 3) + 0.01 * noise.noise(p * 2.5 + off)
		for (c, r) in heaps:
			d = (p - c).length / r
			if d < 1.0:
				h += 0.03 * (1.0 - d * d)
		return max(h, 0.004)
	edge = []
	for i in range(segments):
		a = i / float(segments) * math.tau
		u = Vector((math.cos(a), math.sin(a), 0.0))
		w = 0.65 * noise.noise(u * 1.2 + off) + 0.35 * noise.noise(u * 3.5 + off * 2.0)
		edge.append(radius * (1.0 + wobble * w))
	centre = bm.verts.new((0.0, 0.0, height(Vector((0, 0, 0)), 0.0)))
	grid = []
	for r in range(1, rings + 1):
		t = r / float(rings)
		row = []
		for i in range(segments):
			a = i / float(segments) * math.tau
			p = Vector((math.cos(a), math.sin(a), 0.0)) * edge[i] * t
			p.z = height(p, t)
			row.append(bm.verts.new(p))
		grid.append(row)
	for i in range(segments):
		j = (i + 1) % segments
		bm.faces.new((centre, grid[0][i], grid[0][j]))
	for r in range(rings - 1):
		a, b = grid[r], grid[r + 1]
		for i in range(segments):
			j = (i + 1) % segments
			f = bm.faces.new((a[i], b[i], b[j], a[j]))
			if r >= rings - 2:
				f.material_index = 1
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	for f in bm.faces:
		if f.normal.z < 0.0:
			f.normal_flip()
	bmesh.ops.translate(bm, vec=Vector((loc[0], loc[1], 0.0)), verts=bm.verts)
	mesh = bpy.data.meshes.new("carpet")
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new("carpet", mesh))
	for m in mats:
		mesh.materials.append(m)
	for p in mesh.polygons:
		p.use_smooth = True
	return obj


def build():
	bark = mat.make("ginkgo_bark", (0.31, 0.28, 0.25), pattern="bark", rough=0.95, bump=0.75)
	moss = mat.get("moss_dark")
	gold = mat.get("ginkgo")
	gold_light = mat.make("ginkgo_light", (0.87, 0.75, 0.33), pattern="leaf", rough=0.75, bump=0.4)
	gold_deep = mat.make("ginkgo_deep", (0.70, 0.56, 0.21), pattern="leaf", rough=0.78, bump=0.45)
	fallen = mat.make("ginkgo_fallen", (0.72, 0.59, 0.24), pattern="leaf", rough=0.88, bump=0.5)
	dry = mat.make("ginkgo_fallen_dry", (0.56, 0.46, 0.24), pattern="leaf", rough=0.92, bump=0.5)
	straw = mat.make("geumjul_straw", (0.55, 0.48, 0.34), pattern="hair", rough=0.95, bump=0.5)
	paper = mat.get("paper")
	glow = mat.get("spore_glow")

	wood = []
	wood.append(geo.lathe([(0.5, -0.05), (0.42, 0.15), (0.33, 0.45), (0.29, 0.9), (0.275, 1.6), (0.26, 2.4), (0.23, 3.0), (0.18, 3.4)],
						  mat=bark, segments=16, name="trunk"))
	rnd = geo.rng(3)
	for k in range(6):
		a = k / 6.0 * math.tau + rnd.uniform(-0.25, 0.25)
		r = rnd.uniform(0.75, 0.95)
		wood.append(geo.tube([(0.0, 0.0, 0.55), (math.cos(a) * 0.38, math.sin(a) * 0.38, 0.16), (math.cos(a) * r, math.sin(a) * r, -0.04)],
							 [0.17, 0.12, 0.05], mat=bark, segments=8, name="root"))
	limbs = [
		[(0.0, 0.0, 2.4), (0.55, 0.2, 3.4), (1.05, 0.35, 4.3)],
		[(0.0, 0.0, 2.5), (-0.5, 0.3, 3.5), (-1.0, 0.5, 4.4)],
		[(0.0, 0.0, 2.6), (0.2, -0.55, 3.6), (0.35, -1.05, 4.5)],
		[(0.0, 0.0, 2.7), (-0.3, -0.4, 3.8), (-0.7, -0.85, 4.8)],
		[(0.0, 0.0, 2.8), (0.05, 0.1, 4.2), (0.0, 0.0, 6.4)],
	]
	for pts in limbs:
		wood.append(geo.tube(pts, [0.18, 0.14, 0.1], mat=bark, segments=10, name="limb"))
	wood.append(geo.blob((0.32, 0.25, 0.08), (-0.35, 0.45, 0.04), mat=moss, lumpy=0.3, seed=5, name="root_moss"))
	wood.append(geo.blob((0.2, 0.12, 0.45), (-0.12, 0.26, 0.85), mat=moss, lumpy=0.3, seed=6, name="trunk_moss"))

	leaves = [geo.blob((1.5, 1.45, 1.9), (0.0, 0.0, 5.3), mat=gold_deep, lumpy=0.15, seed=40, name="core")]
	spec = []
	for k in range(5):
		a = k / 5.0 * math.tau + 0.3
		spec.append(((math.cos(a) * 1.35, math.sin(a) * 1.35, 3.9 + 0.15 * (k % 2)), (1.02, 0.95, 0.8), gold_deep, gold, 41 + k))
	for k in range(5):
		a = k / 5.0 * math.tau + 0.95
		spec.append(((math.cos(a) * 1.3, math.sin(a) * 1.3, 5.25 + 0.15 * (k % 2)), (1.05, 1.0, 0.88), gold, gold_light, 51 + k))
	for k in range(3):
		a = k / 3.0 * math.tau + 0.2
		spec.append(((math.cos(a) * 0.75, math.sin(a) * 0.75, 6.45), (0.95, 0.9, 0.8), gold, gold_light, 61 + k))
	spec.append(((0.0, 0.05, 7.2), (0.85, 0.8, 0.62), gold_light, gold_light, 70))
	for (c, r, m, top, seed) in spec:
		leaves += clump(c, r, m, seed, 4, top)
	for p in leaves:
		geo.tag(p, "sway")

	hard = [carpet(2.15, (fallen, dry), seed=2)]
	for (x, y, r, seed) in ((2.15, -0.75, 0.32, 7), (-1.05, -2.0, 0.26, 8), (-2.15, 0.6, 0.3, 9)):
		hard.append(carpet(r, (fallen, dry), (x, y), rings=2, segments=12, seed=seed, piles=0, wobble=0.4))
	ring = [(0.305 * math.cos(i / 20.0 * math.tau), 0.305 * math.sin(i / 20.0 * math.tau), 1.55 + 0.03 * math.sin(i / 20.0 * math.tau * 2.0)) for i in range(21)]
	hard.append(geo.tube(ring, 0.035, mat=straw, segments=6, name="rope", twist=0.6))
	for k in range(4):
		a = -0.9 + k * 0.45
		strip = geo.box((0.06, 0.01, 0.2), (0.33 * math.cos(a), 0.33 * math.sin(a), 1.42), (0.0, 0.0, math.degrees(a) + 90.0), mat=paper, name="paper")
		hard.append(strip)
	for (p, r) in (((1.6, -1.35, 3.25), 0.075), ((-1.55, 1.2, 3.5), 0.065)):
		g = geo.blob(r, p, mat=glow, subdivisions=2, name="mote")
		geo.tag(g, "pulse", "sway")
		hard.append(g)

	return pipeline.mixed("ginkgo", hard, wood + leaves, voxel=0.045, tris=2800, size=512)
