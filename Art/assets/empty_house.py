# An empty house far out along the road: a small cement-block house under corrugated slate, the front slope
# caved in on the left so the rafters show and a young tree has come up through the hole. An aluminium sliding
# door left ajar, broken and boarded windows, a cement step; a rusty TV antenna leaning off the ridge, an LPG
# cylinder by the wall, a broken chair; at the back a barred bathroom window, a bent stovepipe and spent
# briquettes. Stained walls, cracks, tall weeds, vines up the left end, moss thick on the slate.
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from gse import core, geo, mat, pipeline

WX, WY = 3.35, 2.55
RX, RY = 3.55, 2.85
RIDGE = 3.55
SLOPE = 0.443
PLINTH = 0.3
WALL_TOP = RIDGE - SLOPE * WY - 0.05
PERIOD, AMP = 0.3, 0.028
CX, CA, CR = -1.75, 1.2, 1.35


def plane(a):
	return RIDGE - SLOPE * a


def sag(x, y):
	# The cave-in: the front slope sinks round the hole between ridge and wall, the ridge dips a little above it.
	a = abs(y)
	bx = max(0.0, 1.0 - ((x - CX) / 2.0) ** 2)
	ridge = max(0.0, 1.0 - (CA / CR) ** 2)
	if y < 0.0:
		by = max(0.0, 1.0 - ((a - CA) / CR) ** 2)
	else:
		by = ridge * max(0.0, 1.0 - a / 1.0)
	return 0.6 * (bx * by) ** 1.3


def roof_z(x, y):
	return plane(abs(y)) - sag(x, y)


def mesh_obj(name, verts, faces, m, smooth=False):
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


def sweep(name, points, profile, m, cap_start=False, cap_end=False):
	# Sweeps a 2D (side, up) profile along a path.
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
	return mesh_obj(name, verts, faces, m, True)


def drop_faces(obj, fn):
	obj = geo.apply_modifiers(obj)
	bpy.context.view_layer.update()
	r = obj.matrix_world.to_3x3()
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bmesh.ops.delete(bm, geom=[f for f in bm.faces if fn((r @ f.normal).normalized())], context='FACES')
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def place(objs, m):
	for o in objs:
		geo.apply_modifiers(o)
		o.matrix_basis = m @ o.matrix_basis
		geo.apply_transform(o)
	return objs


def rot(x=0.0, y=0.0, z=0.0):
	return Matrix.Rotation(math.radians(z), 4, 'Z') @ Matrix.Rotation(math.radians(y), 4, 'Y') @ Matrix.Rotation(math.radians(x), 4, 'X')


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

	def quad(self, u0, u1, z0, z1, n, m, name="quad", u0_top=None, u1_top=None):
		u0t = u0 if u0_top is None else u0_top
		u1t = u1 if u1_top is None else u1_top
		vs = [self.at(u0, n, z0), self.at(u1, n, z0), self.at(u1t, n, z1), self.at(u0t, n, z1)]
		return mesh_obj(name, vs, [(0, 1, 2, 3)], m)

	def tri(self, a, b, c, n, m, name="shard"):
		return mesh_obj(name, [self.at(a[0], n, a[1]), self.at(b[0], n, b[1]), self.at(c[0], n, c[1])], [(0, 1, 2)], m)

	def poly(self, pts, n, m, name="patch"):
		return mesh_obj(name, [self.at(u, n, z) for (u, z) in pts], [tuple(range(len(pts)))], m)

	def crack(self, pts, n, m, w=0.022):
		# A hairline crack: a thin strip following a zigzag in (u, z).
		vs = []
		for (u, z) in pts:
			vs += [self.at(u - w * 0.5, n, z), self.at(u + w * 0.5, n, z)]
		faces = [(2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2) for i in range(len(pts) - 1)]
		return mesh_obj("crack", vs, faces, m)


def sash_window(w, a, b, c, d, alu, glass, dark, rnd, state, board_m=None):
	# An aluminium sliding window: frame, dark room behind, two sashes; state per sash: "ok", "broken", "gone".
	ps = [w.quad(a, b, c, d, 0.004, dark, "window_dark"),
		  w.box((b - a + 0.12, 0.12, 0.05), (a + b) * 0.5, 0.04, d + 0.025, alu, "win_frame"),
		  w.box((b - a + 0.16, 0.16, 0.05), (a + b) * 0.5, 0.06, c - 0.025, alu, "win_sill"),
		  w.box((0.05, 0.12, d - c), a - 0.025, 0.04, (c + d) * 0.5, alu, "win_frame"),
		  w.box((0.05, 0.12, d - c), b + 0.025, 0.04, (c + d) * 0.5, alu, "win_frame")]
	m_ = (a + b) * 0.5
	for k, (u0, u1, nn) in enumerate(((a, m_ + 0.02, 0.03), (m_ - 0.02, b, 0.06))):
		st = state[k]
		if st == "gone":
			continue
		ps.append(w.quad(u0, u1, d - 0.05, d, nn, alu, "sash"))
		ps.append(w.quad(u0, u1, c, c + 0.05, nn, alu, "sash"))
		ps.append(w.quad(u0, u0 + 0.045, c, d, nn, alu, "sash"))
		ps.append(w.quad(u1 - 0.045, u1, c, d, nn, alu, "sash"))
		if st == "ok":
			ps.append(w.quad(u0 + 0.04, u1 - 0.04, c + 0.05, d - 0.05, nn - 0.005, glass, "glass"))
		else:
			du, dz = u1 - u0 - 0.09, d - c - 0.1
			x0, z0 = u0 + 0.045, c + 0.05
			ps.append(w.tri((x0, z0), (x0 + du * rnd.uniform(0.4, 0.7), z0), (x0, z0 + dz * rnd.uniform(0.3, 0.6)), nn - 0.005, glass))
			ps.append(w.tri((x0 + du, z0 + dz), (x0 + du * rnd.uniform(0.3, 0.6), z0 + dz), (x0 + du, z0 + dz * rnd.uniform(0.3, 0.6)), nn - 0.005, glass))
	if board_m is not None:
		for (z0, tilt) in ((c + 0.12, 0.25), (c + 0.5, -0.2)):
			ps.append(w.poly([(a - 0.1, z0), (b + 0.1, z0 + tilt), (b + 0.1, z0 + tilt + 0.15), (a - 0.1, z0 + 0.15)], 0.1, board_m, "board"))
	return ps


