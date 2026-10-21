# Grandmother Sunim's farmhouse: an old giwa roof with upturned corners over plaster walls set between dark
# posts on a stone plinth, a wooden toenmaru along the front, paper-grid sliding doors, a stovepipe and firewood.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

WX, WY = 3.5, 3.0
PLINTH = 0.3
WALL_TOP = 2.4
RX, RY = WX + 0.6, WY + 0.6
NX, NY = 24, 12
# End slopes are steeper so the hip creases fall exactly on the grid diagonals.
KX = (RY * NX) / (RX * NY)
EAVE_Z = 2.25
RIDGE_Z = 4.1
LIFT = 0.34
HIP_X = RX - RY / KX


def roof_height(x, y):
	d = min(RY - abs(y), (RX - abs(x)) * KX)
	t = max(0.0, min(1.0, d / RY))
	# Concave slope (flat at the eaves, steep near the ridge) and corners that lift.
	return EAVE_Z + (RIDGE_Z - EAVE_Z) * t ** 1.35 + LIFT * (abs(x) / RX) ** 3 * (abs(y) / RY) ** 3


def roof_normal(x, y):
	e = 0.02
	hx = (roof_height(x + e, y) - roof_height(x - e, y)) / (2 * e)
	hy = (roof_height(x, y + e) - roof_height(x, y - e)) / (2 * e)
	return Vector((-hx, -hy, 1.0)).normalized()


def mesh_obj(name, verts, faces, m, smooth=True, sharp=None):
	bm = bmesh.new()
	bv = [bm.verts.new(v) for v in verts]
	for f in faces:
		bm.faces.new([bv[i] for i in f])
	if sharp:
		bm.edges.ensure_lookup_table()
		for e in bm.edges:
			if sharp(e.verts[0].co, e.verts[1].co):
				e.smooth = False
	me = bpy.data.meshes.new(name)
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, me))
	if m is not None:
		me.materials.append(m)
	for p in me.polygons:
		p.use_smooth = smooth
	return obj


def sweep(name, points, profile, m, closed=False, cap_start=False, cap_end=False, smooth=True, scales=None):
	# Sweeps a 2D (side, up) profile along a path; profiles run clockwise for outward faces.
	pts = [Vector(p) for p in points]
	scales = scales or [1.0] * len(pts)
	n = len(profile)
	verts = []
	for i, p in enumerate(pts):
		a = pts[max(i - 1, 0)]
		b = pts[min(i + 1, len(pts) - 1)]
		t = (b - a).normalized()
		side = t.cross(Vector((0, 0, 1))).normalized()
		up = side.cross(t).normalized()
		for (s, u) in profile:
			verts.append(p + (side * s + up * u) * scales[i])
	faces = []
	for i in range(len(pts) - 1):
		for k in range(n if closed else n - 1):
			k2 = (k + 1) % n
			faces.append((i * n + k, i * n + k2, (i + 1) * n + k2, (i + 1) * n + k))
	if cap_start:
		faces.append(tuple(reversed(range(n))))
	if cap_end:
		last = (len(pts) - 1) * n
		faces.append(tuple(last + k for k in range(n)))
	return mesh_obj(name, verts, faces, m, smooth)


def moss_pad(radii, x, y, z, rot, m, seed, segments=11):
	# A soft cushion: domed, edges rolled under, lumped by low-frequency noise.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, z), rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.22, 1.6, seed=seed)


def eave_moss(x0, x1, side, m, seed):
	# Moss creeping over the eave rim and hanging below it, thinning at both ends.
	pts = []
	sc = []
	n = 7
	for k in range(n):
		t = k / (n - 1)
		if side in ("front", "back"):
			x = x0 + (x1 - x0) * t
			y = -RY if side == "front" else RY
		else:
			y = x0 + (x1 - x0) * t
			x = RX if side == "right" else -RX
		pts.append((x, y, roof_height(x, y) + 0.02))
		sc.append(0.35 + 0.65 * math.sin(math.pi * t))
	if side in ("back", "left"):
		pts.reverse()
	# Side points outward on every eave: the cushion starts buried in the tiles and hangs just past the rim.
	prof = [(-0.4, -0.04), (-0.3, 0.1), (-0.14, 0.1), (0.02, 0.07), (0.05, -0.05), (0.03, -0.22)]
	strip = sweep("moss_eave", pts, prof, m, scales=sc)
	return geo.jitter(strip, 0.05, 2.5, seed=seed)


