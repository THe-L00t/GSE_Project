# A three-storey concrete-frame shop building, mostly fallen. The back-left corner still stands two floors high:
# columns, slabs and broken brick infill, one papered room on the second floor open to the air with its lamp
# still hanging. The rest has pancaked into tilted slabs on a rubble heap spilling to the front, rebar curling
# out of every broken edge, the restaurant's sign lying face-up in the rubble, a tree grown out of the heap,
# moss on the slabs and vines hanging down the standing faces.
import math

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

from gse import core, geo, mat, pipeline

HX, HY = 4.5, 4.0
L1, L2 = 3.0, 5.95
SL = 0.22
CW = 0.4
BEAM = 0.35
GX0, GX1, GY0, GY1 = -4.7, 4.9, -4.8, 4.6
SINK = 0.05
MOUNDS = [
	(1.4, 2.4, 3.6, 4.2, 2.55),
	(0.6, -1.8, 3.6, 3.0, 1.25),
	(-2.7, -1.6, 2.0, 2.4, 0.9),
	(-2.6, 2.0, 1.6, 1.6, 0.45),
	(4.0, 0.0, 1.0, 2.6, 0.5),
]


def heap(x, y):
	# Height of the rubble: a few soft mounds, a little noise, falling to nothing at the grid's rim.
	h = 0.0
	for (cx, cy, rx, ry, a) in MOUNDS:
		d = 1.0 - ((x - cx) / rx) ** 2 - ((y - cy) / ry) ** 2
		if d > 0.0:
			h += a * d ** 1.4
	h += 0.14 * noise.noise(Vector((x * 1.1, y * 1.1, 0.7))) * min(1.0, h)
	edge = min(x - GX0, GX1 - x, y - GY0, GY1 - y) / 0.9
	# Sunk a little, so the heap's rim is wherever it meets the ground rather than a grid edge.
	return max(0.0, h) * max(0.0, min(1.0, edge)) - SINK


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


def flat_text(body, size, loc, rot, m, angle=30.0):
	# Painted lettering: geo.text with only the front faces kept and the curves thinned.
	obj = geo.text(body, size, loc, rot, m, depth=0.003)
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bm.normal_update()
	bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < 0.5], context='FACES')
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(angle), use_dissolve_boundaries=False, verts=bm.verts[:], edges=bm.edges[:])
	bmesh.ops.triangulate(bm, faces=bm.faces[:])
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


def on_heap(cx, cy, span, lift, ex=0.0, ey=0.0, rz=0.0):
	# A frame lying on the heap's slope at (cx, cy), tipped further by ex/ey and turned by rz.
	sx = (heap(cx + span, cy) - heap(cx - span, cy)) / (2.0 * span)
	sy = (heap(cx, cy + span) - heap(cx, cy - span)) / (2.0 * span)
	n = Vector((-sx, -sy, 1.0)).normalized()
	align = n.to_track_quat('Z', 'Y').to_matrix().to_4x4()
	return Matrix.Translation((cx, cy, heap(cx, cy) + lift)) @ align @ rot(ex, ey, rz)


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

	def jag(self, pts, t, m, name="broken_wall"):
		# A slab of wall cut to an outline in (u, z), from the face back t into the building.
		k = len(pts)
		vs = [self.at(u, 0.0, z) for (u, z) in pts] + [self.at(u, -t, z) for (u, z) in pts]
		faces = [tuple(range(k)), tuple(range(2 * k - 1, k - 1, -1))]
		for i in range(k):
			j = (i + 1) % k
			faces.append((i, k + i, k + j, j))
		obj = mesh_obj(name, vs, faces, m)
		bm = bmesh.new()
		bm.from_mesh(obj.data)
		bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
		bm.to_mesh(obj.data)
		bm.free()
		return obj


def panel(w, u0, u1, z0, z1, holes, ext_m, t=0.2, inner=None, dado=None):
	# An infill panel between columns with rectangular openings; the room side papered or tiled.
	rects = []
	u = u0
	for (a, b, c, d) in sorted(holes):
		if a > u:
			rects.append((u, a, z0, z1))
		if c > z0:
			rects.append((a, b, z0, c))
		if d < z1:
			rects.append((a, b, d, z1))
		u = b
	if u < u1:
		rects.append((u, u1, z0, z1))
	ps = []
	for (a, b, c, d) in rects:
		ps.append(w.box((b - a, t, d - c), (a + b) * 0.5, -t * 0.5, (c + d) * 0.5, ext_m, "infill"))
		if inner is None:
			continue
		if dado is not None and c < dado[0]:
			ps.append(w.quad(a, b, c, min(d, dado[0]), -t - 0.004, dado[1], "dado"))
			if d > dado[0]:
				ps.append(w.quad(a, b, dado[0], d, -t - 0.004, inner, "paper"))
		else:
			ps.append(w.quad(a, b, c, d, -t - 0.004, inner, "paper"))
	return ps


