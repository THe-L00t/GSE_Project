# Grandmother's letter and hand-drawn map left on the ground: a curled sheet addressed "하율아" in lines of
# handwriting, the map half unfolded beneath it with a red route to the fold, a river stone holding them down.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

LETTER = dict(c=(-0.05, 0.03), yaw=7.0, size=(0.42, 0.30), z=0.011, n=(10, 8))
MAP = dict(c=(0.17, -0.12), yaw=-17.0, size=(0.38, 0.28), z=0.005, n=(12, 6))
FOLD = math.radians(38.0)


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


def face_up(obj):
	for p in obj.data.polygons:
		if p.normal.z < 0.0:
			p.flip()
	return obj


def letter_curl(u, v):
	# The bottom-right corner curls up; the stone keeps the top-right flat.
	a = max(0.0, (u - 0.2) / 0.3)
	b = max(0.0, (-0.1 - v) / 0.4)
	return 0.032 * (a * a) * (0.3 + 0.7 * b) + 0.0015 * math.sin(u * 9.0 + v * 4.0)


def map_fold(u, v):
	# Returns (along, lift): the right third is folded back up about the second crease.
	crease = 1.0 / 6.0
	if u <= crease:
		bump = 0.004 * max(0.0, 1.0 - abs(u + crease) * 30.0)
		return u, bump
	d = u - crease
	return crease + d * math.cos(FOLD), d * math.sin(FOLD) * (MAP["size"][0] / 1.0)


def on_sheet(sheet, u, v, lift=0.0):
	sx, sy = sheet["size"]
	if sheet is MAP:
		uu, z = map_fold(u, v)
	else:
		uu, z = u, letter_curl(u, v)
	x, y = uu * sx, v * sy
	a = math.radians(sheet["yaw"])
	cx, cy = sheet["c"]
	return Vector((cx + x * math.cos(a) - y * math.sin(a), cy + x * math.sin(a) + y * math.cos(a), sheet["z"] + z + lift))


def sheet_mesh(sheet, m, name, thick):
	nu, nv = sheet["n"]
	verts = []
	idx = {}
	for j in range(nv + 1):
		for i in range(nu + 1):
			idx[(i, j)] = len(verts)
			verts.append(tuple(on_sheet(sheet, -0.5 + i / nu, -0.5 + j / nv)))
	faces = [(idx[(i, j)], idx[(i + 1, j)], idx[(i + 1, j + 1)], idx[(i, j + 1)]) for j in range(nv) for i in range(nu)]
	loop = [idx[(i, 0)] for i in range(nu)] + [idx[(nu, j)] for j in range(nv)] + \
		   [idx[(i, nv)] for i in range(nu, 0, -1)] + [idx[(0, j)] for j in range(nv, 0, -1)]
	low = []
	for v in loop:
		x, y, z = verts[v]
		low.append(len(verts))
		verts.append((x, y, z - thick))
	for k in range(len(loop)):
		k2 = (k + 1) % len(loop)
		faces.append((loop[k], low[k], low[k2], loop[k2]))
	return mesh_obj(name, verts, faces, m, True)


def stroke(sheet, pts, width, m, name, lift=0.0016):
	# A flat ink line drawn on a sheet; pts are (u, v) in the sheet's own unit square.
	sx, sy = sheet["size"]
	dense = []
	for k in range(len(pts) - 1):
		a, b = Vector(pts[k]), Vector(pts[k + 1])
		seg = Vector(((b.x - a.x) * sx, (b.y - a.y) * sy)).length
		n = max(1, int(seg / 0.025))
		for t in range(n):
			dense.append(a.lerp(b, t / n))
	dense.append(Vector(pts[-1]))
	verts = []
	for k, p in enumerate(dense):
		q = dense[min(k + 1, len(dense) - 1)] - dense[max(k - 1, 0)]
		tm = Vector((q.x * sx, q.y * sy)).normalized()
		nrm = Vector((-tm.y / sx, tm.x / sy)) * width * 0.5
		verts.append(tuple(on_sheet(sheet, p.x + nrm.x, p.y + nrm.y, lift)))
		verts.append(tuple(on_sheet(sheet, p.x - nrm.x, p.y - nrm.y, lift)))
	faces = [(2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2) for k in range(len(dense) - 1)]
	return face_up(mesh_obj(name, verts, faces, m, True))


def conform(obj, sheet, lift):
	# Lays flat geometry onto the sheet's curl so nothing sinks under the paper.
	geo.apply_transform(obj)
	a = math.radians(sheet["yaw"])
	cx, cy = sheet["c"]
	sx, sy = sheet["size"]
	for v in obj.data.vertices:
		dx, dy = v.co.x - cx, v.co.y - cy
		u = (dx * math.cos(a) + dy * math.sin(a)) / sx
		w = (-dx * math.sin(a) + dy * math.cos(a)) / sy
		v.co.z = on_sheet(sheet, u, w, lift).z
	obj.data.update()
	return obj