def flat_sheet(w, l, m, name="slate_piece"):
	# A loose corrugated piece in its own frame: across X, length along Y from 0.
	cols = max(4, int(round(w / (PERIOD / 4.0))))
	vs = []
	for j in (0.0, l):
		for i in range(cols + 1):
			x = -w * 0.5 + w * i / cols
			vs.append((x, j, AMP * math.sin(2.0 * math.pi * x / PERIOD)))
	faces = [(i, i + 1, cols + 2 + i, cols + 1 + i) for i in range(cols)]
	return mesh_obj(name, vs, faces, m, True)


def sheet(x0, x1, a0, a1, sgn, m, rnd, rows=1, lift0=0.03):
	# One corrugated sheet lying on the roof from a0 (eave side, lifted where it laps) up to a1.
	cols = max(4, int(round((x1 - x0) / (PERIOD / 3.0))))
	jit = rnd.uniform(-0.012, 0.012)
	vs = []
	for j in range(rows + 1):
		t = j / float(rows)
		a = a0 + (a1 - a0) * t
		lift = lift0 * (1.0 - t)
		for i in range(cols + 1):
			x = x0 + (x1 - x0) * i / cols
			y = sgn * a
			vs.append((x, y, roof_z(x, y) + AMP * (1.0 + math.sin(2.0 * math.pi * (x - x0) / PERIOD)) + lift + jit * (1.0 - t)))
	faces = []
	for j in range(rows):
		for i in range(cols):
			p, q = j * (cols + 1) + i, (j + 1) * (cols + 1) + i
			faces.append((p, p + 1, q + 1, q) if sgn < 0 else (q, q + 1, p + 1, p))
	return mesh_obj("slate", vs, faces, m, True)


def leaf_card(c, size, out, roll, m):
	w, h = size * 0.8, size
	vs = [(0, -0.35 * w, 0.1 * h), (-w, 0, 0), (0, 0, h), (w, 0, 0), (0, 0, -0.6 * h)]
	obj = mesh_obj("leaf", vs, [(0, 2, 1), (0, 3, 2), (0, 4, 3), (0, 1, 4)], m, True)
	rz = math.degrees(math.atan2(-out[0], out[1])) + 180.0
	obj.location = Vector(c)
	obj.rotation_euler = (0.0, math.radians(roll), math.radians(rz))
	geo.apply_transform(obj)
	geo.tag(obj, "sway", "foliage")
	return obj


def vine(parts, path, rads, out, vine_m, leaves, rnd, per=2, size=0.13):
	parts.append(geo.tube(path, rads, mat=vine_m, segments=4, name="vine"))
	side = Vector((-out[1], out[0], 0.0))
	for k in range(1, len(path)):
		p0, p1 = Vector(path[k - 1]), Vector(path[k])
		for j in range(per):
			c = p0.lerp(p1, (j + 0.5) / per) + Vector(out) * 0.03 + side * rnd.uniform(-0.13, 0.13)
			c.z += rnd.uniform(-0.05, 0.05)
			parts.append(leaf_card(tuple(c), size * rnd.uniform(0.75, 1.2), out, rnd.uniform(-50, 50), leaves[(k + j) % len(leaves)]))


def moss_pad(radii, x, y, z, rotation, m, seed, segments=None):
	segments = segments or (9 if max(radii[0], radii[1]) > 0.5 else 7)
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, z), rotation, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.22, 1.6, seed=seed)


def slope_rot(x, y):
	e = 0.05
	hx = (roof_z(x + e, y) - roof_z(x - e, y)) / (2 * e)
	hy = (roof_z(x, y + e) - roof_z(x, y - e)) / (2 * e)
	q = Vector((-hx, -hy, 1.0)).normalized().to_track_quat('Z', 'Y').to_euler()
	return (math.degrees(q.x), math.degrees(q.y), math.degrees(q.z))


def eave_moss(x0, x1, sgn, m, seed):
	# Moss rolling over the eave and hanging below it, thinning at both ends.
	pts = []
	sc = []
	for k in range(7):
		t = k / 6.0
		x = x0 + (x1 - x0) * t
		pts.append((x, sgn * RY, roof_z(x, sgn * RY) + 0.05))
		sc.append(0.35 + 0.65 * math.sin(math.pi * t))
	if sgn > 0:
		pts.reverse()
	prof = [(-0.4, 0.1), (-0.3, 0.17), (-0.14, 0.12), (0.02, 0.07), (0.05, -0.05), (0.03, -0.24)]
	pv = [Vector(p) for p in pts]
	verts = []
	for i, p in enumerate(pv):
		t = (pv[min(i + 1, 6)] - pv[max(i - 1, 0)]).normalized()
		side = t.cross(Vector((0, 0, 1))).normalized()
		up = side.cross(t).normalized()
		for (s, u) in prof:
			verts.append(p + (side * s + up * u) * sc[i])
	n = len(prof)
	faces = [(i * n + k, i * n + k + 1, (i + 1) * n + k + 1, (i + 1) * n + k) for i in range(6) for k in range(n - 1)]
	strip = mesh_obj("moss_eave", verts, faces, m, True)
	return geo.jitter(strip, 0.05, 2.5, seed=seed)