def window(w, a, b, c, d, frame_m, glass_m, rnd, bars_m=None, t=0.2):
	# An aluminium sash in the hole, its glass broken back to teeth, burglar bars outside if given.
	ps = [w.box((b - a + 0.12, 0.26, 0.06), (a + b) * 0.5, -0.07, c - 0.03, frame_m, "win_sill"),
		  w.box((b - a, 0.08, 0.05), (a + b) * 0.5, -t * 0.5, d - 0.025, frame_m, "win_frame"),
		  w.box((0.05, 0.08, d - c), a + 0.025, -t * 0.5, (c + d) * 0.5, frame_m, "win_frame"),
		  w.box((0.05, 0.08, d - c), b - 0.025, -t * 0.5, (c + d) * 0.5, frame_m, "win_frame"),
		  w.box((0.04, 0.06, d - c), (a + b) * 0.5, -t * 0.5, (c + d) * 0.5, frame_m, "win_frame")]
	du, dz = (b - a) * 0.5 - 0.05, d - c - 0.05
	for (u0, s) in ((a + 0.05, 1.0), ((a + b) * 0.5 + 0.02, -1.0)):
		ps.append(w.tri((u0, c), (u0 + du * rnd.uniform(0.4, 0.8), c), (u0, c + dz * rnd.uniform(0.3, 0.6)), -t * 0.5, glass_m))
		ps.append(w.tri((u0 + du, d - 0.05), (u0 + du * rnd.uniform(0.3, 0.6), d - 0.05), (u0 + du, d - dz * rnd.uniform(0.3, 0.5)), -t * 0.5, glass_m))
	if bars_m is not None:
		for k in range(5):
			uu = a + (b - a) * (k + 0.5) / 5.0
			ps.append(w.quad(uu - 0.014, uu + 0.014, c - 0.04, d + 0.04, 0.08, bars_m, "bar"))
		for zz in (c + 0.05, (c + d) * 0.5, d - 0.05):
			ps.append(w.quad(a - 0.06, b + 0.06, zz - 0.015, zz + 0.015, 0.085, bars_m, "bar_h"))
	return ps


def rebar(p, d, length, rnd, m, droop=0.5):
	p = Vector(p)
	d = Vector(d).normalized()
	p1 = p + d * length * 0.5
	p2 = p + d * length + Vector((rnd.uniform(-0.12, 0.12), rnd.uniform(-0.12, 0.12), -droop * length * rnd.uniform(0.3, 1.0)))
	p1.z = max(p1.z, 0.04)
	p2.z = max(p2.z, 0.03)
	return geo.tube([p - d * 0.08, p1, p2], 0.016, mat=m, segments=3, name="rebar")


def edge_rebar(outline, mw, picks, rnd, m, z=0.0, droop=0.5, per=1):
	# Bars poking out of chosen edges of a slab outline (local), carried into the slab's frame mw.
	cx = sum(p[0] for p in outline) / len(outline)
	cy = sum(p[1] for p in outline) / len(outline)
	out = []
	for i in picks:
		a, b = Vector(outline[i]), Vector(outline[(i + 1) % len(outline)])
		mid = a.lerp(b, rnd.uniform(0.3, 0.7))
		e = (b - a).normalized()
		nrm = Vector((e.y, -e.x))
		if nrm.dot(mid - Vector((cx, cy))) < 0.0:
			nrm = -nrm
		for k in range(per):
			p = mid + e * (k * 0.22 - 0.11 * (per - 1))
			wp = mw @ Vector((p.x, p.y, z))
			wd = mw.to_3x3() @ Vector((nrm.x, nrm.y, rnd.uniform(-0.2, 0.4)))
			out.append(rebar(wp, wd, rnd.uniform(0.35, 0.7), rnd, m, droop))
	return out


def column(x, y, z0, z1, m, rust_m, rnd, tilt=(0.0, 0.0), bars=True):
	# A frame column with its top broken off ragged and the bars standing out of the break.
	c = geo.box((CW, CW, z1 - z0), (x, y, (z0 + z1) * 0.5), mat=m, name="column")
	geo.apply_transform(c)
	for v in c.data.vertices:
		if v.co.z > z1 - 0.01:
			v.co.z -= rnd.uniform(0.0, 0.28)
	ps = [c]
	if bars:
		for (dx, dy) in ((-0.12, -0.12), (0.12, -0.12), (-0.12, 0.12), (0.12, 0.12)):
			if rnd.random() < 0.8:
				ps.append(rebar((x + dx, y + dy, z1 - 0.2), (dx * rnd.uniform(0.5, 2.0), dy * rnd.uniform(0.5, 2.0), 1.0),
								rnd.uniform(0.3, 0.6), rnd, rust_m, 0.3))
	if tilt != (0.0, 0.0):
		place(ps, Matrix.Translation((x, y, z0)) @ rot(tilt[0], tilt[1], 0) @ Matrix.Translation((-x, -y, -z0)))
	return ps


def leaf_card(c, size, out, roll, m):
	# A cheap leaf pressed to a surface: a diamond with a raised middle, facing along out.
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
			c = p0.lerp(p1, (j + 0.5) / per) + Vector(out) * 0.03 + side * rnd.uniform(-0.12, 0.12)
			c.z += rnd.uniform(-0.05, 0.05)
			parts.append(leaf_card(tuple(c), size * rnd.uniform(0.75, 1.2), out, rnd.uniform(-50, 50), leaves[(k + j) % len(leaves)]))


def moss_pad(radii, x, y, z, rotation, m, seed, segments=None):
	segments = segments or (11 if max(radii[0], radii[1]) > 0.5 else 8)
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, z), rotation, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.22, 1.6, seed=seed)


def chunk(size, loc, rotation, m, seed, name="chunk", subdiv=0):
	c = geo.box(size, loc, rotation, mat=m, subdiv=subdiv, name=name)
	geo.jitter(c, min(size) * 0.2, scale=3.0, seed=seed)
	return c


