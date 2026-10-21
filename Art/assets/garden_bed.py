# One raised row of grandmother's vegetable patch: a mound of dark tilled soil with hoe furrows, held by
# weathered boards along both long sides, seedlings in two rows. Radishes are set on top at x = +-1.0.
import math

import bmesh
import bpy
from mathutils import Vector, noise

from gse import core, geo, mat, pipeline

HX, HY = 1.6, 0.41
TOP = 0.15


def soil_h(x, y):
	# A flat-topped ridge (radishes sit at x = +-1.0) whose shoulders roll down to the boards.
	ay = abs(y) / HY
	t = max(0.0, min(1.0, (ay - 0.35) / 0.65))
	shoulder = t * t * (3.0 - 2.0 * t)
	h = 0.136 - 0.08 * shoulder
	end = max(0.0, min(1.0, (HX - abs(x)) / 0.3))
	h *= math.sin(end * math.pi * 0.5) ** 0.7
	furrow = 0.017 * math.cos(2.0 * math.pi * y / 0.2)
	clod = 0.01 * noise.noise(Vector((x * 5.0, y * 5.0, 0.3)))
	return max(0.0, h + (furrow + clod) * end)


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


def drop_faces(obj, fn):
	# Faces nobody sees (on the ground, against the soil) would only waste texture space.
	obj = geo.apply_modifiers(obj)
	bpy.context.view_layer.update()
	rot = obj.matrix_world.to_3x3()
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	dead = [f for f in bm.faces if fn((rot @ f.normal).normalized())]
	bmesh.ops.delete(bm, geom=dead, context='FACES')
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def soil(m):
	nx, ny = 20, 20
	verts = []
	for j in range(ny + 1):
		y = -HY + 2.0 * HY * j / ny
		for i in range(nx + 1):
			x = -HX + 2.0 * HX * i / nx
			verts.append((x, y, soil_h(x, y)))
	faces = []
	w = nx + 1
	for j in range(ny):
		for i in range(nx):
			faces.append((j * w + i, j * w + i + 1, (j + 1) * w + i + 1, (j + 1) * w + i))
	return mesh_obj("soil", verts, faces, m, True)


def sprout(x, y, m, rnd):
	# Two seed leaves on a short stem.
	h = rnd.uniform(0.04, 0.07)
	z = soil_h(x, y)
	verts = [(x, y, z), (x, y, z + h)]
	faces = []
	a = rnd.uniform(0.0, math.pi)
	for s in (-1.0, 1.0):
		d = Vector((math.cos(a), math.sin(a), 0.0)) * s
		side = Vector((-d.y, d.x, 0.0))
		base = Vector((x, y, z + h))
		tip = base + d * 0.06 + Vector((0, 0, 0.025))
		mid = base + d * 0.032 + Vector((0, 0, 0.018))
		k = len(verts)
		verts += [tuple(base), tuple(mid + side * 0.018), tuple(tip), tuple(mid - side * 0.018)]
		f = (k, k + 1, k + 2, k + 3)
		faces.append(f if s < 0 else tuple(reversed(f)))
	leaf = mesh_obj("sprout", verts[2:], [tuple(i - 2 for i in f) for f in faces], m, True)
	for p in leaf.data.polygons:
		if p.normal.z < 0.0:
			p.flip()
	stem = geo.cone(0.006, h + 0.01, (x, y, z + (h + 0.01) * 0.5), mat=m, segments=4, name="sprout_stem")
	return [leaf, stem]


