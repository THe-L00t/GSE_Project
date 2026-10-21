# A neighbour's 1970s slate house: corrugated slate roof sagging between rafters, stained cement walls,
# blue-painted door and window frames, an outdoor tap with a rubber basin, styrofoam-box greens and pots.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

WX, WY = 2.75, 2.5
WALL_TOP = 2.36
RX, RY = WX + 0.3, WY + 0.4
RIDGE_Z = 3.68
SLOPE = (RIDGE_Z - 2.5) / WY
PERIOD = 0.2
AMP = 0.026


def plane(ay):
	return RIDGE_Z - SLOPE * ay


def sag(x):
	return 0.05 * math.cos(math.pi * x / (2.0 * RX))


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


def sweep(name, points, profile, m, closed=False, cap_start=False, cap_end=False, smooth=True, scales=None):
	# Sweeps a 2D (side, up) profile along a path; profiles run clockwise for outward faces.
	pts = [Vector(p) for p in points]
	scales = scales or [1.0] * len(pts)
	n = len(profile)
	verts = []
	for i, p in enumerate(pts):
		t = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
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


def slate_course(a0, a1, sgn, m, name="slate"):
	# One row of corrugated sheets from |y| = a0 (eave side) up to a1; the lower edge lifts where it laps.
	cols = int(round(2.0 * RX / (PERIOD / 4.0)))
	verts = []
	for j, ay in enumerate((a0, a1)):
		lift = 0.035 if j == 0 else 0.0
		for i in range(cols + 1):
			x = -RX + 2.0 * RX * i / cols
			z = plane(ay) + AMP * math.sin(2.0 * math.pi * x / PERIOD) + lift - sag(x)
			verts.append((x, sgn * ay, z))
	faces = []
	for i in range(cols):
		a, b, c, d = i, i + 1, cols + 2 + i, cols + 1 + i
		faces.append((a, b, c, d) if sgn < 0 else (d, c, b, a))
	return mesh_obj(name, verts, faces, m, True)


def roof_z(x, y):
	return plane(abs(y)) - sag(x) + AMP


def slope_rot(y):
	ang = math.degrees(math.atan(SLOPE))
	return (ang if y < 0 else -ang, 0, 0)


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

	def box(self, size, u, n, z, m, name="part"):
		return geo.box(size, self.at(u, n, z), (0, 0, self.rz), mat=m, name=name)

	def quad(self, u0, u1, z0, z1, n, m, name="bar", u0_top=None, u1_top=None):
		u0t = u0 if u0_top is None else u0_top
		u1t = u1 if u1_top is None else u1_top
		vs = [self.at(u0, n, z0), self.at(u1, n, z0), self.at(u1t, n, z1), self.at(u0t, n, z1)]
		return mesh_obj(name, vs, [(0, 1, 2, 3)], m, False)


def glazed(wall, u0, u1, z0, z1, frame, glass, cols, rows, board=None, board_m=None):
	# A blue-framed sliding window or door: two sashes of small panes, a sill, and a stain running down.
	parts = []
	w = u1 - u0
	h = z1 - z0
	cu = (u0 + u1) * 0.5
	f = 0.07
	parts.append(wall.box((w + 2 * f, 0.08, f), cu, 0.02, z1 + f * 0.5, frame, "frame"))
	parts.append(wall.box((f, 0.08, h), u0 - f * 0.5, 0.02, (z0 + z1) * 0.5, frame, "frame"))
	parts.append(wall.box((f, 0.08, h), u1 + f * 0.5, 0.02, (z0 + z1) * 0.5, frame, "frame"))
	parts.append(wall.quad(u0, u1, z0, z1, 0.006, glass, "glass"))
	for (a, b, n) in ((u0, cu + 0.02, 0.03), (cu - 0.02, u1, 0.045)):
		lw = b - a
		parts.append(wall.quad(a, a + 0.05, z0, z1, n, frame, "sash"))
		parts.append(wall.quad(b - 0.05, b, z0, z1, n, frame, "sash"))
		parts.append(wall.quad(a, b, z1 - 0.05, z1, n, frame, "sash"))
		parts.append(wall.quad(a, b, z0, z0 + 0.06, n, frame, "sash"))
		for c in range(1, cols):
			u = a + lw * c / cols
			parts.append(wall.quad(u - 0.018, u + 0.018, z0, z1, n + 0.002, frame, "muntin"))
		for r_ in range(1, rows):
			z = z0 + h * r_ / rows
			parts.append(wall.quad(a, b, z - 0.018, z + 0.018, n + 0.003, frame, "muntin"))
	if board is not None:
		bu0, bu1, bz0, bz1 = board
		parts.append(wall.quad(bu0, bu1, bz0, bz1, 0.06, board_m, "board"))
	return parts


