# A Korean green direction sign on route 32: two steel posts holding a faded green panel with
# "↑ 은행나무 읍 6 km" and "↓ 물안개 마을" over the blue national-road shield for 32. The top right corner is
# bent back, rust runs down from the bolts, ivy climbs one post, moss sits along the top edge and at the feet.
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from gse import core, geo, mat, pipeline

PW, PH = 2.6, 1.4
PZ = 2.7
PY = -0.115
POST_X = 0.9
POST_Y = PY + 0.154
RAIL_Z = 0.58
FOLD = ((1.02, 0.7), (1.3, 0.42))


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


def face_shape(outline, y, m, name, scale=1.0, cx=0.0, cz=0.0):
	# A single flat face on the panel (outline in board x, z), wound to face -Y.
	area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(outline, outline[1:] + outline[:1]))
	pts = outline if area > 0.0 else list(reversed(outline))
	verts = [(cx + x * scale, y, cz + z * scale) for (x, z) in pts]
	return mesh_obj(name, verts, [tuple(range(len(verts)))], m, False)


def strip(p0, p1, width, y, m, name, depth=0.003):
	# A painted band on the panel between two board points.
	(x0, z0), (x1, z1) = p0, p1
	length = math.hypot(x1 - x0, z1 - z0)
	ang = math.degrees(math.atan2(z1 - z0, x1 - x0))
	return geo.box((length, depth, width), ((x0 + x1) * 0.5, y, (z0 + z1) * 0.5), (0, -ang, 0), mat=m, name=name)


def lettering(body, size, loc, m, budget=400, resolution=3):
	# Flat lettering facing -Y: a coarse font curve, merged and thinned, kept under a triangle budget.
	import os
	curve = bpy.data.curves.new("lettering", 'FONT')
	curve.body = body
	path = os.path.join(core.GAME_DIR, "Fonts", "NanumGothic-Bold.ttf")
	if os.path.exists(path):
		curve.font = bpy.data.fonts.load(path, check_existing=True)
	curve.size = size
	curve.resolution_u = resolution
	curve.align_x = 'CENTER'
	curve.align_y = 'CENTER'
	curve.materials.append(m)
	obj = core.link(bpy.data.objects.new("lettering", curve))
	core.select_only([obj])
	bpy.ops.object.convert(target='MESH')
	obj = bpy.context.view_layer.objects.active
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(4.0), use_dissolve_boundaries=False, verts=bm.verts[:], edges=bm.edges[:])
	bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
	bm.normal_update()
	if sum(f.normal.z for f in bm.faces) < 0.0:
		bmesh.ops.reverse_faces(bm, faces=bm.faces[:])
	bm.to_mesh(obj.data)
	bm.free()
	obj = geo.decimate(obj, budget)
	obj.location = Vector(loc)
	obj.rotation_euler = (math.radians(90.0), 0.0, 0.0)
	return obj


def leaf_card(p, out, size, m):
	# A small cupped ivy leaf pointing up and away from the post.
	up = Vector((0, 0, 1))
	along = (out * 0.55 + up * 0.85).normalized()
	side = out.cross(up).normalized()
	nrm = side.cross(along).normalized()
	if nrm.dot(out) < 0.0:
		nrm = -nrm
	c = p + along * size * 0.45 + nrm * size * 0.12
	vs = [p, p + along * size * 0.4 + side * size * 0.38, p + along * size, p + along * size * 0.4 - side * size * 0.38, c]
	faces = [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)]
	obj = mesh_obj("ivy_leaf", [tuple(v) for v in vs], faces, m, True)
	for f in obj.data.polygons:
		if Vector(f.normal).dot(nrm) < 0.0:
			f.flip()
	geo.tag(obj, "sway", "foliage")
	return obj


def moss_pad(radii, loc, rot, m, seed, segments=10):
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], loc, rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.3, 6.0, seed=seed)


def floor(obj):
	# Ground pieces stop just under the ground plane, so the model's bottom stays at z = 0.
	geo.apply_transform(obj)
	for v in obj.data.vertices:
		v.co.z = max(v.co.z, -0.01)
	obj.data.update()
	return obj


