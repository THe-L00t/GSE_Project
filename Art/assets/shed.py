# The heotgan: a shed of weathered vertical boards under rusty corrugated tin, its door hanging half open
# on the dark inside, a shovel and a hoe leaning by the door, a rusty jerry can, moss creeping up.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

WX, WY = 1.5, 1.4
EAVE_H = 1.95
RIDGE_Z = 2.5
SLOPE = (RIDGE_Z - EAVE_H) / WY
RX, RY = WX + 0.16, WY + 0.26
PERIOD = 0.16
AMP = 0.02


def plane(ay):
	return RIDGE_Z - SLOPE * ay


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


def sweep(name, points, profile, m, cap_start=False, cap_end=False, smooth=True):
	# Sweeps a 2D (side, up) profile along a path; profiles run clockwise for outward faces.
	pts = [Vector(p) for p in points]
	n = len(profile)
	verts = []
	for i, p in enumerate(pts):
		t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
		side = t.cross(Vector((0, 0, 1))).normalized()
		up = side.cross(t).normalized()
		for (s, u) in profile:
			verts.append(p + side * s + up * u)
	faces = []
	for i in range(len(pts) - 1):
		for k in range(n - 1):
			faces.append((i * n + k, i * n + k + 1, (i + 1) * n + k + 1, (i + 1) * n + k))
	if cap_start:
		faces.append(tuple(reversed(range(n))))
	if cap_end:
		last = (len(pts) - 1) * n
		faces.append(tuple(last + k for k in range(n)))
	return mesh_obj(name, verts, faces, m, smooth)


def drop_faces(obj, fn):
	# Faces nobody sees (against the dark core, on the ground) would only waste texture space.
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


def tin_sheet(x0, x1, sgn, m, lift=0.0, seed=0):
	# One corrugated sheet from the ridge to the eave; lift curls its outer eave corner up.
	cols = max(2, int(round((x1 - x0) / (PERIOD / 4.0))))
	rows = 3
	verts = []
	for j in range(rows + 1):
		ay = RY * j / rows
		for i in range(cols + 1):
			x = x0 + (x1 - x0) * i / cols
			z = plane(ay) + 0.03 + AMP * math.sin(2.0 * math.pi * x / PERIOD)
			if lift > 0.0:
				u = (x - x0) / (x1 - x0)
				z += lift * (u ** 2) * (ay / RY) ** 2
			verts.append((x, sgn * ay, z))
	faces = []
	w = cols + 1
	for j in range(rows):
		for i in range(cols):
			a, b, c, d = j * w + i, j * w + i + 1, (j + 1) * w + i + 1, (j + 1) * w + i
			faces.append((a, b, c, d) if sgn > 0 else (d, c, b, a))
	return mesh_obj("tin", verts, faces, m, True)


def moss_pad(radii, x, y, z, rot, m, seed, segments=10):
	# A soft cushion: domed, edges rolled under, lumped by low-frequency noise.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, z), rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.22, 2.0, seed=seed)


