# One weathered fence rail for a span between posts: a sagging hewn plank with one end split open and a rusty
# nail at each end. Built at real size and squeezed into the unit box the game stretches along the fence.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

L = 1.4
W = 0.07
T = 0.095
SAG = 0.015


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


def bottom(y):
	# Ends rest on the posts; the middle sags.
	t = y / (L * 0.5)
	return SAG * t * t


def plank(y0, y1, z_lo, z_hi, twist, m, name, segs=6, cap0=True, cap1=True, lift=0.0):
	# A rectangular section swept along the rail from y0 to y1, between heights z_lo..z_hi above the sag line.
	verts = []
	for i in range(segs + 1):
		y = y0 + (y1 - y0) * i / segs
		b = bottom(y) + lift * (i / segs) ** 2
		dx = twist * (i / segs)
		for (x, z) in ((-W * 0.5, z_lo), (W * 0.5, z_lo), (W * 0.5, z_hi), (-W * 0.5, z_hi)):
			verts.append((x + dx * (z - z_lo) / T, y, b + z))
	faces = []
	for i in range(segs):
		for k in range(4):
			k2 = (k + 1) % 4
			faces.append((i * 4 + k, i * 4 + k2, (i + 1) * 4 + k2, (i + 1) * 4 + k))
	if cap0:
		faces.append((0, 1, 2, 3))
	if cap1:
		last = segs * 4
		faces.append((last + 3, last + 2, last + 1, last))
	obj = mesh_obj(name, verts, faces, m, False)
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def build():
	wood = mat.make("rail_wood", (0.31, 0.28, 0.23), pattern="wood", rough=0.92, bump=0.55, axis='X')
	wood_end = mat.make("rail_wood_end", (0.42, 0.37, 0.30), pattern="wood", rough=0.92, bump=0.4)
	nail_m = mat.get("rust")
	moss = mat.get("moss")

	parts = []
	split_at = L * 0.5 - 0.2
	parts.append(geo.jitter(plank(-L * 0.5, split_at, 0.0, T, 0.0, wood, "rail", segs=7, cap1=False), 0.004, 5.0, seed=1))
	# The split end: the upper half lifts away from the lower along the grain.
	parts.append(plank(split_at, L * 0.5, 0.0, T * 0.48, -0.004, wood, "rail_split_lo", segs=2, cap0=False))
	parts.append(plank(split_at, L * 0.5, T * 0.52, T, 0.003, wood, "rail_split_hi", segs=3, cap0=False, lift=0.01))
	parts.append(geo.box((W * 0.9, 0.004, T * 0.9), (0, -L * 0.5 + 0.002, bottom(-L * 0.5) + T * 0.5), mat=wood_end, name="rail_end"))

	for y in (-L * 0.5 + 0.06, L * 0.5 - 0.07):
		parts.append(geo.cylinder(0.009, 0.006, (-W * 0.5 - 0.002, y, bottom(y) + T * 0.5), (0, 90, 0), mat=nail_m, segments=6, name="nail"))

	# A strip of moss along the top in the middle of the span.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (0.0, -0.12, T + bottom(-0.12) - 0.005), mat=moss, segments=8,
					name="moss_pad", close_bottom=False)
	pad.scale = (0.034, 0.26, 0.018)
	geo.apply_transform(pad)
	parts.append(geo.jitter(pad, 0.006, 10.0, seed=3))

	for p in parts:
		if p.name.startswith("rail"):
			geo.moss_mask(p, seed=4)

	res = pipeline.static("fence_rail", parts, size=256)
	res["unit"] = (0.08, 0.12, 1.4)
	return res
