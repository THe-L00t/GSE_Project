# A Korean radish (mu) growing in the garden: the pale green-white shoulder of the root showing above
# a little soil mound, and a rosette of long lobed leaves arching out of it.
import math

import bmesh
import bpy
from mathutils import Vector, noise

from gse import core, geo, mat, pipeline


def spline(points, steps=4):
	# Catmull-Rom through the points; returns the smooth points and their 0..1 position along the line.
	pts = [Vector(p) for p in points]
	ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
	out = []
	for i in range(len(pts) - 1):
		p0, p1, p2, p3 = ext[i], ext[i + 1], ext[i + 2], ext[i + 3]
		for k in range(steps):
			t = k / float(steps)
			t2, t3 = t * t, t * t * t
			out.append(0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (3 * p1 - p0 - 3 * p2 + p3) * t3))
	out.append(pts[-1])
	return out, [i / float(len(out) - 1) for i in range(len(out))]


def blade(points, width, mats, thick=0.004, fold=0.3, lobes=0, lobe_depth=0.45, stalk=0.0, facing=(0, 0, 1), steps=4, name="leaf"):
	# A thin closed leaf swept along a curve: cupped along the midrib, pinnately lobed if asked.
	# width(u) gives the half width; faces before `stalk` take the second material (the leaf stalk).
	pts, us = spline(points, steps)
	bm = bmesh.new()
	rings = []
	side_prev = None
	hint = Vector(facing).normalized()
	for i, p in enumerate(pts):
		if i == 0:
			t = (pts[1] - pts[0]).normalized()
		elif i == len(pts) - 1:
			t = (pts[-1] - pts[-2]).normalized()
		else:
			t = (pts[i + 1] - pts[i - 1]).normalized()
		if side_prev is None:
			side = t.cross(hint).normalized()
		else:
			side = (side_prev - t * side_prev.dot(t)).normalized()
		side_prev = side
		up = side.cross(t).normalized()
		u = us[i]
		w = width(u)
		if lobes > 0 and u > stalk:
			v = (u - stalk) / max(1.0 - stalk, 1e-4)
			w *= 1.0 - lobe_depth * (1.0 - abs(math.sin(v * lobes * math.pi)) ** 0.5) * (1.0 - v * 0.5)
		th = thick * (0.3 + 0.7 * min(1.0, w / max(thick * 4.0, 1e-4)))
		ring = [p - side * w + up * (fold * w), p + up * th, p + side * w + up * (fold * w), p - up * th]
		rings.append([bm.verts.new(c) for c in ring])
	for k in range(len(rings) - 1):
		a, b = rings[k], rings[k + 1]
		for i in range(4):
			j = (i + 1) % 4
			f = bm.faces.new((a[i], a[j], b[j], b[i]))
			f.material_index = 1 if (us[k] < stalk and len(mats) > 1) else 0
	bm.faces.new(list(reversed(rings[0])))
	bm.faces.new(rings[-1])
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	mesh = bpy.data.meshes.new(name)
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, mesh))
	for m in mats:
		mesh.materials.append(m)
	for poly in mesh.polygons:
		poly.use_smooth = True
	return obj


def build():
	soil = mat.get("soil")
	white = mat.make("radish_white", (0.84, 0.84, 0.77), pattern="skin", rough=0.55, bump=0.15, grime=0.3)
	green = mat.make("radish_green", (0.56, 0.64, 0.44), pattern="skin", rough=0.55, bump=0.15, grime=0.3)
	leaf = mat.make("radish_leaf", (0.24, 0.38, 0.22), pattern="leaf", rough=0.8, bump=0.35)
	stalk = mat.make("radish_stalk", (0.46, 0.56, 0.38), pattern="plain", rough=0.7, bump=0.1)

	parts = []
	mound = geo.lathe([(0.25, -0.02), (0.22, 0.02), (0.16, 0.05), (0.08, 0.07), (0.0, 0.075)], mat=soil, segments=18, name="mound")
	geo.jitter(mound, 0.012, scale=9.0, seed=2, axes=(1.0, 1.0, 0.6))
	parts.append(mound)

	root = geo.lathe([(0.085, 0.0), (0.098, 0.04), (0.102, 0.08), (0.098, 0.11), (0.085, 0.135), (0.055, 0.155), (0.0, 0.162)],
					 segments=18, name="root")
	root.data.materials.append(white)
	root.data.materials.append(green)
	for poly in root.data.polygons:
		poly.material_index = 1 if poly.center.z > 0.075 + 0.015 * math.sin(poly.center.x * 40.0) else 0
	geo.jitter(root, 0.004, scale=12.0, seed=3)
	parts.append(root)

	rnd = geo.rng(7)
	for k in range(6):
		a = k / 6.0 * math.tau + rnd.uniform(-0.25, 0.25)
		d = Vector((math.cos(a), math.sin(a), 0.0))
		hs = rnd.uniform(0.82, 1.05)
		rs = rnd.uniform(0.85, 1.1)
		ctrl = [d * 0.025 + Vector((0, 0, 0.15)), d * 0.07 * rs + Vector((0, 0, 0.31 * hs)), d * 0.17 * rs + Vector((0, 0, 0.44 * hs)),
				d * 0.29 * rs + Vector((0, 0, 0.41 * hs)), d * 0.36 * rs + Vector((0, 0, 0.3 * hs))]
		bw = rnd.uniform(0.062, 0.072)
		lf = blade(ctrl, lambda u, bw=bw: bw * (0.18 + 0.82 * min(1.0, u * 1.6)) * (1.0 - u ** 4) if u > 0.22 else bw * 0.16,
				   (leaf, stalk), thick=0.005, fold=0.12, lobes=3, lobe_depth=0.7, stalk=0.22, facing=d, steps=5, name="leaf")
		geo.tag(lf, "sway", "foliage")
		parts.append(lf)

	return pipeline.static("vegetable", parts, size=256)