def moss_pad(radii, x, y, z, rot, m, seed, segments=11):
	# A soft cushion: domed, edges rolled under, lumped by low-frequency noise.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, z), rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.22, 1.6, seed=seed)


def eave_moss(x0, x1, sgn, m, seed):
	# Moss creeping over the eave edge and hanging below it, thinning at both ends.
	pts = []
	sc = []
	n = 7
	for k in range(n):
		t = k / (n - 1)
		x = x0 + (x1 - x0) * t
		pts.append((x, sgn * RY, roof_z(x, RY) + 0.03))
		sc.append(0.35 + 0.65 * math.sin(math.pi * t))
	if sgn > 0:
		pts.reverse()
	# Side points outward on both eaves: the cushion starts buried in the slate and hangs just past the edge.
	prof = [(-0.4, 0.1), (-0.3, 0.17), (-0.14, 0.12), (0.02, 0.07), (0.05, -0.05), (0.03, -0.22)]
	strip = sweep("moss_eave", pts, prof, m, scales=sc)
	return geo.jitter(strip, 0.05, 2.5, seed=seed)


def build():
	slate = mat.get("slate")
	cap_m = mat.make("slate_cap", (0.27, 0.28, 0.28), pattern="concrete", rough=0.85, bump=0.4)
	cement = mat.make("cement_wall", (0.50, 0.50, 0.47), pattern="concrete", rough=0.93, bump=0.35)
	base_m = mat.make("cement_base", (0.36, 0.36, 0.34), pattern="concrete", rough=0.95, bump=0.45)
	stain = mat.make("wall_stain", (0.38, 0.38, 0.35), pattern="plaster", rough=0.95, bump=0.2)
	blue = mat.get("paint_blue")
	glass = mat.get("glass")
	board_m = mat.get("wood_grey")
	vent_m = mat.make("vent_dark", (0.08, 0.08, 0.08), pattern="plain", rough=0.9, grime=0.0, edge=0.0, bump=0.0)
	metal = mat.get("metal")
	rust = mat.get("rust")
	basin_m = mat.make("rubber_basin", (0.50, 0.22, 0.18), pattern="plain", rough=0.6, bump=0.1)
	water = mat.make("tap_water", (0.07, 0.10, 0.11), pattern="plain", rough=0.08, grime=0.0, edge=0.0, bump=0.0)
	styro = mat.make("styrofoam", (0.76, 0.75, 0.70), pattern="concrete", rough=0.95, bump=0.3)
	onggi = mat.get("onggi")
	soil = mat.get("soil")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")

	parts = []

	# Body: cement walls on a darker skirting, gables filling the roof ends.
	flat = lambda n: abs(n.z) > 0.9
	parts.append(drop_faces(geo.box((2 * WX, 2 * WY, WALL_TOP), (0, 0, WALL_TOP * 0.5), mat=cement, name="wall"), flat))
	parts.append(drop_faces(geo.box((2 * WX + 0.06, 2 * WY + 0.06, 0.35), (0, 0, 0.175), mat=base_m, name="wall_base"), flat))
	gable = geo.prism((2 * WX, 2 * WY, 1.24), (0, 0, WALL_TOP), mat=cement, name="wall_gable")
	parts.append(drop_faces(gable, lambda n: abs(n.x) < 0.5))
	for sx in (-1.0, 1.0):
		parts.append(geo.box((0.04, 0.3, 0.3), (sx * (WX + 0.01), 0, 3.05), mat=vent_m, name="vent"))
		for k in range(3):
			parts.append(geo.box((0.05, 0.32, 0.03), (sx * (WX + 0.03), 0, 2.95 + k * 0.1), mat=blue, name="vent_slat"))

	front = Wall((0, -WY, 0), (1, 0, 0), (0, -1, 0), 0)
	back = Wall((0, WY, 0), (-1, 0, 0), (0, 1, 0), 180)
	left = Wall((-WX, 0, 0), (0, -1, 0), (-1, 0, 0), 90)
	right = Wall((WX, 0, 0), (0, 1, 0), (1, 0, 0), 90)

	# Front: a glazed sliding door, one pane long since boarded over, windows either side.
	parts += glazed(front, -0.78, 0.78, 0.36, 2.12, blue, glass, 2, 4, board=(0.05, 0.7, 0.4, 0.82), board_m=board_m)
	parts += glazed(front, -2.25, -1.3, 1.05, 1.95, blue, glass, 2, 2)
	parts += glazed(front, 1.3, 2.25, 1.05, 1.95, blue, glass, 2, 2)
	parts += glazed(back, -1.4, -0.4, 1.1, 1.9, blue, glass, 2, 2)
	parts += glazed(left, -0.6, 0.6, 1.1, 1.9, blue, glass, 2, 2)
	parts += glazed(right, 0.6, 1.6, 1.15, 1.9, blue, glass, 2, 2)

	# Rain stains: thin drips running down from each sill and from the eaves.
	srnd = geo.rng(17)
	for (w, u0, u1, z) in ((front, -2.2, -1.35, 0.98), (front, 1.35, 2.2, 0.98), (back, -1.35, -0.45, 1.03),
						   (left, -0.55, 0.55, 1.03), (right, 0.65, 1.55, 1.08), (front, -2.65, -2.1, WALL_TOP),
						   (front, 0.9, 1.25, WALL_TOP), (right, -1.9, -1.1, WALL_TOP), (back, 0.6, 1.6, WALL_TOP)):
		for k in range(3):
			u = srnd.uniform(u0, u1)
			wd = srnd.uniform(0.04, 0.09)
			ln = srnd.uniform(0.3, 0.75)
			parts.append(w.quad(u - wd * 0.15, u + wd * 0.15, max(0.36, z - ln), z, 0.004, stain, "stain",
								u0_top=u - wd * 0.5, u1_top=u + wd * 0.5))

	# Concrete doorstep.
	parts.append(drop_faces(geo.box((1.9, 0.38, 0.2), (0, -WY - 0.19, 0.1), mat=base_m, name="step"), lambda n: n.z < -0.9))

	# Corrugated slate in three lapped courses a side, a bent ridge cap, grey barge boards.
	edges = [0.0, 0.95, 1.95, RY]
	for sgn in (-1.0, 1.0):
		for k in range(3):
			a0 = edges[3 - k]
			a1 = max(0.0, edges[2 - k] - (0.1 if k < 2 else 0.0))
			parts.append(slate_course(a0, a1, sgn, slate, "slate"))
	cap = [(-0.21, -0.07), (0.0, 0.07), (0.21, -0.07)]
	cap_pts = [(-RX - 0.02 + 2 * (RX + 0.02) * k / 8, 0, RIDGE_Z - sag(-RX + 2 * RX * k / 8) + 0.03) for k in range(9)]
	parts.append(sweep("ridge_cap", cap_pts, cap, cap_m, cap_start=True, cap_end=True))
	ang = math.degrees(math.atan(SLOPE))
	slope_len = RY / math.cos(math.atan(SLOPE))
	for sx in (-1.0, 1.0):
		for sy in (-1.0, 1.0):
			parts.append(geo.box((0.04, slope_len, 0.16), (sx * (RX + 0.02), sy * RY * 0.5, plane(RY * 0.5) - 0.06),
								 (sy * -ang, 0, 0), mat=board_m, name="barge"))

	# TV antenna on the ridge, leaning after years of wind.
	ax, ay = 1.9, 0.0
	az = RIDGE_Z - sag(ax) + 0.05
	top = Vector((ax + 0.12, ay - 0.06, az + 0.8))
	parts.append(geo.tube([(ax, ay, az), tuple(top)], 0.022, mat=metal, segments=6, name="antenna"))
	for k, h in enumerate((0.45, 0.62, 0.78)):
		p = Vector((ax, ay, az)).lerp(top, h / 0.8)
		wdt = 0.55 - k * 0.12
		parts.append(geo.tube([(p.x - 0.03, p.y - wdt * 0.5, p.z), (p.x + 0.03, p.y + wdt * 0.5, p.z - 0.02)], 0.012, mat=rust, segments=4, name="antenna_bar"))
	parts.append(geo.cylinder(0.08, 0.06, (ax, ay, az), mat=rust, segments=8, name="antenna_foot"))

	# Outdoor tap: a concrete sink against the right wall, a standpipe and tap, a red rubber basin.
	parts.append(drop_faces(geo.box((0.44, 0.95, 0.34), (WX + 0.22, -1.25, 0.17), mat=base_m, name="sink"), lambda n: n.z < -0.9))
	parts.append(geo.box((0.32, 0.8, 0.01), (WX + 0.23, -1.25, 0.335), mat=water, name="sink_water"))
	parts.append(geo.tube([(WX + 0.08, -0.68, 0.0), (WX + 0.08, -0.68, 0.92), (WX + 0.22, -0.68, 0.92), (WX + 0.24, -0.72, 0.84)],
						  0.025, mat=metal, segments=6, name="standpipe"))
	parts.append(geo.cylinder(0.035, 0.03, (WX + 0.16, -0.68, 0.97), mat=rust, segments=6, name="tap_handle"))
	basin = geo.lathe([(0.17, 0.0), (0.19, 0.02), (0.25, 0.15), (0.27, 0.16), (0.235, 0.16), (0.17, 0.03), (0.0, 0.03)],
					  (WX + 0.27, -2.15, 0.0), (0, -8, 0), mat=basin_m, segments=14, name="basin")
	parts.append(basin)

	# Greens: a styrofoam box of spring onions and two clay pots by the door.
	parts.append(geo.box((0.58, 0.36, 0.26), (-1.55, -WY - 0.24, 0.13), (0, 0, 4), mat=styro, name="styro"))
	parts.append(geo.box((0.52, 0.3, 0.02), (-1.55, -WY - 0.24, 0.25), (0, 0, 4), mat=soil, name="styro_soil"))
	rnd = geo.rng(9)
	for k in range(9):
		x = -1.78 + (k % 5) * 0.11 + rnd.uniform(-0.02, 0.02)
		y = -WY - 0.31 + (k // 5) * 0.13
		h = rnd.uniform(0.28, 0.42)
		blade = geo.cone(0.025, h, (x, y, 0.26 + h * 0.5), (rnd.uniform(-12, 12), rnd.uniform(-12, 12), 0), mat=leaf, segments=4, name="onion")
		geo.tag(blade, "sway", "foliage")
		parts.append(blade)
	for (x, r, h, seed) in ((1.2, 0.17, 0.3, 3), (1.6, 0.12, 0.22, 4)):
		pot = geo.lathe([(r * 0.7, 0.0), (r, h * 0.7), (r * 0.9, h), (r * 0.75, h)], (x, -WY - 0.25, 0.0), mat=onggi, segments=12,
						name="pot", close_top=False)
		parts.append(pot)
		bush = geo.blob((r * 1.5, r * 1.4, r * 1.3), (x, -WY - 0.25, h + r * 0.8), mat=leaf_dark if seed % 2 else leaf,
						subdivisions=2, lumpy=0.3, seed=seed, name="pot_plant")
		geo.tag(bush, "sway", "foliage")
		parts.append(bush)

	# Moss: thick on the slate, rolling over the eaves, at the wet sink and the foot of the walls.
	for k, (x, y, radii, m) in enumerate(((-1.6, -2.1, (0.9, 0.55, 0.12), moss), (0.9, -1.2, (0.5, 0.36, 0.1), moss_dark),
										  (2.4, -2.4, (0.45, 0.35, 0.1), moss), (-0.8, 1.9, (0.85, 0.5, 0.12), moss),
										  (2.2, 1.2, (0.45, 0.4, 0.1), moss_dark))):
		pad = moss_pad(radii, x, y, roof_z(x, y) - 0.03, slope_rot(y), m, 20 + k)
		if k in (0, 3):
			geo.tag(pad, "pulse")
		parts.append(pad)
	parts.append(eave_moss(-2.8, -0.7, -1.0, moss, 31))
	parts.append(eave_moss(-1.6, 0.9, 1.0, moss_dark, 32))
	parts.append(moss_pad((0.25, 0.5, 0.1), WX + 0.2, -1.25, 0.3, (0, 0, 0), moss, 33, segments=8))
	for k, (x, y, radii) in enumerate(((WX + 0.1, 0.6, (0.18, 0.7, 0.15)), (-WX - 0.08, -1.2, (0.2, 0.8, 0.16)),
									   (-1.2, WY + 0.08, (0.7, 0.2, 0.14)), (2.3, -WY - 0.06, (0.4, 0.15, 0.12)))):
		parts.append(moss_pad(radii, x, y, -0.02, (0, 0, 0), moss_dark if k % 2 else moss, 40 + k, segments=8))
	for k, (cx, cy) in enumerate(((-WX - 0.1, -WY - 0.1), (WX + 0.12, WY + 0.05), (-WX - 0.05, WY + 0.1), (WX + 0.5, -1.8))):
		for j in range(3):
			h = rnd.uniform(0.2, 0.4)
			wd = geo.cone(rnd.uniform(0.05, 0.08), h, (cx + rnd.uniform(-0.12, 0.12), cy + rnd.uniform(-0.12, 0.12), h * 0.5),
						  (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=4, name="weed")
			geo.tag(wd, "sway", "foliage")
			parts.append(wd)

	for p in parts:
		if p.name.startswith(("wall", "slate", "ridge_cap", "barge", "step", "sink")):
			geo.moss_mask(p, seed=7)

	res = pipeline.static("house_b", parts, size=512)
	res["unit"] = (5.5, 3.2, 5.0)
	return res