def drop_faces(obj, fn):
	# Faces nobody sees (under the roof, on the ground) would only waste texture space.
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


def slope_rot(x, y):
	q = roof_normal(x, y).to_track_quat('Z', 'Y').to_euler()
	return (math.degrees(q.x), math.degrees(q.y), math.degrees(q.z))


def roof(giwa):
	verts = []
	idx = {}
	for j in range(NY + 1):
		y = -RY + 2.0 * RY * j / NY
		for i in range(NX + 1):
			x = -RX + 2.0 * RX * i / NX
			idx[(i, j)] = len(verts)
			verts.append((x, y, roof_height(x, y)))
	faces = []
	for j in range(NY):
		for i in range(NX):
			a, b, c, d = idx[(i, j)], idx[(i + 1, j)], idx[(i + 1, j + 1)], idx[(i, j + 1)]
			cx = (verts[a][0] + verts[c][0]) * 0.5
			cy = (verts[a][1] + verts[c][1]) * 0.5
			if cx * cy > 0.0:
				faces += [(a, b, c), (a, c, d)]
			else:
				faces += [(a, b, d), (b, c, d)]
	# The tile rim: the boundary dropped into a short fascia, walked anticlockwise from above.
	loop = [idx[(i, 0)] for i in range(NX)] + [idx[(NX, j)] for j in range(NY)] + \
		   [idx[(i, NY)] for i in range(NX, 0, -1)] + [idx[(0, j)] for j in range(NY, 0, -1)]
	low = []
	for v in loop:
		x, y, z = verts[v]
		low.append(len(verts))
		verts.append((x, y, z - 0.14))
	for k in range(len(loop)):
		k2 = (k + 1) % len(loop)
		faces.append((loop[k], low[k], low[k2], loop[k2]))

	def crease(a, b):
		def on(p):
			on_hip = abs((RY - abs(p.y)) - (RX - abs(p.x)) * KX) < 1e-3 and abs(p.x) >= HIP_X - 1e-3
			on_ridge = abs(p.y) < 1e-3 and abs(p.x) <= HIP_X + 1e-3
			return on_hip or on_ridge
		return on(a) and on(b) and a.z > EAVE_Z - 0.05 and b.z > EAVE_Z - 0.05
	return mesh_obj("roof", verts, faces, giwa, True, crease)


def tile_rows(cover):
	# Round cover tiles running down every slope, ending in a cap at the eave.
	parts = []
	prof = [(-0.09, -0.02), (0.0, 0.08), (0.09, -0.02)]
	rnd = geo.rng(11)
	dy = 2.0 * RY / NY
	dx = 2.0 * RX / NX
	x = -RX + 0.16
	while x < RX - 0.1:
		xx = x + rnd.uniform(-0.02, 0.02)
		for sy in (-1.0, 1.0):
			reach = min(RY, (RX - abs(xx)) * KX)
			pts = []
			yy = RY
			while yy > RY - reach + 1e-4:
				pts.append((xx, sy * yy, roof_height(xx, sy * yy) + 0.012))
				yy -= dy
			pts.append((xx, sy * (RY - reach), roof_height(xx, sy * (RY - reach)) + 0.012))
			if len(pts) >= 2 and reach > 0.12:
				parts.append(sweep("tiles", pts, prof, cover, cap_start=True))
		x += 0.32
	y = -RY + 0.2
	while y < RY - 0.1:
		for sx in (-1.0, 1.0):
			reach = (RY - abs(y)) / KX
			if reach < 0.12:
				continue
			pts = []
			xx = RX
			while xx > RX - reach + 1e-4:
				pts.append((sx * xx, y, roof_height(sx * xx, y) + 0.012))
				xx -= dx
			pts.append((sx * (RX - reach), y, roof_height(sx * (RX - reach), y) + 0.012))
			if len(pts) >= 2:
				parts.append(sweep("tiles_end", pts, prof, cover, cap_start=True))
		y += 0.32
	return parts