def build():
	dirt = mat.make("tilled_soil", (0.22, 0.17, 0.12), pattern="moss", accent=(0.29, 0.23, 0.17), rough=0.98, bump=0.6)
	board_m = mat.make("bed_board", (0.32, 0.31, 0.27), pattern="wood", rough=0.92, bump=0.5)
	board_b = mat.make("bed_board_old", (0.27, 0.25, 0.21), pattern="wood", rough=0.92, bump=0.5)
	stake_m = mat.get("wood_dark")
	clod_m = mat.make("soil_clod", (0.27, 0.21, 0.15), pattern="stone", rough=0.98, bump=0.5)
	green = mat.make("seedling", (0.30, 0.46, 0.26), pattern="leaf", rough=0.7, bump=0.2)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")

	parts = [soil(dirt)]
	rnd = geo.rng(8)

	# Edging boards on both long sides: the back one leans out, the front one is two old pieces.
	back = geo.box((3.12, 0.035, 0.1), (0.0, HY + 0.022, 0.048), (-7, 0, 0.6), mat=board_m, name="board")
	parts.append(drop_faces(geo.jitter(back, 0.006, 2.0, seed=1), lambda n: n.z < -0.9))
	f1 = geo.box((1.9, 0.035, 0.1), (-0.62, -HY - 0.022, 0.05), (3, 0, -0.5), mat=board_b, name="board")
	f2 = geo.box((1.16, 0.035, 0.09), (0.99, -HY - 0.026, 0.045), (5, 1.5, 1.0), mat=board_m, name="board")
	for b in (f1, f2):
		parts.append(drop_faces(geo.jitter(b, 0.006, 2.0, seed=2), lambda n: n.z < -0.9))
	for (x, y) in ((-1.54, -HY - 0.05), (1.54, -HY - 0.05), (-1.54, HY + 0.05), (1.54, HY + 0.05), (0.38, -HY - 0.05), (0.0, HY + 0.05)):
		st = geo.box((0.04, 0.035, 0.15), (x, y, 0.07), (rnd.uniform(-4, 4), rnd.uniform(-4, 4), 0), mat=stake_m, name="stake")
		parts.append(drop_faces(st, lambda n: n.z < -0.9))
	parts.append(geo.cylinder(0.008, 0.012, (0.37, -HY - 0.075, 0.08), (90, 0, 0), mat=mat.get("rust"), segments=5, name="nail"))

	# Seedlings in two rows, clear of the radish spots.
	for x in (-1.38, -0.62, -0.25, 0.22, 0.58, 1.38):
		for y in (-0.205, 0.205):
			if rnd.random() < 0.8:
				for p in sprout(x + rnd.uniform(-0.05, 0.05), y + rnd.uniform(-0.02, 0.02), green, rnd):
					geo.tag(p, "sway", "foliage")
					parts.append(p)

	# A few clods, moss creeping on the board ends and weeds at the corners.
	for (x, y) in ((-0.4, 0.05), (0.75, -0.12), (1.45, 0.1), (-1.25, -0.08)):
		c = geo.blob((0.04, 0.035, 0.025), (x, y, soil_h(x, y) + 0.008), mat=clod_m, subdivisions=1, lumpy=0.3, seed=int(x * 10), name="clod")
		parts.append(c)
	for k, (x, y, radii) in enumerate(((-1.45, -HY - 0.04, (0.18, 0.06, 0.08)), (1.3, HY + 0.05, (0.25, 0.06, 0.09)),
									   (0.25, -HY - 0.05, (0.12, 0.05, 0.06)))):
		pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, 0.0), mat=moss_dark if k % 2 else moss,
						segments=8, name="moss_pad", close_bottom=False)
		pad.scale = radii
		geo.apply_transform(pad)
		parts.append(geo.jitter(pad, 0.02, 4.0, seed=k))
	for (cx, cy) in ((-1.62, 0.35), (1.63, -0.3)):
		for j in range(3):
			h = rnd.uniform(0.12, 0.22)
			wd = geo.cone(rnd.uniform(0.025, 0.04), h, (cx + rnd.uniform(-0.06, 0.06), cy + rnd.uniform(-0.08, 0.08), h * 0.5),
						  (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=4, name="weed")
			geo.tag(wd, "sway", "foliage")
			parts.append(wd)

	for p in parts:
		if p.name.startswith(("board", "stake")):
			geo.moss_mask(p, seed=13)

	res = pipeline.static("garden_bed", parts, size=256)
	res["unit"] = (3.2, 1.0, 0.9)
	return res