def build():
	green = mat.make("roadsign_green", (0.19, 0.37, 0.28), pattern="rust", rough=0.62, bump=0.3, amount=0.35)
	white = mat.make("roadsign_white", (0.80, 0.80, 0.74), rough=0.55, bump=0.05, grime=0.45)
	blue = mat.make("roadsign_blue", (0.18, 0.30, 0.50), pattern="rust", rough=0.55, bump=0.2, amount=0.25)
	back = mat.make("roadsign_back", (0.43, 0.44, 0.43), pattern="rust", rough=0.7, bump=0.35, amount=0.8)
	steel = mat.make("roadsign_steel", (0.42, 0.43, 0.42), pattern="rust", rough=0.6, bump=0.3, amount=0.6)
	streak = mat.make("roadsign_streak", (0.42, 0.26, 0.17), pattern="plain", rough=0.85, bump=0.0, edge=0.0)
	rust = mat.get("rust")
	metal = mat.get("metal")
	concrete = mat.get("concrete")
	vine = mat.get("bark")
	ivy = mat.make("roadsign_ivy", (0.20, 0.33, 0.20), pattern="leaf", rough=0.75, bump=0.4)
	ivy_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	glow = mat.get("spore_glow")
	rnd = geo.rng(32)

	hw, hh = PW * 0.5, PH * 0.5
	(fx0, fz0), (fx1, fz1) = FOLD
	board = []
	flap = []

	# Panel and its grey back, the top right corner cut off along the fold and hung on as its own flap.
	main = [(-hw, -hh), (hw, -hh), (fx1, fz1), (fx0, fz0), (-hw, hh)]
	corner = [(fx0, fz0), (fx1, fz1), (hw, hh)]
	panel = geo.polygon_slab(main, 0.03, (0, 0.03, 0), (90, 0, 0), mat=green, name="panel")
	board.append(panel)
	board.append(geo.polygon_slab(main, 0.004, (0, 0.034, 0), (90, 0, 0), mat=back, name="panel_back"))
	flap.append(geo.polygon_slab(corner, 0.03, (0, 0.03, 0), (90, 0, 0), mat=green, name="panel"))
	flap.append(geo.polygon_slab(corner, 0.004, (0, 0.034, 0), (90, 0, 0), mat=back, name="panel_back"))

	# White border inset from the edge, broken at the fold, and a rule between the two rows.
	bi = 0.045 + 0.0175
	bx, bz = hw - bi, hh - bi
	ftop = (fx0 + fz0) - bz
	fright = (fx0 + fz0) - bx
	board.append(strip((-bx - 0.0175, -bz), (bx + 0.0175, -bz), 0.035, -0.003, white, "border"))
	board.append(strip((-bx, -bz), (-bx, bz + 0.0175), 0.035, -0.003, white, "border"))
	board.append(strip((-bx, bz), (ftop, bz), 0.035, -0.003, white, "border"))
	board.append(strip((bx, -bz), (bx, fright), 0.035, -0.003, white, "border"))
	flap.append(strip((ftop, bz), (bx + 0.0175, bz), 0.035, -0.003, white, "border"))
	flap.append(strip((bx, fright), (bx, bz), 0.035, -0.003, white, "border"))
	board.append(strip((-bx + 0.06, 0.015), (bx - 0.06, 0.015), 0.022, -0.003, white, "rule"))

	# Row one: up arrow, 은행나무 읍, 6 km. Row two: down arrow, 물안개 마을, the route 32 shield.
	r1, r2 = 0.335, -0.31
	arrow = [(-0.04, -0.20), (0.04, -0.20), (0.04, 0.04), (0.11, 0.04), (0.0, 0.20), (-0.11, 0.04), (-0.04, 0.04)]
	board.append(face_shape(arrow, -0.005, white, "arrow_up", cx=-1.10, cz=r1))
	board.append(face_shape([(x, -z) for (x, z) in arrow], -0.005, white, "arrow_down", cx=-1.10, cz=r2))
	board.append(lettering("은행나무 읍", 0.345, (-0.11, -0.005, r1), white))
	board.append(lettering("물안개 마을", 0.345, (-0.11, -0.005, r2), white))
	board.append(lettering("6 km", 0.18, (1.02, -0.005, r1 - 0.03), white, budget=200))
	shield = [(-0.5, 0.5), (-0.5, 0.0), (-0.42, -0.3), (-0.22, -0.52), (0.0, -0.62), (0.22, -0.52), (0.42, -0.3), (0.5, 0.0),
			  (0.5, 0.5), (0.28, 0.5), (0.0, 0.43), (-0.28, 0.5)]
	board.append(face_shape(shield, -0.005, white, "shield_rim", scale=0.42, cx=1.0, cz=r2 + 0.02))
	board.append(face_shape(shield, -0.0065, blue, "shield", scale=0.36, cx=1.0, cz=r2 + 0.02))
	board.append(lettering("32", 0.22, (1.0, -0.008, r2 + 0.04), white, budget=200))

	# Bolts through the face onto the rails behind, each weeping a rust streak down the paint.
	for sx in (-1.0, 1.0):
		for (z, run) in ((RAIL_Z, 0.30 if sx < 0 else 0.22), (-RAIL_Z, 0.10)):
			x = sx * POST_X
			board.append(geo.cylinder(0.026, 0.012, (x, -0.006, z), (90, 0, 0), mat=metal, segments=6, name="bolt"))
			board.append(geo.box((0.03, 0.0012, run), (x + 0.004, -0.0008, z - run * 0.5), mat=streak, name="streak"))
			board.append(geo.box((0.012, 0.0012, run * 0.6), (x - 0.02, -0.0008, z - run * 0.3), mat=streak, name="streak"))
	for sz in (-1.0, 1.0):
		board.append(geo.box((PW - 0.3, 0.04, 0.07), (0, 0.054, sz * RAIL_Z), mat=steel, name="rail"))

	# The bent corner swings back about the fold line.
	pivot = Vector((fx0, 0.0, fz0))
	axis = Vector((fx1 - fx0, 0.0, fz1 - fz0)).normalized()
	bend = Matrix.Translation(pivot) @ Matrix.Rotation(math.radians(-55.0), 4, axis) @ Matrix.Translation(-pivot)
	place = Matrix.Translation((0.0, PY, PZ))
	bpy.context.view_layer.update()
	for p in flap:
		p.matrix_world = place @ bend @ p.matrix_world
	for p in board:
		p.matrix_world = place @ p.matrix_world
	parts = board + flap

	# Posts with clamps at the rails, capped, on cracked concrete footings.
	man_made = [panel]
	for sx in (-1.0, 1.0):
		x = sx * POST_X
		post = geo.lathe([(0.08, 0.0), (0.08, PZ + 0.62), (0.07, PZ + 0.66), (0.0, PZ + 0.68)], (x, POST_Y, 0), mat=steel, segments=10, name="post")
		parts.append(post)
		man_made.append(post)
		for sz in (-1.0, 1.0):
			parts.append(geo.cylinder(0.092, 0.08, (x, POST_Y, PZ + sz * RAIL_Z), mat=rust, segments=10, caps=False, name="clamp"))
		foot = geo.cylinder(0.16, 0.14, (x, POST_Y, 0.05), mat=concrete, segments=10, name="footing")
		parts.append(floor(geo.jitter(foot, 0.012, 4.0, seed=int(3 + sx))))
		man_made.append(foot)
		parts.append(geo.box((0.03, 0.004, 0.40), (x + sx * 0.081, POST_Y, PZ - RAIL_Z - 0.24), (0, 0, 90), mat=streak, name="streak"))

	# Moss along the top edge, drooping a little over the face.
	for (x, w, m, seed) in ((-0.62, 0.62, moss, 3), (0.42, 0.40, moss_dark, 4)):
		top = moss_pad((w, 0.075, 0.075), (x, PY + 0.015, PZ + hh - 0.012), (0, 0, 0), m, seed, segments=12)
		parts.append(top)
	parts.append(moss_pad((0.12, 0.06, 0.05), (-hw + 0.04, PY + 0.01, PZ + hh - 0.10), (0, 70, 0), moss, 5, segments=8))

	# Ivy up the left post and onto the panel's bottom edge.
	pts = []
	n = 12
	for k in range(n + 1):
		t = k / n
		a = 0.4 + 2.6 * 2.0 * math.pi * t
		r = 0.092 + 0.005 * math.sin(k * 1.7)
		pts.append((-POST_X + math.cos(a) * r, POST_Y + math.sin(a) * r, 0.14 + (PZ - hh - 0.18) * t))
	pts.append((-POST_X - 0.10, PY - 0.03, PZ - hh + 0.02))
	pts.append((-POST_X - 0.32, PY - 0.03, PZ - hh + 0.10))
	radii = [0.018 * (1.0 - 0.6 * k / (n + 2)) for k in range(n + 3)]
	parts.append(geo.tube(pts, radii, mat=vine, segments=4, name="vine"))
	for k in range(1, n + 3):
		for j in range(2 if k < n else 1):
			p = Vector(pts[k])
			if k <= n:
				a = 0.4 + 2.6 * 2.0 * math.pi * k / n + rnd.uniform(-0.6, 0.6)
				out = Vector((math.cos(a), math.sin(a), 0.0))
			else:
				out = Vector((rnd.uniform(-0.3, 0.3), -1.0, 0.0)).normalized()
			p = p + out * 0.01 + Vector((0, 0, rnd.uniform(-0.05, 0.05)))
			parts.append(leaf_card(p, out, rnd.uniform(0.13, 0.18), ivy if rnd.random() < 0.6 else ivy_dark))
	for k, (a, z, s) in enumerate(((0.9, 0.26, 0.15), (3.6, 0.55, 0.12), (5.4, 1.10, 0.10))):
		c = geo.blob((s, s * 0.8, s * 0.9), (-POST_X + math.cos(a) * 0.10, POST_Y + math.sin(a) * 0.10, z), mat=ivy if k % 2 else ivy_dark,
					 subdivisions=1, lumpy=0.35, seed=k + 7, name="ivy_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	# Moss and grass round both feet.
	for (sx, seed) in ((-1.0, 9), (1.0, 10)):
		pad = floor(moss_pad((0.30, 0.24, 0.10), (sx * POST_X + sx * 0.03, POST_Y, 0.0), (0, 0, sx * 20), moss if sx < 0 else moss_dark, seed, segments=12))
		if sx < 0:
			geo.tag(pad, "pulse")
		parts.append(pad)
		for j in range(5):
			h = rnd.uniform(0.18, 0.38)
			a = rnd.uniform(0.0, 2.0 * math.pi)
			r = rnd.uniform(0.2, 0.32)
			g = geo.cone(rnd.uniform(0.03, 0.05), h, (sx * POST_X + math.cos(a) * r, POST_Y + math.sin(a) * r, h * 0.5),
						 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=4, name="grass")
			geo.tag(g, "sway", "foliage")
			parts.append(g)
	g = geo.blob(0.03, (POST_X + 0.22, POST_Y - 0.16, 0.10), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 6)

	res = pipeline.static("road_sign", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
