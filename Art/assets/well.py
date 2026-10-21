# The village well: a ring of stacked fieldstones under flat capstones, dark still water inside, two rough posts
# carrying a little board roof, a windlass with a crank, rope and a wooden bucket; moss in every joint.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

R_OUT = 0.95
WALL_H = 0.78
RIM_IN, RIM_OUT = 0.64, 0.99
WATER_Z = 0.5
AXLE_Z = 1.62
ROOF_TOP = 2.9
PITCH = 42.0


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


def ring_wall(r, z0, z1, m, inward, segments=24, name="ring"):
	verts = []
	for (z) in (z0, z1):
		for i in range(segments):
			a = 2.0 * math.pi * i / segments
			verts.append((math.cos(a) * r, math.sin(a) * r, z))
	faces = []
	for i in range(segments):
		j = (i + 1) % segments
		f = (i, j, segments + j, segments + i)
		faces.append(tuple(reversed(f)) if inward else f)
	return mesh_obj(name, verts, faces, m, True)


def drop_faces(obj, fn):
	# Faces nobody sees (against the core, on the ground) would only waste texture space.
	obj = geo.apply_modifiers(obj)
	bpy.context.view_layer.update()
	mw = obj.matrix_world
	rot = mw.to_3x3()
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	dead = [f for f in bm.faces if fn((rot @ f.normal).normalized(), mw @ f.calc_center_median())]
	bmesh.ops.delete(bm, geom=dead, context='FACES')
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def moss_pad(radii, loc, rot, m, seed, segments=10):
	# A soft cushion: domed, edges rolled under, lumped by low-frequency noise.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], loc, rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.25, 3.0, seed=seed)