def weeds(parts, cx, cy, n, rnd, mats, hmin=0.3, hmax=0.8, spread=0.2, z=0.0):
	for k in range(n):
		h = rnd.uniform(hmin, hmax)
		w = geo.cone(rnd.uniform(0.04, 0.08), h, (cx + rnd.uniform(-spread, spread), cy + rnd.uniform(-spread, spread), z + h * 0.5),
					 (rnd.uniform(-18, 18), rnd.uniform(-18, 18), 0), mat=mats[k % len(mats)], segments=4, name="weed")
		geo.tag(w, "sway", "foliage")
		parts.append(w)


def build():
	slate = mat.make("eh_slate", (0.31, 0.32, 0.32), pattern="slate", rough=0.82, bump=0.6)
	slate_old = mat.make("eh_slate_old", (0.36, 0.36, 0.34), pattern="slate", rough=0.85, bump=0.6)
	cap_m = mat.make("eh_ridge_cap", (0.27, 0.28, 0.28), pattern="concrete", rough=0.85, bump=0.4)
	cement = mat.make("eh_cement", (0.47, 0.46, 0.42), pattern="plaster", rough=0.93, bump=0.35, var=0.16)
	base_m = mat.make("eh_base", (0.33, 0.33, 0.31), pattern="concrete", rough=0.95, bump=0.45)
	block = mat.make("eh_block", (0.52, 0.51, 0.47), pattern="stone", rough=0.93, bump=0.5)
	stain = mat.make("eh_stain", (0.34, 0.33, 0.30), pattern="plaster", rough=0.95, bump=0.2)
	algae = mat.make("eh_algae", (0.30, 0.35, 0.27), pattern="plaster", rough=0.95, bump=0.2)
	crack_m = mat.make("eh_crack", (0.14, 0.13, 0.12), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	dark = mat.make("eh_dark", (0.06, 0.055, 0.05), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	floor_m = mat.make("eh_floor", (0.24, 0.19, 0.14), pattern="paper", rough=0.8, grime=0.3, edge=0.0, bump=0.1)
	alu = mat.make("eh_alu", (0.50, 0.50, 0.48), pattern="rust", rough=0.6, bump=0.25, amount=0.6, accent="rust")
	glass = mat.get("glass")
	wood = mat.get("wood_grey")
	rafter_m = mat.make("eh_rafter", (0.34, 0.28, 0.22), pattern="wood", rough=0.9, bump=0.4)
	board_m = mat.make("eh_board", (0.40, 0.36, 0.30), pattern="planks", rough=0.92, bump=0.45)
	door_wood = mat.make("eh_kitchen_door", (0.38, 0.33, 0.27), pattern="planks", axis='X', rough=0.92, bump=0.45)
	metal = mat.get("metal")
	rust = mat.get("rust")
	tin = mat.get("tin")
	lpg = mat.make("eh_lpg", (0.52, 0.53, 0.52), pattern="rust", rough=0.55, bump=0.25, amount=0.75)
	lpg_band = mat.make("eh_lpg_band", (0.52, 0.24, 0.20), pattern="rust", rough=0.6, bump=0.2, amount=0.6)
	hose = mat.make("eh_hose", (0.42, 0.24, 0.18), pattern="plain", rough=0.7, bump=0.05)
	briq = mat.make("eh_briquette", (0.22, 0.21, 0.20), pattern="concrete", rough=0.97, bump=0.5)
	briq_spent = mat.make("eh_briquette_spent", (0.60, 0.52, 0.46), pattern="concrete", rough=0.97, bump=0.5)
	chair_m = mat.make("eh_chair", (0.42, 0.32, 0.22), pattern="wood", rough=0.85, bump=0.35)
	bark = mat.get("bark")
	vine_m = mat.make("eh_vine", (0.22, 0.25, 0.17), pattern="bark", rough=0.9, bump=0.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	grass = mat.make("eh_grass", (0.32, 0.41, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	dry = mat.make("eh_dry_grass", (0.50, 0.46, 0.32), pattern="leaf", rough=0.9, bump=0.3)
	glow = mat.get("spore_glow")
	rnd = geo.rng(19)

	parts = []
	hard = []

	def add(o):
		parts.append(o)
		hard.append(o)
		return o

	front = Wall((0, -WY, 0), (1, 0, 0), (0, -1, 0), 0)
	back = Wall((0, WY, 0), (-1, 0, 0), (0, 1, 0), 180)
	left = Wall((-WX, 0, 0), (0, -1, 0), (-1, 0, 0), -90)
	right = Wall((WX, 0, 0), (0, 1, 0), (1, 0, 0), 90)

	# Body: block walls skimmed with cement, a darker footing, gable ends, the dark rooms inside.
	ends = lambda n: n.z < -0.9
	add(drop_faces(geo.box((2 * WX, 0.2, WALL_TOP), (0, -WY + 0.1, WALL_TOP * 0.5), mat=cement, name="wall"), ends))
	add(drop_faces(geo.box((2 * WX, 0.2, WALL_TOP), (0, WY - 0.1, WALL_TOP * 0.5), mat=cement, name="wall"), ends))
	add(drop_faces(geo.box((0.2, 2 * WY - 0.4, WALL_TOP), (-WX + 0.1, 0, WALL_TOP * 0.5), mat=cement, name="wall"), ends))
	add(drop_faces(geo.box((0.2, 2 * WY - 0.4, WALL_TOP), (WX - 0.1, 0, WALL_TOP * 0.5), mat=cement, name="wall"), ends))
	gable = geo.prism((2 * WX - 0.02, 2 * WY, RIDGE - 0.12 - WALL_TOP), (0, 0, WALL_TOP), mat=cement, name="wall_gable")
	add(drop_faces(gable, lambda n: abs(n.x) < 0.5))
	for w, (u0, u1) in ((front, (-WX, WX)), (back, (-WX, WX)), (left, (-WY, WY)), (right, (-WY, WY))):
		parts.append(w.quad(u0, u1, 0.0, PLINTH, 0.005, base_m, "footing"))
	ix, iy = WX - 0.205, WY - 0.205
	for (a, b) in (((-ix, -iy), (ix, -iy)), ((ix, -iy), (ix, iy)), ((ix, iy), (-ix, iy)), ((-ix, iy), (-ix, -iy))):
		parts.append(mesh_obj("lining", [(a[0], a[1], PLINTH), (b[0], b[1], PLINTH), (b[0], b[1], WALL_TOP), (a[0], a[1], WALL_TOP)], [(0, 1, 2, 3)], dark))
	parts.append(mesh_obj("floor", [(-ix, -iy, PLINTH), (ix, -iy, PLINTH), (ix, iy, PLINTH), (-ix, iy, PLINTH)], [(0, 1, 2, 3)], floor_m))
	parts.append(mesh_obj("partition", [(-0.4, -iy, PLINTH), (-0.4, iy, PLINTH), (-0.4, iy, WALL_TOP), (-0.4, -iy, WALL_TOP)], [(0, 1, 2, 3)], dark))

	# The front: a sliding door left ajar, a broken window, a boarded one, the step.
	d0, d1, dz0, dz1 = -0.85, 0.85, PLINTH, 2.05
	parts.append(front.quad(d0, d1, dz0, dz1, 0.004, dark, "door_dark"))
	parts.append(front.box((d1 - d0 + 0.14, 0.14, 0.06), 0.0, 0.05, dz1 + 0.03, alu, "door_frame"))
	for u in (d0 - 0.035, d1 + 0.035):
		parts.append(front.box((0.07, 0.14, dz1 - dz0), u, 0.05, (dz0 + dz1) * 0.5, alu, "door_frame"))
	for k, (u0, u1, nn, state) in enumerate(((-0.45, 0.42, 0.03, "broken"), (0.0, d1, 0.075, "ok"))):
		parts.append(front.quad(u0, u1, dz1 - 0.06, dz1, nn, alu, "sash"))
		parts.append(front.quad(u0, u1, dz0, dz0 + 0.14, nn, alu, "sash"))
		parts.append(front.quad(u0, u0 + 0.06, dz0, dz1, nn, alu, "sash"))
		parts.append(front.quad(u1 - 0.06, u1, dz0, dz1, nn, alu, "sash"))
		parts.append(front.quad(u0, u1, 1.15, 1.2, nn, alu, "sash"))
		if state == "ok":
			parts.append(front.quad(u0 + 0.05, u1 - 0.05, dz0 + 0.14, 1.15, nn - 0.005, glass, "glass"))
			parts.append(front.quad(u0 + 0.05, u1 - 0.05, 1.2, dz1 - 0.06, nn - 0.005, glass, "glass"))
		else:
			parts.append(front.tri((u0 + 0.06, 1.2), (u0 + 0.45, 1.2), (u0 + 0.06, 1.6), nn - 0.005, glass))
			parts.append(front.tri((u1 - 0.06, dz1 - 0.06), (u1 - 0.4, dz1 - 0.06), (u1 - 0.06, 1.7), nn - 0.005, glass))
			parts.append(front.quad(u0 + 0.05, u1 - 0.05, dz0 + 0.14, 1.15, nn - 0.005, glass, "glass"))
	parts.append(front.box((0.04, 0.05, 0.25), 0.38, 0.11, 1.1, metal, "door_handle"))
	parts += sash_window(front, -2.65, -1.55, 1.0, 1.85, alu, glass, dark, rnd, ("broken", "gone"))
	parts += sash_window(front, 1.55, 2.65, 1.0, 1.85, alu, glass, dark, rnd, ("ok", "broken"), board_m)
	add(drop_faces(geo.box((2.3, 0.5, PLINTH - 0.04), (0.0, -WY - 0.25, (PLINTH - 0.04) * 0.5), mat=base_m, name="step"), lambda n: n.z < -0.9))
	add(geo.box((0.7, 0.36, 0.14), (0.25, -WY - 0.68, 0.06), (2, -3, 5), mat=base_m, name="step_low"))
	parts.append(geo.box((0.02, 0.4, 0.01), (-0.5, -WY - 0.25, PLINTH - 0.035), (0, 0, 25), mat=crack_m, name="step_crack"))

	# The left end: a small window, cracks, a patch where the skim fell off showing the blocks.
	parts += sash_window(left, -0.5, 0.45, 1.15, 1.8, alu, glass, dark, rnd, ("gone", "broken"))
	# The back: a plank kitchen door with a padlock, the barred bathroom window.
	kd = 1.6
	parts.append(back.box((0.95, 0.1, 1.75), kd, 0.03, PLINTH + 0.875, rafter_m, "kitchen_frame"))
	parts.append(back.box((0.82, 0.05, 1.65), kd, 0.07, PLINTH + 0.84, door_wood, "kitchen_door"))
	parts.append(back.box((0.06, 0.04, 0.1), kd - 0.32, 0.11, 1.15, rust, "hasp"))
	parts.append(back.box((0.08, 0.03, 0.1), kd - 0.32, 0.135, 1.06, metal, "padlock"))
	parts.append(back.box((1.1, 0.4, 0.2), kd, 0.2, 0.1, base_m, "back_step"))
	bw0, bw1, bz0, bz1 = -1.7, -1.15, 1.55, 2.0
	parts.append(back.quad(bw0, bw1, bz0, bz1, 0.004, dark, "window_dark"))
	parts.append(back.quad(bw0 + 0.04, (bw0 + bw1) * 0.5, bz0 + 0.04, bz1 - 0.04, 0.01, mat.make("eh_frosted", (0.46, 0.50, 0.50), rough=0.5, bump=0.05), "frosted"))
	parts.append(back.box((bw1 - bw0 + 0.1, 0.12, 0.05), (bw0 + bw1) * 0.5, 0.05, bz0 - 0.025, alu, "win_sill"))
	for k in range(4):
		uu = bw0 + (bw1 - bw0) * (k + 0.5) / 4.0
		parts.append(back.quad(uu - 0.013, uu + 0.013, bz0 - 0.03, bz1 + 0.03, 0.08, rust, "bar"))
	parts.append(back.quad(bw0 - 0.05, bw1 + 0.05, bz1 - 0.06, bz1 - 0.03, 0.085, rust, "bar_h"))
	# The stovepipe: out of the wall, up past the eave, then bent over where a branch or the wind took it.
	sp = [back.at(0.3, -0.05, 0.75), back.at(0.3, 0.36, 0.78), back.at(0.3, 0.42, 0.95), back.at(0.3, 0.42, 2.55), back.at(0.18, 0.5, 2.95),
		  back.at(-0.15, 0.56, 3.12)]
	parts.append(geo.tube(sp, 0.075, mat=tin, segments=8, name="stovepipe"))
	hat = geo.cone(0.17, 0.14, (0, 0, 0.1), mat=rust, segments=8, name="pipe_hat")
	parts.append(place([hat], Matrix.Translation(back.at(-0.22, 0.57, 3.12)) @ rot(0, -75, 0))[0])
	for z in (1.3, 2.1):
		parts.append(geo.tube([back.at(0.3, 0.0, z), back.at(0.3, 0.34, z)], 0.012, mat=rust, segments=3, name="pipe_stay"))
	parts.append(geo.cylinder(0.2, 0.01, back.at(0.3, 0.003, 0.76), (90, 0, 0), mat=crack_m, segments=10, name="soot"))
	parts.append(back.quad(0.18, 0.42, 0.85, 1.9, 0.004, stain, "soot_streak", 0.24, 0.36))
	# Briquettes: a stack of spent pale ones and a few black ones still whole.
	k = 0
	for (u, n0, rows) in ((-0.62, 0.22, 4), (-0.88, 0.22, 3), (-0.75, 0.46, 2)):
		for r_ in range(rows):
			m = briq_spent if (k % 3) else briq
			c = geo.cylinder(0.11, 0.19, back.at(u + rnd.uniform(-0.015, 0.015), n0, 0.095 + r_ * 0.195), mat=m, segments=7, name="briquette")
			parts.append(c)
			k += 1
	parts.append(geo.cylinder(0.11, 0.19, back.at(-1.2, 0.6, 0.11), (80, 0, 30), mat=briq_spent, segments=7, name="briquette"))

	# The right end: LPG cylinder with its hose, a meter box, the vent.
	lx, ly = WX + 0.25, -1.45
	add(geo.lathe([(0.15, 0.0), (0.16, 0.03), (0.16, 0.78), (0.13, 0.88), (0.06, 0.92), (0.0, 0.93)], (lx, ly, 0.0), mat=lpg, segments=10, name="lpg"))
	parts.append(geo.cylinder(0.162, 0.12, (lx, ly, 0.55), mat=lpg_band, segments=10, name="lpg_band"))
	parts.append(geo.cylinder(0.1, 0.12, (lx, ly, 0.98), mat=lpg, segments=8, name="lpg_collar", caps=False))
	parts.append(geo.box((0.06, 0.05, 0.08), (lx, ly, 0.97), mat=metal, name="valve"))
	parts.append(geo.tube([(lx, ly, 0.98), (lx - 0.1, ly + 0.1, 1.08), (WX + 0.03, ly + 0.3, 1.25), (WX + 0.02, ly + 0.5, 1.25)], 0.014, mat=hose, segments=4, name="hose"))
	parts.append(geo.tube([(lx + 0.12, ly - 0.08, 0.9), (lx + 0.22, ly - 0.2, 0.6), (lx + 0.18, ly - 0.28, 0.3)], 0.008, mat=rust, segments=3, name="chain"))
	parts.append(right.box((0.36, 0.14, 0.42), 0.9, 0.07, 1.75, alu, "meter_box"))
	parts.append(right.quad(0.82, 0.98, 0.6, 1.53, 0.003, rust, "rust_run", 0.8, 1.0))
	for (w, s) in ((left, 1.0), (right, 1.0)):
		parts.append(w.quad(-0.18, 0.18, 2.75, 3.0, 0.005, dark, "vent"))
		for k in range(3):
			parts.append(w.quad(-0.2, 0.2, 2.77 + k * 0.08, 2.81 + k * 0.08, 0.02, alu, "vent_slat"))

	# Weathering: rain stains, green algae at the foot, cracks, a skim patch fallen off.
	for (w, spans) in ((front, ((-2.6, -1.6, 0.98), (1.6, 2.6, 0.98), (-3.2, -2.8, WALL_TOP), (2.8, 3.2, WALL_TOP), (-0.6, 0.6, WALL_TOP))),
					   (back, ((-3.0, -2.2, WALL_TOP), (-1.6, -1.2, 1.53), (2.4, 3.0, WALL_TOP))),
					   (left, ((-2.0, -1.0, WALL_TOP), (-0.4, 0.4, 1.12), (1.2, 2.2, WALL_TOP))),
					   (right, ((-2.0, -1.2, WALL_TOP), (1.4, 2.2, WALL_TOP)))):
		for (u0, u1, z) in spans:
			for k in range(2):
				u = rnd.uniform(u0, u1)
				wd = rnd.uniform(0.06, 0.12)
				parts.append(w.quad(u - wd * 0.15, u + wd * 0.15, max(PLINTH, z - rnd.uniform(0.5, 1.1)), z, 0.004, stain, "stain", u - wd * 0.5, u + wd * 0.5))
	for (w, u0, u1) in ((front, -WX, -1.0), (front, 1.0, WX), (back, -WX, WX), (left, -WY, WY), (right, -WY, 0.4)):
		pts = [(u0, PLINTH)]
		steps = max(3, int((u1 - u0) / 0.5))
		for k in range(steps + 1):
			pts.append((u1 - (u1 - u0) * k / steps, PLINTH + rnd.uniform(0.12, 0.45)))
		pts[1] = (u1, PLINTH)
		parts.append(w.poly([(u0, PLINTH), (u1, PLINTH)] + pts[2:], 0.006, algae, "algae"))
	parts.append(front.crack([(-1.55, 1.85), (-1.35, 2.05), (-1.4, 2.2), (-1.2, WALL_TOP)], 0.006, crack_m))
	parts.append(front.crack([(1.55, 1.0), (1.3, 0.75), (1.38, 0.55), (1.15, PLINTH)], 0.006, crack_m))
	parts.append(left.crack([(0.45, 1.8), (0.8, 2.1), (0.75, 2.4), (1.1, 2.75), (1.0, 3.0)], 0.006, crack_m))
	parts.append(back.crack([(-2.6, PLINTH), (-2.4, 0.8), (-2.55, 1.3), (-2.3, 1.9)], 0.006, crack_m))
	for (w, u, z, ru, rz_) in ((front, 2.95, 0.75, 0.32, 0.22), (left, 1.6, 1.5, 0.4, 0.3), (back, 2.6, 1.2, 0.35, 0.25)):
		pts = [(u + math.cos(math.tau * k / 10) * ru * rnd.uniform(0.7, 1.0), z + math.sin(math.tau * k / 10) * rz_ * rnd.uniform(0.7, 1.0)) for k in range(10)]
		parts.append(w.poly(pts, 0.006, block, "skim_off"))

	# The roof: corrugated slate a sheet at a time, the front-left sheets gone into the room.
	gone = {(1, 0), (1, 1), (2, 1)}
	cols = 7
	cw = 2.0 * RX / cols
	courses = ((1.42, RY), (0.0, 1.52))
	for sgn in (-1.0, 1.0):
		for i in range(cols):
			for c, (a1, a0) in enumerate(courses):
				if sgn < 0 and (i, c) in gone:
					continue
				x0 = -RX + cw * i - (0.02 if i else 0.0)
				x1 = -RX + cw * (i + 1)
				near = sgn < 0 and (abs((x0 + x1) * 0.5 - CX) < 2.4)
				m = slate_old if (i + c) % 3 == 0 else slate
				s = sheet(x0, x1, a0, a1, sgn, m, rnd, 2 if near else 1, 0.03 if c == 1 else 0.0)
				if not near and rnd.random() < 0.25:
					s.location.z += rnd.uniform(0.0, 0.03)
				add(s)
	# Ridge cap, broken over the hole; barge boards on the gable edges.
	prof = [(-0.22, -0.06), (0.0, 0.07), (0.22, -0.06)]
	for (xa, xb) in ((-RX - 0.02, -2.55), (-1.15, RX + 0.02)):
		n = max(2, int((xb - xa) / 0.5))
		pts = [(xa + (xb - xa) * k / n, 0.0, roof_z(xa + (xb - xa) * k / n, 0.0) + 0.06) for k in range(n + 1)]
		add(sweep("ridge_cap", pts, prof, cap_m, True, True))
	cap_piece = sweep("ridge_cap", [(-0.45, 0, 0), (0.45, 0, 0)], prof, cap_m, True, True)
	add(place([cap_piece], Matrix.Translation((-0.9, -0.9, roof_z(-0.9, -0.9) + 0.1)) @ rot(-22, 8, 35))[0])
	slope_len = math.hypot(RY, SLOPE * RY)
	ang = math.degrees(math.atan(SLOPE))
	for sx in (-1.0, 1.0):
		for sy in (-1.0, 1.0):
			add(geo.box((0.04, slope_len, 0.16), (sx * (RX + 0.02), sy * RY * 0.5, plane(RY * 0.5) - 0.05), (sy * -ang, 0, 0), mat=wood, name="barge"))
	# In the hole: rafters (one snapped, its end down in the room), broken battens, loose sheets.
	for x in (-2.5, -1.95, -1.4, -0.9):
		if x == -1.95:
			pts = [(x, -a, roof_z(x, -a) - 0.08) for a in (0.0, 0.5, 1.0)]
			parts.append(geo.tube(pts, 0.045, mat=rafter_m, segments=4, name="rafter"))
			parts.append(geo.tube([(x + 0.05, -1.15, roof_z(x, -1.0) - 0.25), (x + 0.12, -1.55, 1.2), (x + 0.2, -1.8, PLINTH + 0.05)], 0.045,
								  mat=rafter_m, segments=4, name="rafter"))
			continue
		pts = [(x, -a, roof_z(x, -a) - 0.08) for a in (0.0, 0.7, 1.4, 2.1, RY + 0.05)]
		parts.append(geo.tube(pts, 0.045, mat=rafter_m, segments=4, name="rafter"))
	for (a, xa, xb) in ((0.55, -2.75, -1.15), (1.35, -2.75, -2.15), (1.35, -1.55, -0.75), (2.2, -2.75, -0.75)):
		pts = [(xa + (xb - xa) * k / 3.0, -a, roof_z(xa + (xb - xa) * k / 3.0, -a) - 0.02) for k in range(4)]
		parts.append(geo.tube(pts, 0.03, mat=rafter_m, segments=4, name="batten"))
	hang = flat_sheet(0.95, 1.3, slate_old)
	add(place([hang], Matrix.Translation((-2.05, -0.5, roof_z(-2.05, -0.5) - 0.05)) @ rot(-112, 6, 4))[0])
	for (x, y, z, rx_, ry_, rz_, w_, l_) in ((-1.3, -1.5, PLINTH + 0.25, 18, -10, 30, 0.9, 1.1), (-2.4, -1.0, PLINTH + 0.12, -8, 12, -20, 0.7, 0.8)):
		p = flat_sheet(w_, l_, slate)
		add(place([p], Matrix.Translation((x, y, z)) @ rot(rx_, ry_, rz_))[0])
	for k in range(3):
		parts.append(geo.box((rnd.uniform(0.15, 0.35), rnd.uniform(0.12, 0.3), 0.025), (rnd.uniform(-2.7, -0.8), rnd.uniform(-2.0, -0.4), PLINTH + 0.03),
							 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), rnd.uniform(0, 180)), mat=slate, name="slate_bit"))

	# The TV antenna on the ridge, leaning out over the right end.
	ax = 2.75
	base_p = Vector((ax, 0.0, roof_z(ax, 0.0) + 0.08))
	top = base_p + Vector((1.25, 0.55, 1.15))
	parts.append(geo.tube([tuple(base_p), tuple(top)], 0.022, mat=rust, segments=5, name="antenna"))
	d = (top - base_p).normalized()
	side = d.cross(Vector((0, 0, 1))).normalized()
	for k, h in enumerate((0.55, 0.75, 0.95)):
		p = base_p.lerp(top, h)
		wdt = 0.55 - k * 0.12
		parts.append(geo.tube([tuple(p - side * wdt * 0.5), tuple(p + side * wdt * 0.5 + Vector((0, 0, -0.05)))], 0.011, mat=rust, segments=3, name="antenna_bar"))
	parts.append(geo.cylinder(0.08, 0.06, tuple(base_p), mat=rust, segments=6, name="antenna_foot"))
	parts.append(geo.tube([tuple(base_p.lerp(top, 0.6)), (WX + 0.2, 0.9, 2.2), (WX + 0.18, 1.0, 1.4)], 0.006, mat=metal, segments=3, name="lead"))

	# The broken chair out front.
	ch = [geo.box((0.42, 0.40, 0.04), (0, 0, 0.44), mat=chair_m, name="chair_seat")]
	for (dx, dy, h) in ((-0.18, -0.17, 0.44), (0.18, -0.17, 0.44), (-0.18, 0.17, 0.9), (0.18, 0.17, 0.62)):
		ch.append(geo.box((0.04, 0.04, h), (dx, dy, h * 0.5), mat=chair_m, name="chair_leg"))
	ch.append(geo.box((0.42, 0.03, 0.08), (0.0, 0.17, 0.82), (0, 0, 0), mat=chair_m, name="chair_rail"))
	place(ch, Matrix.Translation((-2.1, -WY - 0.75, 0.21)) @ rot(0, 72, 30))
	parts += ch
	parts.append(geo.box((0.04, 0.04, 0.42), (-1.55, -WY - 0.8, 0.02), (90, 0, 65), mat=chair_m, name="chair_leg_loose"))

	# Nature: moss thick on the slate and over the eaves, tall weeds, vines up the left end, two trees.
	pads = [((-2.9, -1.9), (0.7, 0.5, 0.11), moss), ((0.6, -1.2), (0.9, 0.55, 0.12), moss), ((2.3, -2.2), (0.6, 0.45, 0.1), moss_dark),
			((-0.3, -0.5), (0.45, 0.3, 0.08), moss_dark), ((-2.2, 1.6), (0.9, 0.6, 0.12), moss), ((1.2, 1.9), (0.8, 0.55, 0.11), moss_dark),
			((2.9, 0.9), (0.5, 0.4, 0.09), moss), ((-0.6, 1.1), (0.55, 0.35, 0.08), moss), ((1.8, -0.9), (0.4, 0.3, 0.08), moss_dark)]
	for k, ((x, y), r, m) in enumerate(pads):
		p = moss_pad(r, x, y, roof_z(x, y) + 0.01, slope_rot(x, y), m, k + 3)
		if k in (1, 4):
			geo.tag(p, "pulse")
		parts.append(p)
	parts.append(eave_moss(-3.4, -2.6, -1.0, moss, 41))
	parts.append(eave_moss(0.0, 2.9, -1.0, moss_dark, 42))
	parts.append(eave_moss(-3.0, 0.5, 1.0, moss, 43))
	parts.append(eave_moss(1.5, 3.3, 1.0, moss_dark, 44))
	ridge = moss_pad((0.7, 0.22, 0.12), 1.6, 0.0, roof_z(1.6, 0.0) + 0.08, (0, 0, 0), moss, 45)
	geo.tag(ridge, "pulse")
	parts.append(ridge)
	for k, (x, y, r) in enumerate(((-WX - 0.08, -1.0, (0.24, 1.1, 0.16)), (WX + 0.06, 0.8, (0.2, 0.8, 0.14)), (-1.6, WY + 0.08, (0.9, 0.22, 0.15)),
								   (2.4, -WY - 0.06, (0.6, 0.18, 0.13)), (0.3, -WY - 0.5, (0.5, 0.2, 0.06)), (-1.4, 0.0, (0.5, 0.45, 0.07)))):
		z = -0.03 if k < 5 else PLINTH - 0.02
		parts.append(moss_pad(r, x, y, z, (0, 0, 0), moss_dark if k % 2 else moss, 50 + k))

	for (cx, cy, n) in ((-WX - 0.25, -WY - 0.2, 5), (WX + 0.25, -WY - 0.25, 4), (-WX - 0.25, WY + 0.25, 4), (WX + 0.3, WY + 0.2, 5),
						(-1.2, -WY - 0.45, 4), (1.1, -WY - 0.4, 3), (-WX - 0.3, 0.6, 4), (WX + 0.35, 0.2, 3), (-0.8, WY + 0.4, 3),
						(2.2, WY + 0.35, 3), (-2.6, -WY - 0.6, 3), (0.9, -WY - 0.6, 3)):
		weeds(parts, cx, cy, n, rnd, (grass, leaf, dry, leaf_dark), 0.35, 0.9, 0.25)
	weeds(parts, -1.5, -1.3, 4, rnd, (grass, leaf), 0.3, 0.6, 0.35, PLINTH)

	leaves = (leaf, leaf_dark)
	for (u, top, sway_) in ((-1.9, 2.6, 0.18), (-1.2, 3.0, 0.14), (1.4, 2.4, 0.2), (2.1, 1.9, 0.12)):
		pts = [left.at(u + sway_ * math.sin(k * 1.8), 0.03, top * k / 4.0 + 0.05) for k in range(5)]
		vine(parts, pts, [0.03, 0.026, 0.022, 0.016, 0.012], (-1, 0, 0), vine_m, leaves, rnd, 3, 0.15)
	for (x, drop) in ((2.3, 1.2), (2.65, 0.8)):
		pts = [(x, -RY + 0.3, roof_z(x, -RY + 0.3) + 0.05), (x + 0.03, -RY - 0.05, roof_z(x, -RY) - 0.05)]
		for k in range(1, 3):
			pts.append((x + 0.06 * k, -RY - 0.08, roof_z(x, -RY) - 0.05 - drop * k / 2.0))
		vine(parts, pts, [0.025, 0.022, 0.016, 0.012], (0, -1, 0), vine_m, leaves, rnd, 2, 0.13)
	for (x, y, z, r) in ((-WX - 0.06, -1.5, 2.3, 0.3), (-WX - 0.06, 1.3, 2.2, 0.28), (-RX + 0.1, -RY + 0.2, roof_z(-RX + 0.1, -RY + 0.2) + 0.1, 0.3),
						 (2.45, -RY - 0.05, roof_z(2.45, -RY) + 0.05, 0.26)):
		c = geo.blob((r, r, r * 0.8), (x, y, z), mat=leaf, subdivisions=1, lumpy=0.3, seed=int(abs(x * y) * 10), name="vine_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	# A young tree come up inside the house and out through the hole; a scrubby one at the back corner.
	tx, ty = -1.75, -1.25
	parts.append(geo.tube([(tx, ty, PLINTH), (tx + 0.08, ty + 0.05, 1.4), (tx + 0.02, ty - 0.1, 2.6), (tx - 0.1, ty - 0.2, 3.3)], [0.09, 0.07, 0.05, 0.03],
						  mat=bark, segments=5, name="trunk"))
	parts.append(geo.tube([(tx + 0.05, ty, 2.2), (tx + 0.45, ty - 0.2, 2.9), (tx + 0.6, ty - 0.3, 3.2)], [0.04, 0.025, 0.015], mat=bark, segments=4, name="branch"))
	for k, (dx, dy, dz, r) in enumerate(((-0.1, -0.2, 3.45, 0.62), (0.5, -0.35, 3.25, 0.5), (-0.55, 0.05, 3.2, 0.45))):
		b = geo.blob((r, r * 0.95, r * 0.7), (tx + dx, ty + dy, dz), mat=leaf if k % 2 == 0 else leaf_dark, subdivisions=2, lumpy=0.35, seed=k + 80, name="crown")
		geo.tag(b, "sway", "foliage")
		parts.append(b)
	bx, by = -WX - 0.05, WY + 0.1
	parts.append(geo.tube([(bx, by, 0.0), (bx + 0.05, by, 0.8), (bx + 0.15, by - 0.05, 1.5)], [0.07, 0.05, 0.03], mat=bark, segments=5, name="trunk"))
	for k, (dx, dy, dz, r) in enumerate(((0.1, 0.0, 1.7, 0.55), (-0.25, 0.15, 1.25, 0.45), (0.45, -0.1, 1.15, 0.4))):
		b = geo.blob((r, r * 0.9, r * 0.8), (bx + dx, by + dy, dz), mat=leaf_dark if k % 2 == 0 else leaf, subdivisions=2, lumpy=0.4, seed=k + 90, name="shrub")
		geo.tag(b, "sway", "foliage")
		parts.append(b)
	g = geo.blob(0.05, (-1.6, -1.3, 3.0), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(hard):
		geo.moss_mask(p, up=1.2, low=0.7, seed=i + 2)

	res = pipeline.static("empty_house", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