def simplify_text(obj, deg=18.0):
	# Font outlines come out very dense; merge the fill and drop nearly straight outline points.
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(1.0), use_dissolve_boundaries=False, verts=[], edges=bm.edges[:])
	for _ in range(3):
		dead = []
		for v in bm.verts:
			es = [e for e in v.link_edges if e.is_boundary]
			if len(v.link_edges) == 2 and len(es) == 2:
				a = (es[0].other_vert(v).co - v.co).normalized()
				b = (es[1].other_vert(v).co - v.co).normalized()
				if a.dot(b) < -math.cos(math.radians(deg)):
					dead.append(v)
		dead = dead[::2]
		if dead:
			bmesh.ops.dissolve_verts(bm, verts=dead)
	bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def build():
	letter_m = mat.make("letter_paper", (0.84, 0.81, 0.70), pattern="paper", rough=0.95, bump=0.12, edge=0.05)
	map_m = mat.make("map_paper", (0.75, 0.70, 0.56), pattern="paper", rough=0.95, bump=0.12, edge=0.05)
	ink = mat.make("ink", (0.17, 0.17, 0.21), pattern="plain", rough=0.6, grime=0.0, edge=0.0, bump=0.0)
	pencil = mat.make("pencil", (0.34, 0.32, 0.29), pattern="plain", rough=0.7, grime=0.0, edge=0.0, bump=0.0)
	red = mat.make("route_red", (0.62, 0.19, 0.15), pattern="plain", rough=0.6, grime=0.0, edge=0.0, bump=0.0)
	river = mat.make("river_ink", (0.27, 0.37, 0.43), pattern="plain", rough=0.6, grime=0.0, edge=0.0, bump=0.0)
	stone = mat.make("river_stone", (0.44, 0.43, 0.40), pattern="concrete", rough=0.6, bump=0.25)
	moss = mat.get("moss_dark")

	parts = []
	paper = sheet_mesh(MAP, map_m, "map", 0.005)
	geo.tag(paper, "pulse")
	parts.append(paper)
	sheet = sheet_mesh(LETTER, letter_m, "letter", 0.009)
	geo.tag(sheet, "pulse")
	parts.append(sheet)

	# The map: the road, a river, hills, the house, and the red route running on under the fold.
	parts.append(stroke(MAP, [(-0.48, -0.36), (-0.25, -0.2), (-0.05, -0.08), (0.15, 0.06), (0.32, 0.18), (0.48, 0.3)], 0.012, pencil, "road"))
	parts.append(stroke(MAP, [(-0.1, -0.06), (-0.02, -0.3), (0.05, -0.48)], 0.009, pencil, "road"))
	parts.append(stroke(MAP, [(-0.48, 0.28), (-0.3, 0.2), (-0.12, 0.3), (0.05, 0.38), (0.2, 0.46)], 0.016, river, "river"))
	for (u, v) in ((-0.36, 0.4), (-0.24, 0.43), (0.05, 0.12), (0.4, -0.15)):
		parts.append(stroke(MAP, [(u - 0.045, v - 0.06), (u, v + 0.03), (u + 0.045, v - 0.06)], 0.008, pencil, "hill"))
	parts.append(stroke(MAP, [(-0.41, -0.3), (-0.33, -0.3), (-0.33, -0.2), (-0.37, -0.15), (-0.41, -0.2), (-0.41, -0.3)], 0.008, ink, "house"))
	parts.append(stroke(MAP, [(-0.34, -0.24), (-0.2, -0.13), (-0.02, -0.01), (0.12, 0.12), (0.28, 0.22), (0.4, 0.26)], 0.02, red, "route"))
	ring = [(0.42 + 0.05 * math.cos(a), 0.27 + 0.07 * math.sin(a)) for a in [2.0 * math.pi * k / 8 for k in range(9)]]
	parts.append(stroke(MAP, ring, 0.014, red, "route_mark"))

	# The letter: her greeting, lines of handwriting broken into words, a little mark for a signature.
	rnd = geo.rng(12)
	for line in range(8):
		v = 0.21 - line * 0.075
		u = -0.42
		end = 0.38 if line < 7 else 0.0
		while u < end:
			wl = rnd.uniform(0.07, 0.16)
			u1 = min(end, u + wl)
			pts = []
			k = 0
			uu = u
			while uu < u1:
				pts.append((uu, v + (0.007 if k % 2 else -0.002) + rnd.uniform(-0.003, 0.003)))
				uu += 0.024
				k += 1
			if len(pts) >= 2:
				parts.append(stroke(LETTER, pts, 0.0055, ink, "writing"))
			u = u1 + rnd.uniform(0.025, 0.04)
	parts.append(stroke(LETTER, [(0.16, -0.42), (0.22, -0.38), (0.26, -0.43), (0.31, -0.38), (0.37, -0.42)], 0.007, ink, "signature"))
	head = on_sheet(LETTER, -0.27, 0.355, 0.0)
	title = geo.text("하율아", 0.052, (head.x, head.y, 0.0), (0, 0, LETTER["yaw"]), mat=ink, depth=0.0, name="title")
	parts.append(face_up(conform(simplify_text(title), LETTER, 0.0022)))

	# A smooth river stone on the top-right corner, a crumb of moss on its shady side.
	sc = on_sheet(LETTER, 0.4, 0.36, 0.0)
	pebble = geo.sphere((0.062, 0.05, 0.034), (sc.x, sc.y, sc.z + 0.018), (0, 6, 30), mat=stone, segments=12, rings=8, name="stone")
	parts.append(geo.jitter(pebble, 0.008, 12.0, seed=5))
	parts.append(geo.blob((0.026, 0.02, 0.01), (sc.x - 0.018, sc.y + 0.038, sc.z + 0.016), (20, 0, 30), mat=moss, subdivisions=1,
						  lumpy=0.3, seed=2, name="moss"))

	return pipeline.static("letter", parts, size=256)