def ridges(ridge_m, mortar):
	parts = []
	box = [(-0.15, -0.12), (-0.15, 0.1), (-0.12, 0.16), (0.12, 0.16), (0.15, 0.1), (0.15, -0.12)]
	r = RIDGE_Z
	pts = [(-HIP_X - 0.28, 0, r + 0.1), (-HIP_X - 0.05, 0, r + 0.04), (0.0, 0, r + 0.01),
		   (HIP_X + 0.05, 0, r + 0.04), (HIP_X + 0.28, 0, r + 0.1)]
	parts.append(sweep("ridge", pts, box, ridge_m, closed=True, cap_start=True, cap_end=True))
	parts.append(geo.box((2 * HIP_X + 0.3, 0.34, 0.06), (0, 0, r - 0.06), mat=mortar, name="ridge_mortar"))
	hip = [(-0.08, -0.06), (-0.08, 0.06), (0.0, 0.1), (0.08, 0.06), (0.08, -0.06)]
	for sx in (-1.0, 1.0):
		for sy in (-1.0, 1.0):
			pts = []
			for k in range(NY // 2 + 1):
				x = sx * (HIP_X + k * (RX - HIP_X) / (NY // 2))
				y = sy * (k * RY / (NY // 2))
				pts.append((x, y, roof_height(x, y) + 0.05))
			end = Vector(pts[-1])
			flick = end + Vector((sx * 0.1, sy * 0.1, 0.07))
			pts.append(tuple(flick))
			parts.append(sweep("hip", pts, hip, ridge_m, closed=True, cap_start=True, cap_end=True))
	return parts


class Wall:
	# A wall face: u runs along the wall seen from outside, n points out of it.
	def __init__(self, origin, right, out, rot_z):
		self.o = Vector(origin)
		self.r = Vector(right)
		self.n = Vector(out)
		self.rz = rot_z

	def at(self, u, n, z):
		p = self.o + self.r * u + self.n * n
		return (p.x, p.y, z)

	def box(self, size, u, n, z, m, name="part", bevel=0.0):
		return geo.box(size, self.at(u, n, z), (0, 0, self.rz), mat=m, name=name, bevel=bevel)

	def quad(self, u0, u1, z0, z1, n, m, name="bar"):
		vs = [self.at(u0, n, z0), self.at(u1, n, z0), self.at(u1, n, z1), self.at(u0, n, z1)]
		return mesh_obj(name, vs, [(0, 1, 2, 3)], m, False)


def paper_opening(wall, u0, u1, z0, z1, frame, paper, dark, cols, rows, gap=None):
	# A paper-grid sliding window or door: dark frame, paper panels, thin lattice bars.
	parts = []
	w = u1 - u0
	h = z1 - z0
	cu = (u0 + u1) * 0.5
	f = 0.06
	parts.append(wall.box((w + 2 * f, 0.07, f), cu, 0.02, z1 + f * 0.5, frame, "frame_top"))
	parts.append(wall.box((w + 2 * f, 0.08, f), cu, 0.03, z0 - f * 0.5, frame, "frame_sill"))
	parts.append(wall.box((f, 0.07, h), u0 - f * 0.5, 0.02, (z0 + z1) * 0.5, frame, "frame_l"))
	parts.append(wall.box((f, 0.07, h), u1 + f * 0.5, 0.02, (z0 + z1) * 0.5, frame, "frame_r"))
	leaves = [(u0, cu - 0.015), (cu + 0.015, u1)]
	if gap is not None:
		leaves = [(u0, gap[0]), (cu + 0.015, u1)]
		parts.append(wall.quad(gap[0], gap[1], z0, z1, 0.008, dark, "gap"))
	for (a, b) in leaves:
		parts.append(wall.quad(a, b, z0, z1, 0.012, paper, "paper"))
		parts.append(wall.quad(a, a + 0.04, z0, z1, 0.032, frame, "stile"))
		parts.append(wall.quad(b - 0.04, b, z0, z1, 0.032, frame, "stile"))
		lw = b - a
		for c in range(1, cols):
			u = a + lw * c / cols
			parts.append(wall.quad(u - 0.011, u + 0.011, z0, z1, 0.03, frame, "bar_v"))
		for r_ in range(1, rows):
			z = z0 + h * r_ / rows
			parts.append(wall.quad(a, b, z - 0.011, z + 0.011, 0.031, frame, "bar_h"))
	return parts


def build():
	giwa = mat.get("giwa")
	cover = mat.make("giwa_cover", (0.30, 0.31, 0.33), pattern="giwa", rough=0.72, bump=0.6)
	ridge_m = mat.make("giwa_ridge", (0.18, 0.19, 0.21), pattern="giwa", rough=0.75, bump=0.5)
	mortar = mat.make("ridge_mortar", (0.58, 0.56, 0.51), pattern="plaster", rough=0.95, bump=0.3)
	plaster = mat.make("house_a_plaster", (0.52, 0.48, 0.41), pattern="plaster", rough=0.93, bump=0.35)
	post = mat.get("wood_dark")
	frame = mat.make("lattice_wood", (0.21, 0.17, 0.13), pattern="wood", rough=0.85, bump=0.3)
	paper = mat.make("changhoji", (0.76, 0.72, 0.61), pattern="paper", rough=0.95, bump=0.15, edge=0.05)
	dark = mat.make("interior_dark", (0.05, 0.045, 0.04), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	stone = mat.get("stone")
	maru = mat.make("maru_boards", (0.38, 0.31, 0.23), pattern="planks", axis='X', rough=0.85, bump=0.45)
	tin = mat.get("tin")
	rust = mat.get("rust")
	logs = mat.make("firewood", (0.42, 0.34, 0.25), pattern="wood", rough=0.9, bump=0.5)
	bark = mat.get("bark")
	shoe = mat.make("rubber_shoe", (0.80, 0.79, 0.74), pattern="plain", rough=0.5, bump=0.05, grime=0.4)
	soot = mat.make("soot", (0.10, 0.09, 0.08), pattern="plaster", rough=0.95, bump=0.1)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	glow = mat.get("spore_glow")

	parts = []

	# Stone plinth (gidan): wide in front where the shoes are left.
	plinth = geo.box((7.3, 6.75, PLINTH), (0, -0.225, PLINTH * 0.5), mat=stone, bevel=0.05, name="plinth")
	plinth = geo.jitter(plinth, 0.025, 1.5, seed=4)
	parts.append(drop_faces(plinth, lambda n: n.z < -0.9))

	core_wall = geo.box((2 * WX - 0.08, 2 * WY - 0.08, WALL_TOP - PLINTH), (0, 0, (WALL_TOP + PLINTH) * 0.5), mat=plaster, name="wall")
	parts.append(drop_faces(core_wall, lambda n: abs(n.z) > 0.9))

	# Posts and beams frame every bay.
	for (x, y) in ((-WX, -WY), (WX, -WY), (-WX, WY), (WX, WY), (-1.2, -WY), (1.2, -WY), (-1.2, WY), (1.2, WY), (-WX, 0.0), (WX, 0.0)):
		px = max(-WX + 0.09, min(WX - 0.09, x))
		py = max(-WY + 0.09, min(WY - 0.09, y))
		parts.append(drop_faces(geo.box((0.18, 0.18, WALL_TOP - PLINTH - 0.05), (px, py, (WALL_TOP + PLINTH) * 0.5 - 0.02), mat=post, name="post"),
								lambda n: abs(n.z) > 0.9))
	parts.append(drop_faces(geo.box((2 * WX, 2 * WY, 0.2), (0, 0, 2.2), mat=post, name="beam"), lambda n: abs(n.z) > 0.9))
	parts.append(drop_faces(geo.box((2 * WX - 0.02, 2 * WY - 0.02, 0.1), (0, 0, PLINTH + 0.05), mat=post, name="sill"), lambda n: abs(n.z) > 0.9))

	front = Wall((0, -WY + 0.04, 0), (1, 0, 0), (0, -1, 0), 0)
	back = Wall((0, WY - 0.04, 0), (-1, 0, 0), (0, 1, 0), 180)
	left = Wall((-WX + 0.04, 0, 0), (0, -1, 0), (-1, 0, 0), 90)
	right = Wall((WX - 0.04, 0, 0), (0, 1, 0), (1, 0, 0), 90)

	# Front: sliding door in the middle bay, one leaf pushed aside onto the dark room.
	parts += paper_opening(front, -1.0, 1.0, 0.7, 2.0, frame, paper, dark, 4, 5, gap=(-0.38, -0.03))
	parts += paper_opening(front, -2.75, -1.75, 1.05, 1.8, frame, paper, dark, 3, 3)
	parts += paper_opening(front, 1.75, 2.75, 1.05, 1.8, frame, paper, dark, 3, 3)
	parts += paper_opening(back, -0.5, 0.5, 1.1, 1.75, frame, paper, dark, 3, 3)
	parts += paper_opening(left, -1.0, 0.0, 1.15, 1.75, frame, paper, dark, 3, 2)
	parts += paper_opening(right, -1.8, -1.0, 1.25, 1.8, frame, paper, dark, 2, 2)

	# Toenmaru: the narrow wooden porch along the front, on short legs.
	deck_y = -WY - 0.21
	parts.append(geo.box((6.9, 0.42, 0.07), (0, deck_y, 0.6), mat=maru, name="maru"))
	parts.append(geo.box((6.9, 0.08, 0.1), (0, -WY - 0.38, 0.52), mat=post, name="maru_edge"))
	for x in (-3.35, -1.2, 1.2, 3.35):
		parts.append(geo.box((0.1, 0.1, 0.26), (x, -WY - 0.36, PLINTH + 0.12), mat=post, name="maru_leg"))

	# Grandmother's white rubber shoes, toes out.
	for k, x in enumerate((-0.12, 0.08)):
		s = geo.blob((0.06, 0.13, 0.045), (x, -WY - 0.46, PLINTH + 0.035), (0, 0, 6 - 12 * k), mat=shoe, subdivisions=1, name="shoe")
		parts.append(s)

	# Stovepipe out of the right wall, up through the eave, with a rain hat.
	pipe_y = 1.6
	parts.append(geo.tube([(WX - 0.02, pipe_y, 0.95), (WX + 0.3, pipe_y, 0.95), (WX + 0.36, pipe_y, 1.05), (WX + 0.36, pipe_y, 3.05)],
						  0.075, mat=tin, segments=10, name="stovepipe"))
	parts.append(geo.cone(0.17, 0.14, (WX + 0.36, pipe_y, 3.17), mat=rust, segments=10, name="pipe_hat"))
	parts.append(geo.cylinder(0.02, 0.1, (WX + 0.36, pipe_y, 3.07), mat=rust, segments=6, name="pipe_neck"))
	parts.append(geo.cylinder(0.24, 0.01, (WX + 0.005, pipe_y, 0.95), (0, 90, 0), mat=soot, segments=12, name="soot"))

	# Firewood stacked against the right wall under the eave.
	parts.append(geo.box((0.46, 1.7, 0.06), (WX + 0.4, -1.7, 0.03), mat=mat.get("wood_grey"), name="wood_sleeper"))
	rnd = geo.rng(5)
	for row in range(5):
		count = 8 - (row // 2)
		for k in range(count):
			y = -2.5 + 0.18 + (1.5 / count) * k + rnd.uniform(-0.02, 0.02) + (0.09 if row % 2 else 0.0)
			z = 0.145 + row * 0.155
			r = rnd.uniform(0.07, 0.085)
			parts.append(geo.cylinder(r, rnd.uniform(0.40, 0.46), (WX + 0.4 + rnd.uniform(-0.03, 0.03), y, z),
									  (rnd.uniform(-4, 4), 90, rnd.uniform(-5, 5)), mat=logs if rnd.random() < 0.7 else bark,
									  segments=5, smooth=False, name="log"))

	parts.append(roof(giwa))
	parts += tile_rows(cover)
	parts += ridges(ridge_m, mortar)

	# Moss settling on the roof, the ridge and round the foot of the walls.
	roof_moss = [((-2.2, -2.55), (0.95, 0.65, 0.13), moss), ((1.7, -1.8), (0.5, 0.38, 0.1), moss_dark),
				 ((3.1, -2.75), (0.42, 0.34, 0.1), moss), ((-1.6, 2.3), (0.85, 0.6, 0.12), moss),
				 ((3.5, 0.5), (0.4, 0.55, 0.1), moss_dark), ((-3.55, -0.9), (0.38, 0.45, 0.1), moss_dark)]
	for k, ((x, y), radii, m) in enumerate(roof_moss):
		pad = moss_pad(radii, x, y, roof_height(x, y) - 0.02, slope_rot(x, y), m, 10 + k)
		if k in (0, 3):
			geo.tag(pad, "pulse")
		parts.append(pad)
	parts.append(eave_moss(-3.3, -1.4, "front", moss, 51))
	parts.append(eave_moss(1.9, 3.6, "front", moss_dark, 52))
	parts.append(eave_moss(-2.6, -0.4, "right", moss, 53))
	parts.append(eave_moss(-0.6, 2.2, "back", moss, 54))
	parts.append(eave_moss(-1.8, 0.6, "left", moss_dark, 55))
	ridge_moss = moss_pad((0.75, 0.2, 0.12), 0.7, 0.0, RIDGE_Z + 0.12, (0, 0, 0), moss, 21)
	geo.tag(ridge_moss, "pulse")
	parts.append(ridge_moss)
	base_moss = [((-3.7, -1.5), (0.22, 0.9, 0.16)), ((-2.2, 3.25), (0.8, 0.22, 0.14)), ((3.72, 0.7), (0.2, 0.7, 0.15)),
				 ((2.9, -3.55), (0.5, 0.16, 0.13)), ((-3.3, -3.55), (0.4, 0.16, 0.14))]
	for k, ((x, y), radii) in enumerate(base_moss):
		parts.append(moss_pad(radii, x, y, -0.02, (0, 0, 0), moss_dark if k % 2 else moss, 30 + k, segments=8))
	parts.append(moss_pad((0.25, 0.5, 0.08), WX + 0.42, -1.5, 0.86, (0, 0, 0), moss, 41))

	# Weeds at the corners of the plinth.
	for k, (cx, cy) in enumerate(((-3.75, -3.5), (3.7, -3.0), (3.75, 2.9), (-3.7, 2.8), (-1.6, -3.55), (2.3, -3.55))):
		for j in range(3):
			h = rnd.uniform(0.22, 0.42)
			wd = geo.cone(rnd.uniform(0.05, 0.08), h, (cx + rnd.uniform(-0.15, 0.15), cy + rnd.uniform(-0.08, 0.08), h * 0.5),
						  (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=4, name="weed")
			geo.tag(wd, "sway", "foliage")
			parts.append(wd)

	spore = geo.blob(0.06, (-2.0, -2.25, roof_height(-2.0, -2.25) + 0.2), mat=glow, subdivisions=1, name="spore")
	geo.tag(spore, "pulse")
	parts.append(spore)

	for p in parts:
		if p.name.startswith(("roof", "tiles", "ridge", "hip", "wall", "plinth", "maru", "beam", "post")):
			geo.moss_mask(p, seed=5)

	res = pipeline.static("house_a", parts, size=512)
	res["unit"] = (7.0, 3.6, 6.0)
	return res
