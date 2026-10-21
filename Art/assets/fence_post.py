# A rough hewn fence post, a little crooked, its top split by the weather, a twist of rusty wire where the
# rail once hung and a tuft of moss round its foot.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

H = 0.9


def mesh_obj(name, verts, faces, m, smooth=True):
	bm = bmesh.new()
	bv = [bm.verts.new(v) for v in verts]
	for f in faces:
		bm.faces.new([bv[i] for i in f])
	me = bpy.data.meshes.new(name)
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, me))
	if m is not None:
		me.materials.append(m)
	for p in me.polygons:
		p.use_smooth = smooth
	return obj


def axis(z):
	# The crooked centre line.
	t = z / H
	return Vector((0.025 * math.sin(t * 2.4), -0.015 * t * t, z))


def post_body(m):
	# A square hewn post, slightly tapering, following the crooked axis.
	rings = [0.0, 0.3, 0.62, H]
	verts = []
	for z in rings:
		c = axis(z)
		h = 0.062 - 0.006 * z / H
		for (dx, dy) in ((-h, -h), (h, -h), (h, h), (-h, h)):
			verts.append((c.x + dx, c.y + dy, z))
	faces = []
	for r in range(len(rings) - 1):
		for k in range(4):
			k2 = (k + 1) % 4
			faces.append((r * 4 + k, r * 4 + k2, (r + 1) * 4 + k2, (r + 1) * 4 + k))
	return mesh_obj("post", verts, faces, m, False)


def build():
	wood = mat.make("fence_wood", (0.30, 0.27, 0.22), pattern="wood", rough=0.92, bump=0.55)
	wood_end = mat.make("fence_wood_end", (0.42, 0.37, 0.30), pattern="wood", rough=0.92, bump=0.4)
	crack = mat.make("fence_crack", (0.08, 0.07, 0.06), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	wire = mat.get("rust")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")

	parts = [geo.jitter(post_body(wood), 0.006, 6.0, seed=2)]

	# The split top: two halves leaning apart over a dark crack that runs down the face.
	top = axis(H)
	for s in (-1.0, 1.0):
		half = geo.box((0.112, 0.052, 0.15), (top.x, top.y + s * 0.03, H + 0.06 - (0.015 if s > 0 else 0.0)), (s * -5, 0, 0), mat=wood, name="post_top")
		parts.append(half)
		cap_z = H + 0.135 - (0.015 if s > 0 else 0.0) + abs(math.sin(math.radians(5))) * 0.03
		parts.append(geo.box((0.1, 0.045, 0.005), (top.x, top.y + s * 0.034, cap_z), (s * -5, 0, 0), mat=wood_end, name="post_end"))
	parts.append(geo.box((0.1, 0.016, 0.14), (top.x, top.y, H + 0.05), mat=crack, name="crack"))
	front = axis(H - 0.12)
	parts.append(geo.box((0.008, 0.004, 0.2), (front.x - 0.01, front.y - 0.058, H - 0.1), (0, 4, 0), mat=crack, name="crack_line"))

	# Rusty wire twisted round the post at rail height.
	ring = []
	for k in range(10):
		a = 2.0 * math.pi * k / 9
		c = axis(0.68)
		ring.append((c.x + math.cos(a) * 0.072, c.y + math.sin(a) * 0.072, 0.66 + 0.04 * k / 9))
	ring.append((ring[-1][0] + 0.05, ring[-1][1] - 0.06, 0.66))
	parts.append(geo.tube(ring, 0.006, mat=wire, segments=4, name="wire"))

	# Moss tuft and a few blades round the foot.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (0.0, 0.0, 0.0), mat=moss, segments=10, name="moss_pad",
					close_bottom=False)
	pad.scale = (0.15, 0.13, 0.1)
	geo.apply_transform(pad)
	parts.append(geo.jitter(pad, 0.03, 8.0, seed=4))
	side = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (0.06, -0.05, 0.08), (75, 0, 30), mat=moss_dark, segments=8,
					 name="moss_side", close_bottom=False)
	side.scale = (0.05, 0.08, 0.025)
	parts.append(side)
	rnd = geo.rng(6)
	for j in range(4):
		h = rnd.uniform(0.12, 0.22)
		a = rnd.uniform(0.0, 2.0 * math.pi)
		bl = geo.cone(0.02, h, (math.cos(a) * 0.12, math.sin(a) * 0.12, h * 0.5), (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0),
					  mat=leaf, segments=3, name="blade")
		geo.tag(bl, "sway", "foliage")
		parts.append(bl)

	for p in parts:
		if p.name.startswith("post"):
			geo.moss_mask(p, seed=2)

	return pipeline.static("fence_post", parts, size=256)