def weeds(parts, cx, cy, n, rnd, mats, hmin=0.22, hmax=0.5, spread=0.2, z=None):
	for k in range(n):
		h = rnd.uniform(hmin, hmax)
		x, y = cx + rnd.uniform(-spread, spread), cy + rnd.uniform(-spread, spread)
		base = heap(x, y) if z is None else z
		w = geo.cone(rnd.uniform(0.04, 0.07), h, (x, y, base + h * 0.5 - 0.03), (rnd.uniform(-16, 16), rnd.uniform(-16, 16), 0),
					 mat=mats[k % len(mats)], segments=4, name="weed")
		geo.tag(w, "sway", "foliage")
		parts.append(w)


def build():
	conc = mat.make("cb_concrete", (0.45, 0.44, 0.41), pattern="concrete", rough=0.93, bump=0.4)
	conc_old = mat.make("cb_concrete_old", (0.38, 0.38, 0.35), pattern="concrete", rough=0.95, bump=0.5)
	brick = mat.make("cb_brick", (0.47, 0.32, 0.25), pattern="stone", rough=0.92, bump=0.6)
	plaster = mat.make("cb_plaster", (0.58, 0.55, 0.48), pattern="plaster", rough=0.93, bump=0.35)
	tile_band = mat.make("cb_tile_band", (0.36, 0.44, 0.44), pattern="stone", rough=0.6, bump=0.3)
	rubble = mat.make("cb_rubble", (0.41, 0.38, 0.34), pattern="stone", rough=0.96, bump=0.75)
	paper = mat.make("cb_wallpaper", (0.80, 0.74, 0.62), pattern="cloth", rough=0.92, bump=0.15, var=0.1, grime=0.2)
	paper_band = mat.make("cb_wallpaper_band", (0.52, 0.56, 0.50), pattern="cloth", rough=0.92, bump=0.15)
	jangpan = mat.make("cb_jangpan", (0.62, 0.47, 0.27), pattern="paper", rough=0.6, bump=0.1, grime=0.5)
	dado = mat.make("cb_dado_tile", (0.60, 0.61, 0.57), pattern="stone", rough=0.5, bump=0.2)
	inner_dark = mat.make("cb_inner_dark", (0.26, 0.26, 0.24), pattern="plaster", rough=0.95, bump=0.2)
	alu = mat.make("cb_alu", (0.52, 0.53, 0.52), pattern="rust", rough=0.55, bump=0.2, amount=0.4, accent="rust")
	glass = mat.get("glass")
	metal = mat.get("metal")
	rust = mat.get("rust")
	door_m = mat.make("cb_door", (0.30, 0.37, 0.42), pattern="rust", rough=0.7, bump=0.35, amount=0.9)
	gas = mat.make("cb_gas_pipe", (0.64, 0.54, 0.25), pattern="rust", rough=0.6, bump=0.2, amount=0.5)
	unit_m = mat.make("cb_unit", (0.58, 0.58, 0.55), pattern="rust", rough=0.6, bump=0.25, amount=0.7)
	dark = mat.make("cb_dark", (0.05, 0.045, 0.04), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	wardrobe_m = mat.make("cb_wardrobe", (0.36, 0.25, 0.18), pattern="wood", rough=0.6, bump=0.25)
	shade_m = mat.make("cb_lamp_shade", (0.72, 0.68, 0.58), pattern="plain", rough=0.5, bump=0.05, grime=0.4)
	cord_m = mat.make("cb_cord", (0.10, 0.10, 0.09), pattern="plain", rough=0.7, bump=0.0, edge=0.0)
	photo_m = mat.make("cb_photo", (0.50, 0.44, 0.36), pattern="paper", rough=0.8, bump=0.05)
	table_m = mat.make("cb_table", (0.44, 0.33, 0.22), pattern="wood", rough=0.7, bump=0.25)
	steel = mat.make("cb_steel", (0.56, 0.57, 0.56), pattern="rust", rough=0.45, bump=0.15, amount=0.35)
	sign_m = mat.make("cb_sign", (0.66, 0.62, 0.52), pattern="plaster", rough=0.8, bump=0.15)
	sign_rim = mat.make("cb_sign_rim", (0.28, 0.40, 0.46), pattern="rust", rough=0.7, bump=0.3, amount=0.7)
	sign_ink = mat.make("cb_sign_ink", (0.56, 0.27, 0.21), rough=0.8, bump=0.05, edge=0.0)
	bark = mat.get("bark")
	vine_m = mat.make("cb_vine", (0.22, 0.25, 0.17), pattern="bark", rough=0.9, bump=0.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	grass = mat.make("cb_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	glow = mat.get("spore_glow")
	rnd = geo.rng(77)

	parts = []
	hard = []

	def add(o):
		parts.append(o)
		hard.append(o)
		return o

	back = Wall((0, HY, 0), (-1, 0, 0), (0, 1, 0), 180)
	left = Wall((-HX, 0, 0), (0, -1, 0), (-1, 0, 0), -90)

	# The standing corner: columns, beams, two slabs with broken edges.
	for (x, y, top, tilt) in ((-4.3, 3.8, 6.6, (0, 0)), (-1.25, 3.8, 6.3, (0, 0)), (-4.3, 0.25, 6.15, (0, 0)), (-1.25, 0.25, 1.3, (-8, 12)),
							  (1.75, 3.8, 3.35, (0, 4)), (4.3, 3.8, 1.9, (6, 12)), (-4.3, -3.8, 2.35, (-4, 0)), (4.3, -3.8, 1.1, (-15, 10))):
		for o in column(x, y, 0.0, top, conc, rust, rnd, tilt):
			add(o) if o.name.startswith("column") else parts.append(o)

	S1 = [(-4.5, 4.0), (-0.75, 4.0), (-0.55, 3.2), (-0.85, 2.5), (-0.45, 1.7), (-0.8, 0.95), (-0.9, 0.25), (-1.45, -0.3),
		  (-2.0, -0.05), (-2.55, -0.55), (-3.25, -0.3), (-3.85, -0.75), (-4.5, -0.45)]
	S2 = [(-4.5, 4.0), (-1.0, 4.0), (-0.8, 3.3), (-1.25, 2.55), (-1.05, 1.6), (-1.55, 0.95), (-2.3, 1.15), (-2.95, 0.55),
		  (-3.6, 0.8), (-3.95, 0.15), (-4.5, 0.35)]
	for (outline, lvl, picks) in ((S1, L1, (1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11)), (S2, L2, (1, 2, 3, 4, 5, 6, 7, 8, 9))):
		add(geo.polygon_slab(outline, SL, loc=(0, 0, lvl - SL), mat=conc, name="slab"))
		parts += edge_rebar(outline, Matrix.Identity(4), picks[::2], rnd, rust, lvl - SL * 0.5)
	for (lvl, x1, y0) in ((L1, -0.8, -0.45), (L2, -1.05, 0.35)):
		z = lvl - SL - BEAM * 0.5
		add(geo.box((x1 + HX, CW, BEAM), ((x1 - HX) * 0.5, HY - CW * 0.5, z), mat=conc, name="beam"))
		add(geo.box((CW, HY - y0, BEAM), (-HX + CW * 0.5, (HY + y0) * 0.5, z), mat=conc, name="beam"))
	add(geo.box((2.65, CW, BEAM), (-2.75, 0.25, L1 - SL - BEAM * 0.5), mat=conc, name="beam"))
	add(geo.box((1.5, CW, BEAM), (-3.35, 0.25, L2 - SL - BEAM * 0.5), mat=conc, name="beam"))
	hang = chunk((1.3, 0.34, 0.3), (0, 0, 0), (0, 0, 0), conc_old, 5, "beam_hanging", 1)
	add(place([hang], Matrix.Translation((-2.0, 0.22, 4.95)) @ rot(0, 38, 0))[0])
	parts.append(rebar((-2.55, 0.22, 5.35), (-1, 0, 0.4), 0.4, rnd, rust))
	parts.append(rebar((-2.55, 0.3, 5.3), (-1, 0, 0.6), 0.45, rnd, rust))

	# Infill: plastered brick outside, papered room upstairs, a tiled restaurant below.
	t = 0.2
	z_lo1, z_hi1 = 0.0, L1 - SL - BEAM
	z_lo2, z_hi2 = L1, L2 - SL - BEAM
	parts_inf = []
	parts_inf += panel(back, 1.45, 4.1, z_lo1, z_hi1, [(1.8, 2.75, 0.0, 2.05), (3.15, 3.8, 1.25, 1.95)], plaster, t, inner_dark, (1.15, dado))
	parts_inf += panel(back, 1.45, 4.1, z_lo2, z_hi2, [(2.25, 3.4, 3.85, 4.95)], plaster, t, paper, (L1 + 0.12, paper_band))
	parts_inf += panel(left, -3.6, -0.45, z_lo1, z_hi1, [(-2.65, -1.35, 0.95, 1.95)], plaster, t, inner_dark, (1.15, dado))
	parts_inf += panel(left, -3.6, -0.45, z_lo2, z_hi2, [(-2.75, -1.75, 3.85, 4.95)], plaster, t, paper, (L1 + 0.12, paper_band))
	for o in parts_inf:
		add(o) if o.name.startswith("infill") else parts.append(o)
	parts += window(back, 3.15, 3.8, 1.25, 1.95, alu, glass, rnd, metal)
	parts += window(back, 2.25, 3.4, 3.85, 4.95, alu, glass, rnd, metal)
	parts += window(left, -2.65, -1.35, 0.95, 1.95, alu, glass, rnd)
	parts += window(left, -2.75, -1.75, 3.85, 4.95, alu, glass, rnd, metal)
	# The restaurant's steel back door, shut.
	parts.append(back.box((0.95, 0.06, 2.05), 2.275, -0.08, 1.025, door_m, "back_door"))
	parts.append(back.box((1.05, 0.12, 0.06), 2.275, -0.08, 2.08, metal, "door_head"))
	parts.append(back.box((0.12, 0.05, 0.04), 1.95, -0.03, 1.0, metal, "door_handle"))
	add(back.box((1.3, 0.4, 0.14), 2.275, 0.2, 0.07, conc_old, "door_step"))
	# A tiled band at each floor line, the old building's one flourish.
	for (w, u0, u1, z) in ((back, 0.75, 4.5, L1), (back, 1.0, 4.5, L2), (left, -4.0, 0.45, L1), (left, -4.0, -0.35, L2)):
		zz = z - SL - BEAM * 0.5
		parts.append(w.quad(u0, u1, zz - 0.17, zz + 0.17, 0.004, tile_band, "tile_band"))
	# Plaster flaked off in patches, the brick showing.
	for (w, u, z, ru, rz_) in ((back, 3.7, 0.55, 0.38, 0.22), (back, 1.85, 5.15, 0.4, 0.18), (left, -3.2, 0.5, 0.4, 0.25), (left, -0.85, 4.5, 0.32, 0.2)):
		pts = []
		for k in range(10):
			a = math.tau * k / 10
			r = rnd.uniform(0.7, 1.0)
			pts.append((u + math.cos(a) * ru * r, z + math.sin(a) * rz_ * r))
		parts.append(w.poly(pts, 0.005, brick, "flake"))

	# Broken brick walls: the back wall beyond the corner, the front of the left side, the third-floor stubs.
	add(back.jag([(-1.55, 0.0), (1.05, 0.0), (1.05, 2.25), (0.7, 2.05), (0.45, 2.3), (0.1, 1.8), (-0.3, 1.55), (-0.6, 1.0), (-0.95, 1.1), (-1.25, 0.6), (-1.55, 0.7)],
				  t, brick))
	add(left.jag([(-0.05, 0.0), (3.6, 0.0), (3.6, 0.4), (3.2, 0.7), (2.7, 0.55), (2.3, 1.1), (1.8, 0.95), (1.3, 1.6), (0.8, 1.45), (0.4, 2.0), (-0.05, 2.25)],
				 t, brick))
	add(back.jag([(1.45, L2), (4.1, L2), (4.1, 6.55), (3.75, 6.4), (3.4, 6.6), (3.0, 6.25), (2.6, 6.3), (2.3, 6.0)], t, brick))
	add(left.jag([(-3.6, L2), (-1.9, L2), (-2.1, 6.05), (-2.5, 6.25), (-2.9, 6.15), (-3.3, 6.45), (-3.6, 6.35)], t, brick))

	# The room: yellow floor paper, a wardrobe, a photo, a low table tipped at the edge, the lamp.
	parts.append(mesh_obj("jangpan", [(-4.1, 0.45, L1 + 0.004), (-1.45, 0.45, L1 + 0.004), (-1.45, 3.6, L1 + 0.004), (-4.1, 3.6, L1 + 0.004)], [(0, 1, 2, 3)], jangpan))
	parts.append(geo.box((1.1, 0.52, 1.85), (-3.45, 3.52, L1 + 0.925), mat=wardrobe_m, name="wardrobe"))
	parts.append(geo.box((0.02, 0.01, 1.6), (-3.45, 3.255, L1 + 0.95), mat=dark, name="wardrobe_seam"))
	parts.append(geo.box((0.04, 0.03, 0.18), (-3.35, 3.245, L1 + 0.95), mat=steel, name="wardrobe_knob"))
	parts.append(geo.box((0.04, 0.03, 0.18), (-3.55, 3.245, L1 + 0.95), mat=steel, name="wardrobe_knob"))
	parts.append(left.box((0.42, 0.03, 0.34), -0.95, -t - 0.015, L1 + 1.65, wardrobe_m, "photo_frame"))
	parts.append(left.quad(-1.12, -0.78, L1 + 1.51, L1 + 1.79, -t - 0.032, photo_m, "photo"))
	# The low table lies upside down, legs in the air.
	tbl = [geo.box((0.85, 0.58, 0.04), (0, 0, 0.02), mat=table_m, name="low_table")]
	for (dx, dy) in ((-0.36, -0.22), (0.36, -0.22), (-0.36, 0.22), (0.36, 0.22)):
		tbl.append(geo.box((0.05, 0.05, 0.26), (dx, dy, 0.17), mat=table_m, name="table_leg"))
	place(tbl, Matrix.Translation((-1.95, 1.15, L1 + 0.005)) @ rot(4, -6, 28))
	parts += tbl
	lx, ly = -2.15, 1.6
	parts.append(geo.cylinder(0.012, 0.45, (lx, ly, L2 - SL - 0.225), mat=cord_m, segments=4, name="lamp_cord"))
	parts.append(geo.lathe([(0.05, 0.0), (0.12, -0.04), (0.24, -0.16), (0.25, -0.2)], (lx, ly, L2 - SL - 0.45), mat=shade_m, segments=10,
							name="lamp_shade", close_top=False, close_bottom=False))
	parts.append(geo.sphere(0.05, (lx, ly, L2 - SL - 0.57), mat=shade_m, segments=6, rings=4, name="bulb"))

	# The restaurant downstairs: a stainless table and a stool still standing in the gloom.
	parts.append(geo.box((0.8, 0.6, 0.04), (-3.3, 2.7, 0.72), mat=steel, name="r_table"))
	parts.append(geo.box((0.06, 0.06, 0.7), (-3.3, 2.7, 0.36), mat=steel, name="r_table_leg"))
	parts.append(geo.cylinder(0.17, 0.45, (-2.6, 2.4, 0.225), mat=steel, segments=8, name="stool"))

	# The back: an air-conditioner on its bracket, the yellow gas line and meter, a downpipe.
	parts.append(back.box((0.85, 0.32, 0.6), 1.85, 0.36, 3.55, unit_m, "ac_unit"))
	parts.append(geo.cylinder(0.2, 0.02, back.at(1.75, 0.53, 3.55), (90, 0, 0), mat=dark, segments=10, name="fan"))
	for a in (45, 135):
		parts.append(geo.box((0.42, 0.02, 0.02), back.at(1.75, 0.545, 3.55), (0, a, 0), mat=unit_m, name="grille"))
	for uu in (1.55, 2.15):
		parts.append(geo.tube([back.at(uu, 0.0, 3.6), back.at(uu, 0.45, 3.25), back.at(uu, 0.45, 3.22)], 0.02, mat=metal, segments=3, name="bracket"))
	parts.append(back.quad(1.75, 1.85, 2.0, 3.25, 0.004, rust, "rust_run", 1.72, 1.9))
	parts.append(geo.tube([back.at(1.15, 0.06, 2.45), back.at(3.9, 0.06, 2.45), back.at(3.9, 0.06, 1.75)], 0.03, mat=gas, segments=5, name="gas_pipe"))
	parts.append(back.box((0.36, 0.2, 0.32), 3.9, 0.12, 1.55, unit_m, "gas_meter"))
	parts.append(geo.tube([back.at(1.3, 0.15, L2 + 0.1), back.at(1.3, 0.15, 0.25), back.at(1.3, 0.3, 0.1)], 0.05, mat=mat.make("cb_pvc", (0.52, 0.52, 0.49), rough=0.6, bump=0.1),
						  segments=6, name="downpipe"))
	for z in (1.0, 2.8, 4.6):
		parts.append(back.box((0.14, 0.12, 0.04), 1.3, 0.08, z, metal, "clamp"))
	# An empty sign frame still bolted out from the left wall, its panel long gone.
	sx, sy = -HX - 0.75, 1.2
	frame = [(-HX, sy, 4.35), (sx, sy, 4.35), (sx, sy, 2.95), (-HX, sy, 2.95)]
	parts.append(geo.tube(frame, 0.035, mat=rust, segments=4, name="sign_frame"))
	parts.append(geo.tube([(sx, sy, 3.65), (-HX, sy, 3.65)], 0.025, mat=rust, segments=4, name="sign_frame"))
	parts.append(left.quad(-1.25, -1.15, 1.6, 2.95, 0.004, rust, "rust_run", -1.25, -1.1))

	# The fallen part: a rubble heap, slabs lying on it, a broken column, chunks and bricks.
	nx, ny = 16, 15
	vs = []
	for j in range(ny + 1):
		y = GY0 + (GY1 - GY0) * j / ny
		for i in range(nx + 1):
			x = GX0 + (GX1 - GX0) * i / nx
			vs.append((x, y, heap(x, y)))
	faces = []
	for j in range(ny):
		for i in range(nx):
			a = j * (nx + 1) + i
			quad = (a, a + 1, a + nx + 2, a + nx + 1)
			if max(vs[q][2] for q in quad) > 0.01 - SINK:
				faces.append(quad)
	heap_obj = mesh_obj("heap", vs, faces, rubble, True)
	bm = bmesh.new()
	bm.from_mesh(heap_obj.data)
	bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
	bm.to_mesh(heap_obj.data)
	bm.free()
	add(heap_obj)

	fallen = [
		([(-2.1, -1.4), (-0.4, -1.55), (0.9, -1.3), (2.0, -1.45), (2.15, -0.2), (1.9, 0.9), (2.05, 1.4), (0.6, 1.3), (-0.8, 1.5), (-2.0, 1.25), (-2.2, 0.1)],
		 (1.5, 1.9, 1.4, 0.06, 7, -9, 6)),
		([(-1.5, -0.9), (0.2, -1.1), (1.5, -0.8), (1.6, 0.2), (1.4, 0.95), (-0.2, 1.05), (-1.6, 0.8)], (0.3, -1.25, 1.0, 0.0, -14, 7, -12)),
		([(-0.9, -1.3), (0.9, -1.2), (1.0, 0.1), (0.8, 1.3), (-0.9, 1.25), (-1.05, 0.0)], (3.75, 0.3, 0.8, 0.02, 4, 24, 16)),
		([(-1.5, -0.7), (0.0, -0.8), (1.5, -0.65), (1.4, 0.75), (-1.4, 0.7)], (2.7, 3.2, 0.7, -0.22, -8, 10, -4)),
		([(-0.8, -0.55), (0.85, -0.6), (0.7, 0.6), (-0.75, 0.5)], (3.35, -2.8, 0.5, 0.5, 64, 0, 33)),
	]
	for k, (outline, (cx, cy, span, lift, ex, ey, rz)) in enumerate(fallen):
		mw = on_heap(cx, cy, span, lift, ex, ey, rz)
		s = geo.polygon_slab(outline, SL, loc=(0, 0, -SL * 0.5), mat=conc if k % 2 == 0 else conc_old, name="fallen_slab")
		add(place([s], mw)[0])
		parts += edge_rebar(outline, mw, range(k % 2, len(outline), 3), rnd, rust, 0.0)
	# The left bay's front slab, torn from the standing floor and hanging down onto the heap.
	hang_outline = [(-1.2, -1.2), (0.1, -1.3), (1.25, -1.1), (1.15, 0.2), (1.3, 1.2), (0.3, 1.25), (-0.6, 1.15), (-1.25, 1.2), (-1.1, 0.0)]
	mw = Matrix.Translation((-2.75, -1.3, 1.72)) @ rot(40, 0, 3)
	add(place([geo.polygon_slab(hang_outline, SL, loc=(0, 0, -SL * 0.5), mat=conc, name="hanging_slab")], mw)[0])
	parts += edge_rebar(hang_outline, mw, (0, 2, 3, 6), rnd, rust, 0.0)
	for x in (-3.6, -3.0, -2.3, -1.8):
		y0 = -0.4 if x > -3.5 else -0.6
		parts.append(geo.tube([(x, y0, L1 - 0.12), (x + 0.05, y0 - 0.25, L1 - 0.3), (x, -0.35 - 0.5, 2.45)], 0.016, mat=rust, segments=3, name="rebar"))
	# A column snapped off and lying across the heap.
	col = geo.box((0.4, 0.4, 2.3), (0, 0, 0), mat=conc_old, name="fallen_column")
	cm = on_heap(1.9, -2.2, 0.8, 0.22, 0, 84, 52)
	add(place([col], cm)[0])
	for s_ in (-1.0, 1.0):
		for k in range(2):
			p = cm @ Vector(((k - 0.5) * 0.2, 0.1, s_ * 1.1))
			parts.append(rebar(p, cm.to_3x3() @ Vector((0, 0, s_)), 0.4, rnd, rust))

	for k in range(24):
		x = rnd.uniform(-3.8, 4.4)
		y = rnd.uniform(-4.3, 3.6)
		if x < -0.9 and y > 0.5:
			x += 3.0
		s = rnd.uniform(0.2, 0.55)
		m = (conc, brick, conc_old, rubble)[k % 4]
		c = chunk((s, s * rnd.uniform(0.6, 1.0), s * rnd.uniform(0.4, 0.7)), (x, y, max(heap(x, y), 0.0) + s * 0.15),
				  (rnd.uniform(-20, 20), rnd.uniform(-20, 20), rnd.uniform(0, 180)), m, 30 + k, "chunk", 1 if s > 0.45 else 0)
		add(c)
	for k in range(14):
		x = rnd.uniform(-3.6, 4.6)
		y = rnd.uniform(-4.7, 3.0)
		if x < -0.9 and y > 0.5:
			y -= 3.0
		parts.append(geo.box((0.22, 0.1, 0.07), (x, y, heap(x, y) + 0.03), (rnd.uniform(-25, 25), rnd.uniform(-25, 25), rnd.uniform(0, 180)), mat=brick, name="brick"))

	# The restaurant's sign, face-up on the front slope, part buried.
	sm = on_heap(-0.2, -3.0, 1.2, 0.1, 4, -6, 35)
	sign = [geo.box((2.7, 0.78, 0.1), (0, 0, 0), mat=sign_rim, name="sign_box"),
			mesh_obj("sign_face", [(-1.28, -0.33, 0.052), (1.28, -0.33, 0.052), (1.28, 0.33, 0.052), (-1.28, 0.33, 0.052)], [(0, 1, 2, 3)], sign_m),
			flat_text("고향식당", 0.46, (0.0, 0.0, 0.056), (0, 0, 0), sign_ink, 32.0),
			mesh_obj("sign_stripe", [(-1.28, -0.31, 0.055), (1.28, -0.31, 0.055), (1.28, -0.25, 0.055), (-1.28, -0.25, 0.055)], [(0, 1, 2, 3)], sign_rim)]
	place(sign, sm)
	parts += sign
	for (dx, dy, s) in ((1.25, 0.25, 0.42), (-1.2, -0.3, 0.3)):
		p = sm @ Vector((dx, dy, 0.1))
		add(chunk((s, s * 0.8, s * 0.6), tuple(p), (rnd.uniform(-20, 20), 0, rnd.uniform(0, 90)), conc_old, int(dx * 10) + 50, "chunk", 1))

	# The tree grown out of the heap.
	tx, ty = 3.0, -1.0
	tz = heap(tx, ty) - 0.15
	trunk = [(tx, ty, tz), (tx - 0.1, ty + 0.05, tz + 1.0), (tx - 0.05, ty + 0.15, tz + 2.0), (tx + 0.15, ty + 0.1, tz + 2.9)]
	parts.append(geo.tube(trunk, [0.16, 0.12, 0.09, 0.06], mat=bark, segments=6, name="trunk"))
	parts.append(geo.tube([(tx - 0.06, ty + 0.1, tz + 1.7), (tx - 0.6, ty + 0.3, tz + 2.4), (tx - 0.85, ty + 0.35, tz + 2.8)], [0.06, 0.04, 0.025],
						  mat=bark, segments=5, name="branch"))
	parts.append(geo.tube([(tx, ty + 0.12, tz + 2.2), (tx + 0.55, ty - 0.2, tz + 2.75)], [0.05, 0.03], mat=bark, segments=5, name="branch"))
	for k, (dx, dy, dz, r) in enumerate(((0.1, 0.1, 3.25, 0.85), (-0.75, 0.35, 2.95, 0.65), (0.6, -0.25, 2.9, 0.6), (-0.2, -0.3, 3.55, 0.55),
										 (0.15, 0.55, 3.0, 0.55))):
		b = geo.blob((r, r * 0.95, r * 0.75), (tx + dx, ty + dy, tz + dz), mat=leaf if k % 2 == 0 else leaf_dark, subdivisions=2, lumpy=0.35,
					 seed=k + 70, name="crown")
		geo.tag(b, "sway", "foliage")
		parts.append(b)
	for (dx, dy) in ((0.2, -0.15), (-0.2, 0.15)):
		parts.append(geo.tube([(tx + dx * 0.3, ty + dy * 0.3, tz + 0.25), (tx + dx * 1.5, ty + dy * 1.5, tz + 0.1), (tx + dx * 2.6, ty + dy * 2.4, tz - 0.05)],
							  [0.06, 0.04, 0.02], mat=bark, segments=4, name="root"))

	# Nature: moss on the slab tops and ledges, vines hanging from the broken edges, weeds on the heap.
	pads = [((-3.3, 2.6, L2), (0.9, 0.7, 0.1), moss), ((-1.8, 3.4, L2), (0.6, 0.4, 0.08), moss_dark), ((-3.9, 1.0, L2), (0.45, 0.4, 0.08), moss),
			((-2.6, -0.2, L1), (0.6, 0.3, 0.08), moss), ((-3.9, -0.35, L1), (0.35, 0.3, 0.07), moss_dark), ((-0.75, 1.8, L1), (0.25, 0.6, 0.07), moss),
			((-4.3, 3.8, 6.5), (0.24, 0.24, 0.08), moss_dark), ((-1.25, 3.8, 6.2), (0.22, 0.22, 0.07), moss)]
	for k, ((x, y, z), r, m) in enumerate(pads):
		p = moss_pad(r, x, y, z - 0.02, (0, 0, 0), m, k + 3)
		if k in (0, 3):
			geo.tag(p, "pulse")
		parts.append(p)
	for k, (outline, (cx, cy, span, lift, ex, ey, rz)) in enumerate(fallen[:4]):
		mw = on_heap(cx, cy, span, lift, ex, ey, rz)
		p = moss_pad((0.75, 0.55, 0.09), 0, 0, SL * 0.5 - 0.02, (0, 0, 0), moss if k % 2 else moss_dark, 20 + k)
		place([p], mw @ Matrix.Translation((0.3 * (k - 1.5), 0.2, 0)))
		parts.append(p)
	p = moss_pad((0.6, 0.5, 0.08), 0, 0, SL * 0.5 - 0.02, (0, 0, 0), moss, 29)
	place([p], Matrix.Translation((-2.75, -1.3, 1.72)) @ rot(40, 0, 3) @ Matrix.Translation((0.3, 0.2, 0)))
	parts.append(p)
	for (x, y, rx, ry) in ((0.8, -3.6, 1.0, 0.5), (-2.4, -3.4, 0.8, 0.6), (4.0, 1.8, 0.5, 0.8), (2.2, 3.6, 0.8, 0.4)):
		parts.append(moss_pad((rx, ry, 0.1), x, y, max(heap(x, y), 0.0) - 0.03, (0, 0, 0), moss_dark if x > 0 else moss, int(x * 10) + 40))

	leaves = (leaf, leaf_dark)
	for (x, y, drop) in ((-1.3, 1.25, 1.9), (-2.75, 0.75, 1.4), (-3.3, 0.72, 2.2), (-1.05, 2.0, 1.2)):
		z0 = L2 - SL * 0.5
		pts = [(x, y, z0 + 0.1), (x + 0.04, y - 0.18, z0 - 0.15)]
		for k in range(1, 4):
			pts.append((x + 0.05 * math.sin(k * 1.7), y - 0.22 - 0.03 * k, z0 - 0.15 - drop * k / 3.0))
		vine(parts, pts, [0.025, 0.022, 0.018, 0.014, 0.01], (0, -1, 0), vine_m, leaves, rnd, 2, 0.12)
	for (u, drop) in ((1.2, 3.6), (3.1, 2.5), (3.85, 4.4)):
		x = -u
		pts = [(x, HY - 0.1, L2 + 0.25), (x, HY + 0.08, L2 + 0.05)]
		for k in range(1, 4):
			pts.append((x + 0.15 * math.sin(k * 2.1), HY + 0.04, L2 - drop * k / 3.0))
		vine(parts, pts, [0.03, 0.026, 0.022, 0.016, 0.012], (0, 1, 0), vine_m, leaves, rnd, 3, 0.14)
	for (y, top) in ((2.6, 5.2), (0.9, 3.4)):
		pts = [(-HX - 0.03, y + 0.1 * math.sin(k * 1.9), top * k / 4.0 + 0.05) for k in range(5)]
		vine(parts, pts, [0.03, 0.026, 0.022, 0.016, 0.012], (-1, 0, 0), vine_m, leaves, rnd, 3, 0.14)
	for (x, y, z, r) in ((-1.4, 1.2, L2 + 0.02, 0.28), (-3.4, 0.75, L2 + 0.03, 0.3), (-1.9, HY + 0.05, L2 + 0.1, 0.3), (-3.9, HY + 0.05, 6.5, 0.28),
						 (-HX - 0.05, 2.6, 5.1, 0.26)):
		c = geo.blob((r, r, r * 0.8), (x, y, z), mat=leaf, subdivisions=1, lumpy=0.3, seed=int(abs(x) * 10 + z), name="vine_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	for (cx, cy, n) in ((-0.2, -3.9, 4), (1.8, -3.4, 3), (3.6, -1.6, 4), (4.2, 2.4, 3), (0.2, 0.4, 3), (-2.0, -2.6, 3), (2.4, 1.5, 3),
						(-3.6, -3.4, 3), (0.9, 3.4, 3), (-2.8, 2.6, 2), (3.0, 3.8, 3), (-4.6, 3.2, 3), (-4.7, -1.2, 3)):
		weeds(parts, cx, cy, n, rnd, (grass, leaf, leaf_dark))
	for loc in ((-2.6, 2.0, L1 + 0.5), (2.9, -0.8, 3.0)):
		g = geo.blob(0.05, loc, mat=glow, subdivisions=1, name="spore")
		geo.tag(g, "pulse")
		parts.append(g)

	for i, p in enumerate(hard):
		geo.moss_mask(p, seed=i + 5)

	res = pipeline.static("collapsed_building", parts, size=1024)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
