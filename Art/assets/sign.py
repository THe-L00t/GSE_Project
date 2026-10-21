# The route sign at the village edge: a faded green guide board, route 32 in its blue national-road shield and
# "은행나무읍" with an arrow, knocked round on its steel pole; ivy climbs the pole and moss sits along the top.
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from gse import core, geo, mat, pipeline

BW, BH = 1.6, 0.7
BOARD_Z = 1.95
POLE_H = 2.24
# Knocked askew toward the quarter-view camera, so "32 은행나무읍" still reads in game.
TWIST = 0.5
ROLL = 3.5


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


def face_plate(outline, depth, m, name, scale=1.0, cx=0.0, cz=0.0):
	# A flat shape on the board face (outline in board x, z), facing -Y.
	pts = [(cx + x * scale, cz + z * scale) for (x, z) in outline]
	return geo.polygon_slab(pts, 0.003, (0, -depth, 0), (90, 0, 0), mat=m, name=name)


def leaf_card(p, out, size, m):
	# A small cupped ivy leaf pointing up and away from the pole.
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


def build():
	green = mat.make("sign_green", (0.16, 0.35, 0.26), pattern="rust", rough=0.6, bump=0.3, amount=0.35)
	white = mat.make("sign_white", (0.82, 0.83, 0.78), pattern="plain", rough=0.55, bump=0.05, grime=0.45)
	blue = mat.make("route_blue", (0.17, 0.29, 0.48), pattern="rust", rough=0.55, bump=0.2, amount=0.25)
	back = mat.make("sign_back", (0.42, 0.43, 0.42), pattern="rust", rough=0.7, bump=0.35, amount=0.8)
	steel = mat.make("pole_steel", (0.40, 0.41, 0.40), pattern="rust", rough=0.6, bump=0.3, amount=0.6)
	clamp_m = mat.get("metal")
	concrete = mat.get("concrete")
	vine = mat.get("bark")
	ivy = mat.make("ivy", (0.20, 0.33, 0.20), pattern="leaf", rough=0.75, bump=0.4)
	ivy_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")

	parts = []

	# The board, built facing -Y about its own centre, then knocked round the pole.
	board = []
	board.append(geo.box((BW, 0.035, BH), (0, 0.018, 0), mat=back, name="board_back"))
	board.append(geo.box((BW - 0.01, 0.004, BH - 0.01), (0, -0.002, 0), mat=green, name="board"))
	inset, bw = 0.035, 0.022
	for (x, z, w, h) in ((0, BH * 0.5 - inset, BW - 2 * inset, bw), (0, -BH * 0.5 + inset, BW - 2 * inset, bw),
						 (-BW * 0.5 + inset, 0, bw, BH - 2 * inset), (BW * 0.5 - inset, 0, bw, BH - 2 * inset)):
		board.append(geo.box((w, 0.003, h), (x, -0.005, z), mat=white, name="border"))
	shield = [(-0.5, 0.5), (-0.5, 0.0), (-0.42, -0.3), (-0.22, -0.52), (0.0, -0.62), (0.22, -0.52), (0.42, -0.3), (0.5, 0.0),
			  (0.5, 0.5), (0.28, 0.5), (0.0, 0.43), (-0.28, 0.5)]
	board.append(face_plate(shield, 0.006, white, "shield_rim", scale=0.4, cx=-0.52, cz=0.06))
	board.append(face_plate(shield, 0.009, blue, "shield", scale=0.35, cx=-0.52, cz=0.06))
	num = geo.text("32", 0.17, (-0.52, -0.0125, 0.075), (90, 0, 0), mat=white, depth=0.0, name="route_no")
	board.append(simplify_text(num, 14.0))
	town = geo.text("은행나무읍", 0.19, (0.24, -0.008, 0.12), (90, 0, 0), mat=white, depth=0.0, name="town")
	board.append(simplify_text(town))
	arrow = [(-0.3, -0.03), (0.13, -0.03), (0.13, -0.085), (0.3, 0.0), (0.13, 0.085), (0.13, 0.03), (-0.3, 0.03)]
	board.append(face_plate(arrow, 0.006, white, "arrow", scale=1.0, cx=0.27, cz=-0.16))
	for z in (-0.2, 0.2):
		board.append(geo.box((0.12, 0.05, 0.05), (0, 0.06, z), mat=clamp_m, name="bracket"))
	place = Matrix.Rotation(TWIST, 4, 'Z') @ Matrix.Translation((0, -0.085, BOARD_Z)) @ Matrix.Rotation(math.radians(ROLL), 4, 'Y')
	bpy.context.view_layer.update()
	for p in board:
		p.matrix_world = place @ p.matrix_world
	parts += board

	# Moss along the top edge of the board, drooping a little over the face.
	top = moss_pad((0.55, 0.07, 0.07), (0.15, -0.005, BH * 0.5 - 0.01), (0, 0, 0), moss, 3, segments=12)
	top.matrix_world = place @ top.matrix_world
	geo.tag(top, "pulse")
	parts.append(top)
	tuft = moss_pad((0.2, 0.06, 0.06), (-0.55, 0.0, BH * 0.5 - 0.005), (0, 0, 0), moss_dark, 4)
	tuft.matrix_world = place @ tuft.matrix_world
	parts.append(tuft)

	# Steel pole with clamp bands, on a cracked concrete footing.
	parts.append(geo.cylinder(0.045, POLE_H, (0, 0, POLE_H * 0.5), mat=steel, segments=12, name="pole"))
	parts.append(geo.sphere((0.05, 0.05, 0.03), (0, 0, POLE_H), mat=steel, segments=12, rings=6, name="pole_cap"))
	for z in (BOARD_Z - 0.2, BOARD_Z + 0.2):
		parts.append(geo.cylinder(0.055, 0.045, (0, 0, z), mat=clamp_m, segments=12, name="clamp"))
	foot = geo.cylinder(0.16, 0.1, (0, 0, 0.03), mat=concrete, segments=12, name="footing")
	parts.append(geo.jitter(foot, 0.012, 4.0, seed=2))

	# Ivy: two stems winding up the pole with leaves, clumps where it thickens.
	rnd = geo.rng(21)
	for (turns, height, a0, seed) in ((2.4, 1.65, 0.3, 1), (1.4, 1.05, 3.4, 2)):
		pts = []
		n = 22
		for k in range(n + 1):
			t = k / n
			a = a0 + turns * 2.0 * math.pi * t
			r = 0.057 + 0.006 * math.sin(k * 1.7)
			pts.append((math.cos(a) * r, math.sin(a) * r, 0.06 + height * t))
		radii = [0.014 * (1.0 - 0.6 * k / n) for k in range(n + 1)]
		parts.append(geo.tube(pts, radii, mat=vine, segments=5, name="vine"))
		for k in range(1, n + 1, 1):
			if rnd.random() < 0.85:
				t = k / n
				a = a0 + turns * 2.0 * math.pi * t + rnd.uniform(-0.4, 0.4)
				out = Vector((math.cos(a), math.sin(a), 0.0))
				p = Vector(pts[k]) + out * 0.01
				parts.append(leaf_card(p, out, rnd.uniform(0.06, 0.09) * (1.0 - 0.3 * t), ivy if rnd.random() < 0.6 else ivy_dark))
	for k, (a, z, s) in enumerate(((0.8, 0.25, 0.13), (3.0, 0.4, 0.11), (5.0, 0.75, 0.09), (1.9, 1.15, 0.08))):
		c = geo.blob((s, s * 0.8, s * 0.9), (math.cos(a) * 0.09, math.sin(a) * 0.09, z), mat=ivy if k % 2 else ivy_dark,
					 subdivisions=1, lumpy=0.35, seed=k + 5, name="ivy_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	# Moss and grass round the foot.
	parts.append(moss_pad((0.32, 0.26, 0.1), (0.04, 0.02, 0.0), (0, 0, 20), moss_dark, 7, segments=12))
	for j in range(7):
		h = rnd.uniform(0.18, 0.36)
		a = rnd.uniform(0.0, 2.0 * math.pi)
		r = rnd.uniform(0.18, 0.3)
		g = geo.cone(rnd.uniform(0.03, 0.05), h, (math.cos(a) * r, math.sin(a) * r, h * 0.5),
					 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=4, name="grass")
		geo.tag(g, "sway", "foliage")
		parts.append(g)

	for p in parts:
		if p.name.startswith(("board_back", "pole", "footing")):
			geo.moss_mask(p, seed=6)

	return pipeline.static("sign", parts, size=512)