def build():
	stone = mat.get("stone")
	stone_b = mat.make("well_stone_b", (0.42, 0.41, 0.38), pattern="stone", rough=0.92, bump=0.6)
	stone_dark = mat.get("stone_dark")
	cap_m = mat.make("well_capstone", (0.45, 0.44, 0.41), pattern="concrete", rough=0.9, bump=0.5)
	water = mat.make("well_water", (0.04, 0.07, 0.08), pattern="plain", rough=0.06, grime=0.0, edge=0.0, bump=0.02)
	post_m = mat.get("wood_dark")
	board = mat.make("well_roof_boards", (0.34, 0.32, 0.28), pattern="planks", axis='X', rough=0.9, bump=0.5)
	axle_m = mat.get("bark")
	rope_m = mat.make("rope", (0.52, 0.45, 0.33), pattern="knit", rough=0.95, bump=0.5)
	bucket_m = mat.make("bucket_wood", (0.40, 0.31, 0.22), pattern="wood", rough=0.85, bump=0.4)
	metal = mat.get("metal")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")

	parts = []
	rnd = geo.rng(4)

	# Stacked fieldstones in four offset courses, round a dark core that fills the joints.
	parts.append(ring_wall(R_OUT - 0.12, 0.0, WALL_H, stone_dark, False, name="core"))
	n = 14
	for k in range(4):
		z0 = k * 0.195
		for i in range(n):
			a = 2.0 * math.pi * (i + 0.5 * (k % 2) + rnd.uniform(-0.08, 0.08)) / n
			w = 2.0 * math.pi * R_OUT / n * rnd.uniform(0.8, 0.95)
			h = 0.19 * rnd.uniform(0.85, 1.0)
			r = R_OUT - 0.1 + rnd.uniform(-0.015, 0.02)
			s = geo.box((w, 0.2, h), (math.cos(a) * r, math.sin(a) * r, z0 + h * 0.5 + 0.005),
						(rnd.uniform(-3, 3), rnd.uniform(-3, 3), math.degrees(a) + 90), mat=stone if rnd.random() < 0.65 else stone_b, name="stone")
			s = geo.jitter(s, 0.025, 3.0, seed=k * 20 + i)
			parts.append(drop_faces(s, lambda nm, c: nm.z < -0.9 or nm.dot(Vector((c.x, c.y, 0)).normalized()) < -0.8))

	# Flat capstones round the rim; the inside face of the shaft above the water.
	segs = 8
	for i in range(segs):
		a0 = 2.0 * math.pi * i / segs + rnd.uniform(-0.03, 0.03)
		a1 = 2.0 * math.pi * (i + 1) / segs - 0.025
		outline = []
		for t in range(4):
			a = a0 + (a1 - a0) * t / 3
			outline.append((math.cos(a) * RIM_OUT * rnd.uniform(0.97, 1.0), math.sin(a) * RIM_OUT * rnd.uniform(0.97, 1.0)))
		for t in range(3, -1, -1):
			a = a0 + (a1 - a0) * t / 3
			outline.append((math.cos(a) * RIM_IN, math.sin(a) * RIM_IN))
		cap = geo.polygon_slab(outline, rnd.uniform(0.09, 0.12), (0, 0, WALL_H - 0.01), (rnd.uniform(-1.5, 1.5), rnd.uniform(-1.5, 1.5), 0),
							   mat=cap_m, name="capstone")
		cap = geo.jitter(cap, 0.02, 2.5, seed=40 + i)
		parts.append(drop_faces(cap, lambda nm, c: nm.z < -0.9))
	parts.append(ring_wall(RIM_IN + 0.01, WATER_Z - 0.05, WALL_H, stone_dark, True, name="shaft"))
	parts.append(drop_faces(geo.cylinder(RIM_IN + 0.02, 0.02, (0, 0, WATER_Z), mat=water, segments=24, name="water"),
							lambda nm, c: nm.z < 0.5))
	for k in range(2):
		lf = geo.blob((0.05, 0.03, 0.006), (0.22 - 0.4 * k, -0.15 + 0.3 * k, WATER_Z + 0.012), (0, 0, 40 + 70 * k), mat=leaf, subdivisions=1, name="float_leaf")
		parts.append(lf)

	# Two rough posts, a ridge beam and a small board roof.
	for sx in (-1.0, 1.0):
		p = geo.box((0.14, 0.16, ROOF_TOP - 0.42), (sx * 1.03, 0.0, (ROOF_TOP - 0.42) * 0.5), (0, sx * 1.5, 0), mat=post_m, name="post")
		parts.append(drop_faces(geo.jitter(p, 0.015, 2.0, seed=int(sx + 3)), lambda nm, c: nm.z < -0.9))
		parts.append(geo.box((0.1, 0.5, 0.08), (sx * 1.03, 0.0, ROOF_TOP - 0.47), mat=post_m, name="bracket"))
	parts.append(geo.box((2.36, 0.12, 0.14), (0, 0, ROOF_TOP - 0.42), mat=post_m, name="ridge_beam"))
	slope = 0.9
	pr = math.radians(PITCH)
	for sy in (-1.0, 1.0):
		cy = sy * slope * 0.5 * math.cos(pr)
		cz = ROOF_TOP - 0.06 - slope * 0.5 * math.sin(pr)
		parts.append(geo.box((2.36, slope, 0.06), (0, cy, cz), (sy * -PITCH, 0, 0), mat=board, name="roof"))
		parts.append(geo.box((2.38, 0.05, 0.1), (0, sy * slope * math.cos(pr), ROOF_TOP - 0.06 - slope * math.sin(pr) - 0.03), mat=post_m, name="roof_edge"))
	parts.append(geo.box((2.42, 0.16, 0.08), (0, 0, ROOF_TOP - 0.04), (45, 0, 0), mat=post_m, name="roof_ridge"))

	# Windlass: a log axle between the posts, rope wound on it, a crank outside the right post.
	parts.append(geo.cylinder(0.065, 2.0, (0, 0, AXLE_Z), (0, 90, 0), mat=axle_m, segments=8, name="axle"))
	parts.append(geo.cylinder(0.085, 0.32, (-0.05, 0, AXLE_Z), (0, 90, 0), mat=rope_m, segments=8, name="rope_coil"))
	parts.append(geo.tube([(1.0, 0, AXLE_Z), (1.16, 0, AXLE_Z), (1.16, -0.02, AXLE_Z - 0.24), (1.27, -0.02, AXLE_Z - 0.24)], 0.018,
						  mat=metal, segments=6, name="crank"))
	parts.append(geo.cylinder(0.025, 0.12, (1.28, -0.02, AXLE_Z - 0.24), (0, 90, 0), mat=post_m, segments=6, name="crank_grip"))
	parts.append(geo.tube([(0.02, 0, AXLE_Z - 0.08), (0.03, 0, 1.26)], 0.012, mat=rope_m, segments=5, name="rope"))

	# The bucket hanging over the water.
	bz = 0.95
	parts.append(geo.lathe([(0.0, 0.0), (0.12, 0.0), (0.13, 0.02), (0.16, 0.26), (0.145, 0.26), (0.115, 0.03), (0.0, 0.03)],
						   (0.03, 0, bz), mat=bucket_m, segments=12, name="bucket"))
	for hz in (0.06, 0.2):
		r = 0.125 + 0.03 * hz / 0.26
		parts.append(geo.cylinder(r + 0.01, 0.025, (0.03, 0, bz + hz), mat=metal, segments=12, name="hoop", caps=False))
	arc = [(0.03 + math.cos(a) * 0.15, 0, bz + 0.25 + math.sin(a) * 0.08) for a in [math.pi * t / 6 for t in range(7)]]
	parts.append(geo.tube(arc, 0.008, mat=metal, segments=4, name="bucket_handle"))
	parts.append(drop_faces(geo.cylinder(0.14, 0.01, (0.03, 0, bz + 0.2), mat=water, segments=12, name="bucket_water"), lambda nm, c: nm.z < 0.5))

	# Moss: cushions on the capstones and roof, a skirt round the foot, ferns in the joints.
	for k, (a, radii, m) in enumerate(((0.5, (0.22, 0.16, 0.07), moss), (2.4, (0.2, 0.13, 0.06), moss_dark),
									   (4.1, (0.24, 0.15, 0.07), moss), (5.4, (0.15, 0.11, 0.05), moss_dark))):
		r = (RIM_IN + RIM_OUT) * 0.5
		pad = moss_pad(radii, (math.cos(a) * r, math.sin(a) * r, WALL_H + 0.08), (0, 0, math.degrees(a) + 90), m, 50 + k)
		if k == 0:
			geo.tag(pad, "pulse")
		parts.append(pad)
	for k, a in enumerate((0.3, 1.3, 2.2, 3.4, 4.4, 5.6)):
		r = R_OUT + 0.02
		pad = moss_pad((0.32, 0.12, 0.16), (math.cos(a) * r, math.sin(a) * r, 0.0), (0, 0, math.degrees(a) + 90),
					   moss_dark if k % 2 else moss, 60 + k, segments=8)
		parts.append(pad)
	for k, a in enumerate((0.9, 3.0, 4.8)):
		r = R_OUT - 0.02
		pad = moss_pad((0.18, 0.14, 0.05), (math.cos(a) * r, math.sin(a) * r, 0.35 + 0.1 * k), (90, 0, math.degrees(a) + 90),
					   moss_dark, 70 + k, segments=8)
		parts.append(pad)
	for k, (x, d, radii, m) in enumerate(((-0.55, 0.5, (0.36, 0.26, 0.08), moss), (-0.15, 0.68, (0.22, 0.16, 0.07), moss_dark),
										  (0.7, 0.3, (0.2, 0.15, 0.06), moss))):
		roof_pad = moss_pad(radii, (x, -d * math.cos(pr), ROOF_TOP - 0.03 - d * math.sin(pr)), (PITCH, 0, rnd.uniform(-30, 30)), m, 80 + k, segments=12)
		if k == 0:
			geo.tag(roof_pad, "pulse")
		parts.append(roof_pad)
	back_pad = moss_pad((0.4, 0.24, 0.08), (0.3, 0.45 * math.cos(pr), ROOF_TOP - 0.03 - 0.45 * math.sin(pr)), (-PITCH, 0, 10), moss_dark, 84, segments=12)
	parts.append(back_pad)
	for k, a in enumerate((0.0, 1.9, 3.7, 5.0)):
		for j in range(4):
			aa = a + rnd.uniform(-0.15, 0.15)
			r = R_OUT + 0.05
			h = rnd.uniform(0.18, 0.32)
			fern = geo.cone(rnd.uniform(0.04, 0.06), h, (math.cos(aa) * r, math.sin(aa) * r, h * 0.5),
							(rnd.uniform(-25, 25), rnd.uniform(-25, 25), 0), mat=leaf if j % 2 else leaf_dark, segments=4, name="fern")
			geo.tag(fern, "sway", "foliage")
			parts.append(fern)

	for p in parts:
		if p.name.startswith(("stone", "capstone", "post", "roof", "ridge_beam", "core")):
			geo.moss_mask(p, seed=11)

	return pipeline.static("well", parts, size=512)