def build():
	plank_a = mat.make("shed_plank", (0.37, 0.32, 0.26), pattern="wood", rough=0.9, bump=0.5)
	plank_b = mat.get("wood_grey")
	plank_c = mat.make("shed_plank_dark", (0.27, 0.23, 0.19), pattern="wood", rough=0.9, bump=0.5)
	batten = mat.get("wood_dark")
	dark = mat.make("shed_dark", (0.04, 0.035, 0.03), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	tin = mat.get("tin")
	tin_old = mat.make("tin_old", (0.33, 0.27, 0.23), pattern="rust", accent="rust_dark", rough=0.85, bump=0.45, amount=1.3)
	rust = mat.get("rust")
	stone = mat.get("stone_dark")
	handle_m = mat.make("tool_handle", (0.45, 0.37, 0.27), pattern="wood", rough=0.8, bump=0.3)
	can_m = mat.make("jerry_can", (0.40, 0.25, 0.18), pattern="rust", accent="rust_dark", rough=0.8, bump=0.45, amount=1.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")

	parts = []
	rnd = geo.rng(3)

	# A dark core: what shows through the board gaps and the open door.
	parts.append(drop_faces(geo.box((2 * WX - 0.1, 2 * WY - 0.1, EAVE_H), (0, 0, EAVE_H * 0.5), mat=dark, name="core"),
							lambda n: abs(n.z) > 0.9))
	parts.append(geo.prism((2 * WX - 0.1, 2 * WY - 0.1, RIDGE_Z - EAVE_H - 0.02), (0, 0, EAVE_H), mat=dark, name="core_gable"))
	parts.append(drop_faces(geo.box((2 * WX + 0.3, 2 * WY + 0.3, 0.08), (0, -0.05, 0.04), mat=stone, name="footing"),
							lambda n: n.z < -0.9))

	# Vertical boards, a little uneven, round all four walls; the door gap stays open.
	def boards(axis, fixed, lo, hi, outward):
		k = 0
		u = lo
		while u < hi - 0.02:
			w = min(rnd.uniform(0.15, 0.2), hi - u)
			c = u + w * 0.5
			if axis == 'x' and outward < 0 and abs(c) < 0.52:
				u += w + 0.012
				continue
			ay = abs(c) if axis == 'y' else WY
			top = plane(ay) - 0.02 if axis == 'y' else EAVE_H + 0.02
			bottom = 0.06 + rnd.uniform(0.0, 0.05)
			m = (plank_a, plank_b, plank_a, plank_c)[k % 4] if rnd.random() < 0.8 else plank_c
			if axis == 'x':
				b = geo.box((w, 0.03, top - bottom), (c, fixed + outward * 0.015, (top + bottom) * 0.5), (0, rnd.uniform(-1, 1), 0), mat=m, name="board")
			else:
				b = geo.box((0.03, w, top - bottom), (fixed + outward * 0.015, c, (top + bottom) * 0.5), (rnd.uniform(-1, 1), 0, 0), mat=m, name="board")
			parts.append(drop_faces(b, lambda n, o=outward, a=axis: n.z < -0.9 or (a == 'x' and n.y * o < -0.9) or (a == 'y' and n.x * o < -0.9)))
			u += w + 0.012
			k += 1

	boards('x', -WY, -WX, WX, -1.0)
	boards('x', WY, -WX, WX, 1.0)
	boards('y', -WX, -WY, WY, -1.0)
	boards('y', WX, -WY, WY, 1.0)
	for z in (0.35, 1.6):
		for sy in (-1.0, 1.0):
			for (x0, x1) in ((-WX, -0.53), (0.53, WX)) if sy < 0 else ((-WX, WX),):
				parts.append(geo.box((x1 - x0, 0.03, 0.09), ((x0 + x1) * 0.5, sy * (WY + 0.045), z), mat=batten, name="batten"))
		for sx in (-1.0, 1.0):
			parts.append(geo.box((0.03, 2 * WY, 0.09), (sx * (WX + 0.045), 0, z), mat=batten, name="batten"))
	for (x, y) in ((-WX, -WY), (WX, -WY), (-WX, WY), (WX, WY)):
		parts.append(geo.box((0.1, 0.1, EAVE_H + 0.05), (x, y, (EAVE_H + 0.05) * 0.5), mat=batten, name="corner"))
	parts.append(geo.box((1.12, 0.06, 0.1), (0, -WY - 0.04, 1.83), mat=batten, name="lintel"))
	for x in (-0.55, 0.55):
		parts.append(geo.box((0.06, 0.06, 1.8), (x, -WY - 0.04, 0.95), mat=batten, name="jamb"))

	# The door: planks on a Z brace, hung from the right jamb and swung half open.
	hinge = Vector((-0.52, -WY - 0.06, 0.0))
	ang = math.radians(-75.0)
	d = Vector((math.cos(ang), math.sin(ang), 0.0))
	for k in range(5):
		t = 0.1 + 0.2 * k
		p = hinge + d * t
		parts.append(geo.box((0.19, 0.035, 1.7 - 0.03 * (k % 2)), (p.x, p.y, 0.95), (0, 0, math.degrees(ang)),
							 mat=(plank_a, plank_c)[k % 2], name="door"))
	nrm = Vector((-math.sin(ang), math.cos(ang), 0.0))
	for z in (0.4, 1.5):
		p = hinge + d * 0.5 + nrm * 0.03
		parts.append(geo.box((0.95, 0.03, 0.09), (p.x, p.y, z), (0, 0, math.degrees(ang)), mat=batten, name="door_rail"))
	p = hinge + d * 0.5 + nrm * 0.03
	parts.append(geo.box((0.09, 0.03, 1.28), (p.x, p.y, 0.95), (0, -38, math.degrees(ang)), mat=batten, name="door_brace"))
	for z in (0.4, 1.5):
		parts.append(geo.box((0.12, 0.02, 0.04), (-0.5, -WY - 0.08, z), mat=rust, name="hinge"))

	# Rusty tin roof: three sheets a side, one curled up by the wind, a bent ridge cap.
	edges = [-RX, -0.52, 0.58, RX]
	for sgn in (-1.0, 1.0):
		for k in range(3):
			lift = 0.18 if (sgn < 0 and k == 2) else 0.0
			m = tin_old if (sgn > 0 and k == 0) or (sgn < 0 and k == 1) else tin
			parts.append(tin_sheet(edges[k] - 0.03, edges[k + 1] + 0.03, sgn, m, lift))
	cap = [(-0.17, -0.04), (0.0, 0.06), (0.17, -0.04)]
	parts.append(sweep("tin_cap", [(-RX - 0.02, 0, RIDGE_Z + 0.05), (0, 0, RIDGE_Z + 0.035), (RX + 0.02, 0, RIDGE_Z + 0.05)], cap, tin_old,
					   cap_start=True, cap_end=True))

	# Shovel and hoe leaning on the wall right of the door.
	parts.append(geo.tube([(0.95, -WY - 0.42, 0.18), (0.82, -WY - 0.06, 1.32)], 0.022, mat=handle_m, segments=6, name="shovel_handle"))
	blade = geo.polygon_slab([(-0.13, 0.0), (0.13, 0.0), (0.12, -0.2), (0.0, -0.32), (-0.12, -0.2)], 0.012,
							 (0.933, -WY - 0.373, 0.33), (72, 0, 0), mat=rust, name="shovel_blade")
	parts.append(blade)
	parts.append(geo.tube([(0.68, -WY - 0.36, 0.0), (0.62, -WY - 0.06, 1.45)], 0.022, mat=handle_m, segments=6, name="hoe_handle"))
	parts.append(geo.box((0.2, 0.26, 0.012), (0.62, -WY - 0.138, 1.346), (-127, 0, 0), mat=rust, name="hoe_blade"))
	parts.append(geo.cylinder(0.035, 0.08, (0.62, -WY - 0.07, 1.45), (-20, 0, 0), mat=mat.get("metal"), segments=6, name="hoe_eye"))

	# A rusty jerry can against the right wall.
	can_c = Vector((WX + 0.2, -0.75, 0.23))
	yaw = 78.0
	along = Vector((-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0.0))
	across = Vector((math.cos(math.radians(yaw)), math.sin(math.radians(yaw)), 0.0))
	parts.append(geo.box((0.17, 0.36, 0.46), tuple(can_c), (0, 0, yaw), mat=can_m, name="jerrycan"))
	for dz in (-0.09, 0.0, 0.09):
		h = can_c + along * dz + Vector((0, 0, 0.26))
		parts.append(geo.box((0.03, 0.03, 0.06), tuple(h), (0, 0, yaw), mat=can_m, name="can_handle"))
	sp = can_c - along * 0.14 + Vector((0, 0, 0.25))
	parts.append(geo.cylinder(0.03, 0.05, tuple(sp), mat=mat.get("metal"), segments=6, name="can_spout"))
	for sx in (-1.0, 1.0):
		v = can_c + across * sx * 0.087
		parts.append(geo.box((0.012, 0.3, 0.012), tuple(v), (35, 0, yaw), mat=can_m, name="can_x"))
		parts.append(geo.box((0.012, 0.3, 0.012), tuple(v), (-35, 0, yaw), mat=can_m, name="can_x"))

	# Moss: on the tin, round the footing and up the boards; weeds at the corners.
	ang_r = math.degrees(math.atan(SLOPE))
	for k, (x, y, radii, m) in enumerate(((-0.9, -1.1, (0.5, 0.38, 0.09), moss), (0.2, -0.5, (0.3, 0.24, 0.08), moss_dark),
										  (0.6, 1.0, (0.55, 0.42, 0.1), moss), (-1.0, 0.7, (0.3, 0.25, 0.08), moss_dark))):
		rot = (ang_r if y < 0 else -ang_r, 0, 0)
		pad = moss_pad(radii, x, y, plane(abs(y)) + 0.0, rot, m, 60 + k)
		if k == 2:
			geo.tag(pad, "pulse")
		parts.append(pad)
	for k, (x, y, radii) in enumerate(((-WX - 0.1, -0.4, (0.16, 0.7, 0.2)), (-1.1, -WY - 0.12, (0.4, 0.14, 0.16)),
									   (WX + 0.1, 0.6, (0.14, 0.6, 0.16)), (0.4, WY + 0.12, (0.7, 0.15, 0.18)),
									   (-0.85, -WY - 0.14, (0.25, 0.12, 0.12)))):
		parts.append(moss_pad(radii, x, y, 0.0, (0, 0, 0), moss_dark if k % 2 else moss, 70 + k, segments=8))
	parts.append(moss_pad((0.28, 0.38, 0.06), -0.3, WY + 0.04, 0.3, (-90, 0, 0), moss_dark, 80, segments=8))
	for k, (cx, cy) in enumerate(((-WX - 0.1, -WY - 0.1), (WX + 0.12, -WY - 0.05), (WX + 0.05, WY + 0.1), (-WX - 0.1, WY + 0.05), (-0.9, -WY - 0.55))):
		for j in range(3):
			h = rnd.uniform(0.18, 0.38)
			wd = geo.cone(rnd.uniform(0.04, 0.07), h, (cx + rnd.uniform(-0.1, 0.1), cy + rnd.uniform(-0.1, 0.1), h * 0.5),
						  (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=4, name="weed")
			geo.tag(wd, "sway", "foliage")
			parts.append(wd)

	for p in parts:
		if p.name.startswith(("board", "tin", "batten", "corner", "door", "footing", "jerrycan")):
			geo.moss_mask(p, seed=9)

	res = pipeline.static("shed", parts, size=512)
	res["unit"] = (3.0, 2.4, 2.8)
	return res
