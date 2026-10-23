# A one-storey corner convenience store, "24시 편의마트": a glass front in aluminium frames wrapping the left
# corner, a few panes shattered onto the dark aisle and a toppled shelf, a faded sign band with one panel
# sagging, a torn striped awning, a plastic table and chairs, an ice-cream freezer, a dead drink machine and
# crates. Behind: a steel back door under a hood, refrigeration units and their pipes, a bin and rubbish bags.
# A flat roof in faded green waterproof paint with a squat water tank, moss on the parapets and the deck, grass,
# ivy up two corners.
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from gse import core, geo, mat, pipeline

HX, HY = 4.0, 3.0
T = 0.22
TOP = 3.75
DECK = 3.22
PLINTH = 0.12
GLASS_LO, TRANSOM, GLASS_HI = 0.45, 2.15, 2.55
BAND_LO, BAND_HI = 2.62, 3.58
BAND_Y = -HY - 0.12


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


def flat_text(body, size, loc, rot, m, angle=28.0):
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


def drop_faces(obj, fn):
	# Faces nobody sees (on the ground, under a coping) would only waste texture space.
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


def pivot(objs, point, axis, angle):
	# Swings a group of parts about a hinge line through point.
	m = Matrix.Translation(Vector(point)) @ Matrix.Rotation(math.radians(angle), 4, axis) @ Matrix.Translation(-Vector(point))
	for o in objs:
		geo.apply_modifiers(o)
		o.matrix_basis = m @ o.matrix_basis
		geo.apply_transform(o)
	return objs


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
		return geo.box(size, self.at(u, n, z), (0, 0, self.rz), mat=m, name=name, bevel=bevel, bevel_segments=1)

	def quad(self, u0, u1, z0, z1, n, m, name="quad", u0_top=None, u1_top=None):
		u0t = u0 if u0_top is None else u0_top
		u1t = u1 if u1_top is None else u1_top
		vs = [self.at(u0, n, z0), self.at(u1, n, z0), self.at(u1t, n, z1), self.at(u0t, n, z1)]
		return mesh_obj(name, vs, [(0, 1, 2, 3)], m)

	def tri(self, a, b, c, n, m, name="shard"):
		return mesh_obj(name, [self.at(a[0], n, a[1]), self.at(b[0], n, b[1]), self.at(c[0], n, c[1])], [(0, 1, 2)], m)

	def text(self, body, size, u, n, z, m, angle=28.0):
		return flat_text(body, size, self.at(u, n, z), (90, 0, self.rz), m, angle)


def shards(w, u0, u1, z0, z1, n, m, rnd):
	# What is left of a shattered pane: jagged teeth along the frame.
	du, dz = u1 - u0, z1 - z0
	j = lambda: rnd.uniform(-0.06, 0.06)
	out = []
	out.append(w.tri((u0, z0), (u0 + du * (0.42 + j()), z0), (u0, z0 + dz * (0.32 + j())), n, m))
	out.append(w.tri((u0 + du * (0.55 + j()), z0), (u1, z0), (u1, z0 + dz * (0.18 + j())), n, m))
	out.append(w.tri((u0 + du * (0.15 + j()), z1), (u0 + du * (0.70 + j()), z1), (u0 + du * (0.38 + j()), z1 - dz * (0.30 + j())), n, m))
	out.append(w.tri((u1, z0 + dz * (0.40 + j())), (u1, z1), (u1 - du * (0.25 + j()), z0 + dz * (0.72 + j())), n, m))
	out.append(w.tri((u0, z0 + dz * (0.55 + j())), (u0, z1), (u0 + du * (0.22 + j()), z1 - dz * (0.10 + j())), n, m))
	return out


def weeds(parts, cx, cy, n, rnd, mats, hmin=0.2, hmax=0.45, spread=0.14, z=0.0):
	for k in range(n):
		h = rnd.uniform(hmin, hmax)
		w = geo.cone(rnd.uniform(0.04, 0.07), h, (cx + rnd.uniform(-spread, spread), cy + rnd.uniform(-spread, spread), z + h * 0.5),
					 (rnd.uniform(-16, 16), rnd.uniform(-16, 16), 0), mat=mats[k % len(mats)], segments=4, name="weed")
		geo.tag(w, "sway", "foliage")
		parts.append(w)


def leaf_card(c, size, out, roll, m):
	# A cheap leaf pressed to a wall: a diamond with a raised middle, facing along out.
	w, h = size * 0.8, size
	vs = [(0, -0.35 * w, 0.1 * h), (-w, 0, 0), (0, 0, h), (w, 0, 0), (0, 0, -0.6 * h)]
	obj = mesh_obj("ivy_leaf", vs, [(0, 2, 1), (0, 3, 2), (0, 4, 3), (0, 1, 4)], m, True)
	rz = {(-1, 0): -90, (1, 0): 90, (0, 1): 180, (0, -1): 0}[(int(round(out[0])), int(round(out[1])))]
	obj.location = Vector(c)
	obj.rotation_euler = (0.0, math.radians(roll), math.radians(rz))
	geo.apply_transform(obj)
	geo.tag(obj, "sway", "foliage")
	return obj


def ivy(parts, path, rads, out, vine_m, leaves, rnd, per=2, size=0.13):
	# A vine stem flat on a wall, its leaves pressed against the face (out = the wall's outward axis).
	parts.append(geo.tube(path, rads, mat=vine_m, segments=4, name="ivy"))
	side = Vector((-out[1], out[0], 0.0))
	for k in range(1, len(path)):
		p0, p1 = Vector(path[k - 1]), Vector(path[k])
		for j in range(per):
			c = p0.lerp(p1, (j + 0.5) / per) + Vector(out) * 0.025 + side * rnd.uniform(-0.12, 0.12)
			c.z += rnd.uniform(-0.05, 0.05)
			parts.append(leaf_card(tuple(c), size * rnd.uniform(0.75, 1.2), out, rnd.uniform(-50, 50), leaves[(k + j) % len(leaves)]))


def moss_pad(radii, x, y, z, rot, m, seed, segments=8):
	# A soft cushion: domed, edges rolled under, lumped by low-frequency noise.
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], (x, y, z), rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.22, 1.6, seed=seed)


def flat_rect(w, d, loc, rz, m, name="rect"):
	c, s_ = math.cos(math.radians(rz)), math.sin(math.radians(rz))
	vs = [(loc[0] + c * x - s_ * y, loc[1] + s_ * x + c * y, loc[2]) for (x, y) in ((-w / 2, -d / 2), (w / 2, -d / 2), (w / 2, d / 2), (-w / 2, d / 2))]
	return mesh_obj(name, vs, [(0, 1, 2, 3)], m)


def stain_patch(x, y, z, rx, ry, m, rnd, name="wet"):
	# A flat irregular patch lying on a level surface: damp, soot, a dried puddle.
	vs = []
	n = 13
	for k in range(n):
		a = math.tau * k / n
		r = rnd.uniform(0.5, 1.0) if k % 2 else rnd.uniform(0.85, 1.05)
		vs.append((x + math.cos(a) * rx * r, y + math.sin(a) * ry * r, z))
	return mesh_obj(name, vs, [tuple(range(n))], m)


def build():
	wall_m = mat.make("store_wall", (0.57, 0.55, 0.49), pattern="plaster", rough=0.92, bump=0.3)
	base_m = mat.make("store_base", (0.37, 0.37, 0.35), pattern="concrete", rough=0.95, bump=0.4)
	deck_m = mat.make("store_deck", (0.33, 0.43, 0.40), pattern="plaster", rough=0.9, bump=0.3, var=0.16)
	bare = mat.make("store_deck_bare", (0.40, 0.40, 0.38), pattern="concrete", rough=0.95, bump=0.45)
	cope_m = mat.make("store_coping", (0.46, 0.46, 0.43), pattern="concrete", rough=0.9, bump=0.35)
	stain = mat.make("store_stain", (0.40, 0.39, 0.34), pattern="plaster", rough=0.95, bump=0.2)
	alu = mat.make("store_alu", (0.55, 0.56, 0.55), pattern="rust", rough=0.55, bump=0.2, amount=0.3, accent="rust")
	kick = mat.make("store_kick", (0.38, 0.39, 0.39), pattern="rust", rough=0.6, bump=0.2, amount=0.4)
	glass = mat.get("glass")
	sheen = mat.make("store_sheen", (0.21, 0.27, 0.29), pattern="plain", rough=0.2, grime=0.05, edge=0.0, bump=0.0)
	dark = mat.make("store_dark", (0.05, 0.045, 0.04), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	floor_m = mat.make("store_floor", (0.16, 0.15, 0.14), pattern="concrete", rough=0.9, grime=0.2, edge=0.0, bump=0.2)
	sign_m = mat.make("store_sign", (0.66, 0.63, 0.53), pattern="plaster", rough=0.8, bump=0.15)
	peel = mat.make("store_sign_peel", (0.47, 0.47, 0.45), pattern="rust", rough=0.8, bump=0.2, amount=0.6)
	teal = mat.make("store_teal", (0.24, 0.42, 0.40), pattern="rust", rough=0.7, bump=0.2, amount=0.5)
	orange = mat.make("store_orange", (0.60, 0.35, 0.22), pattern="rust", rough=0.7, bump=0.2, amount=0.5)
	letter = mat.make("store_letter", (0.16, 0.29, 0.30), rough=0.7, bump=0.05, edge=0.0)
	tube_m = mat.make("store_tube", (0.70, 0.70, 0.66), rough=0.4, bump=0.0, edge=0.0)
	awning_a = mat.make("store_awning_cream", (0.66, 0.62, 0.52), pattern="cloth", rough=0.9, bump=0.35)
	awning_b = mat.make("store_awning_red", (0.52, 0.27, 0.22), pattern="cloth", rough=0.9, bump=0.35)
	poster = mat.make("store_poster", (0.62, 0.50, 0.40), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	poster_ink = mat.make("store_poster_ink", (0.50, 0.22, 0.18), rough=0.9, bump=0.0, edge=0.0)
	plastic_w = mat.make("store_plastic_white", (0.68, 0.67, 0.60), pattern="plain", rough=0.6, bump=0.1, grime=0.45)
	plastic_g = mat.make("store_plastic_green", (0.33, 0.45, 0.37), pattern="plain", rough=0.6, bump=0.1, grime=0.45)
	freezer_m = mat.make("store_freezer", (0.70, 0.70, 0.66), pattern="rust", rough=0.55, bump=0.2, amount=0.35)
	freezer_band = mat.make("store_freezer_band", (0.32, 0.44, 0.54), pattern="plain", rough=0.6, bump=0.05)
	vend_m = mat.make("store_vending", (0.25, 0.37, 0.45), pattern="rust", rough=0.6, bump=0.3, amount=0.6)
	vend_top = mat.make("store_vending_top", (0.66, 0.65, 0.58), pattern="rust", rough=0.6, bump=0.2, amount=0.4)
	cans = [mat.make("store_can_" + str(i), c, rough=0.5, bump=0.05, edge=0.1) for i, c in enumerate(
		((0.58, 0.28, 0.24), (0.66, 0.60, 0.36), (0.30, 0.46, 0.36), (0.62, 0.62, 0.58), (0.36, 0.42, 0.56)))]
	crate_y = mat.make("store_crate_yellow", (0.62, 0.52, 0.26), pattern="plain", rough=0.65, bump=0.15, grime=0.5)
	crate_b = mat.make("store_crate_blue", (0.30, 0.39, 0.47), pattern="plain", rough=0.65, bump=0.15, grime=0.5)
	door_m = mat.make("store_door", (0.33, 0.40, 0.38), pattern="rust", rough=0.7, bump=0.35, amount=0.85)
	unit_m = mat.make("store_unit", (0.58, 0.58, 0.55), pattern="rust", rough=0.6, bump=0.25, amount=0.6)
	pipe_m = mat.make("store_pipe_lag", (0.14, 0.14, 0.13), pattern="plain", rough=0.8, bump=0.1)
	pvc = mat.make("store_pvc", (0.55, 0.55, 0.52), pattern="plain", rough=0.6, bump=0.1, grime=0.5)
	bin_m = mat.make("store_bin", (0.29, 0.36, 0.31), pattern="plain", rough=0.6, bump=0.15, grime=0.5)
	bag_w = mat.make("store_bag_white", (0.66, 0.64, 0.56), pattern="cloth", rough=0.5, bump=0.3)
	bag_k = mat.make("store_bag_black", (0.13, 0.13, 0.12), pattern="cloth", rough=0.45, bump=0.3)
	shelf_m = mat.make("store_shelf", (0.48, 0.48, 0.45), pattern="rust", rough=0.6, bump=0.2, amount=0.5)
	counter_m = mat.make("store_counter", (0.34, 0.30, 0.26), pattern="wood", rough=0.8, bump=0.3)
	metal = mat.get("metal")
	rust = mat.get("rust")
	vine_m = mat.make("store_vine", (0.22, 0.25, 0.17), pattern="bark", rough=0.9, bump=0.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	grass = mat.make("store_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	glow = mat.get("spore_glow")
	tank_m = mat.make("store_tank", (0.40, 0.49, 0.52), pattern="plaster", rough=0.7, bump=0.2, grime=0.5)
	wet = mat.make("store_wet", (0.22, 0.28, 0.26), pattern="concrete", rough=0.95, grime=0.0, edge=0.0, bump=0.2)
	rnd = geo.rng(41)

	parts = []
	shell = []

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			shell.append(o)
		return o

	front = Wall((0, -HY, 0), (1, 0, 0), (0, -1, 0), 0)
	back = Wall((0, HY, 0), (-1, 0, 0), (0, 1, 0), 180)
	left = Wall((-HX, 0, 0), (0, -1, 0), (-1, 0, 0), -90)
	right = Wall((HX, 0, 0), (0, 1, 0), (1, 0, 0), 90)

	# Shell: plinth, walls rising into parapets, copings, roof deck, front apron.
	bottom = lambda n: n.z < -0.9
	ends = lambda n: abs(n.z) > 0.9
	add(drop_faces(geo.box((2 * HX, 2 * HY, PLINTH), (0, 0, PLINTH * 0.5), mat=base_m, name="plinth"), bottom))
	add(drop_faces(geo.box((T, 2 * HY, TOP - PLINTH), (HX - T * 0.5, 0, (TOP + PLINTH) * 0.5), mat=wall_m, name="wall_r"), ends))
	add(drop_faces(geo.box((T, HY + 1.2, TOP - PLINTH), (-HX + T * 0.5, (HY - 1.2) * 0.5, (TOP + PLINTH) * 0.5), mat=wall_m, name="wall_l"), ends))
	add(drop_faces(geo.box((2 * HX - 2 * T, T, TOP - PLINTH), (0, HY - T * 0.5, (TOP + PLINTH) * 0.5), mat=wall_m, name="wall_b"), ends))
	add(drop_faces(geo.box((2 * HX - T, T, TOP - GLASS_HI), (-T * 0.5, -HY + T * 0.5, (TOP + GLASS_HI) * 0.5), mat=wall_m, name="wall_f"), ends))
	add(drop_faces(geo.box((T, HY - 1.2 - T, TOP - GLASS_HI), (-HX + T * 0.5, (-HY + T - 1.2) * 0.5, (TOP + GLASS_HI) * 0.5), mat=wall_m, name="wall_lf"), ends))
	add(drop_faces(geo.box((T, T, GLASS_HI - PLINTH), (-HX + T * 0.5, -HY + T * 0.5, (GLASS_HI + PLINTH) * 0.5), mat=wall_m, name="corner_post"), ends))
	cw = T + 0.08
	for (size, loc) in (((cw, 2 * HY + 0.08, 0.07), (HX - T * 0.5, 0, TOP + 0.035)), ((cw, 2 * HY + 0.08, 0.07), (-HX + T * 0.5, 0, TOP + 0.035)),
						((2 * HX - 2 * T, cw, 0.07), (0, HY - T * 0.5, TOP + 0.035)), ((2 * HX - 2 * T, cw, 0.07), (0, -HY + T * 0.5, TOP + 0.035))):
		add(drop_faces(geo.box(size, loc, mat=cope_m, bevel=0.02, bevel_segments=1, name="coping"), bottom))
	add(geo.box((2 * HX - 2 * T, 2 * HY - 2 * T, 0.12), (0, 0, DECK - 0.06), mat=deck_m, name="deck"))
	apron = add(drop_faces(geo.box((2 * HX, 0.72, 0.08), (0, -HY - 0.36, 0.04), mat=base_m, bevel=0.015, bevel_segments=1, name="apron"), bottom))
	for (x, ang, ln) in ((-1.2, 30, 0.6), (1.6, -20, 0.7), (3.4, 50, 0.4)):
		parts.append(geo.box((0.02, ln, 0.01), (x, -HY - 0.36, 0.081), (0, 0, ang), mat=mat.get("stone_dark"), name="crack"))
	add(geo.box((0.5, 0.36, 0.07), (-HX + 0.15, -HY - 0.55, 0.02), (3, -5, 14), mat=base_m, name="apron_chunk"))

	# Inside: dark linings, a floor, the counter by the door, a standing shelf and the toppled one.
	parts.append(geo.box((2 * HX - 2 * T, 2 * HY - 2 * T, 0.004), (0, 0, PLINTH + 0.002), mat=floor_m, name="floor"))
	ix, iy = HX - T - 0.005, HY - T - 0.005
	parts.append(mesh_obj("lining", [(-ix, iy, PLINTH), (ix, iy, PLINTH), (ix, iy, DECK - 0.12), (-ix, iy, DECK - 0.12)], [(0, 1, 2, 3)], dark))
	parts.append(mesh_obj("lining", [(-ix, -iy, PLINTH), (-ix, iy, PLINTH), (-ix, iy, DECK - 0.12), (-ix, -iy, DECK - 0.12)], [(0, 1, 2, 3)], dark))
	parts.append(mesh_obj("lining", [(ix, -iy, PLINTH), (ix, iy, PLINTH), (ix, iy, DECK - 0.12), (ix, -iy, DECK - 0.12)], [(0, 1, 2, 3)], dark))
	parts.append(mesh_obj("lining", [(-ix, -iy, DECK - 0.121), (ix, -iy, DECK - 0.121), (ix, iy, DECK - 0.121), (-ix, iy, DECK - 0.121)], [(0, 1, 2, 3)], dark))
	parts.append(mesh_obj("lining", [(-ix, -iy, GLASS_HI), (ix, -iy, GLASS_HI), (ix, -iy, DECK - 0.12), (-ix, -iy, DECK - 0.12)], [(0, 1, 2, 3)], dark))
	parts.append(geo.box((0.6, 1.5, 0.95), (-3.2, -1.7, PLINTH + 0.475), mat=counter_m, name="counter"))
	parts.append(geo.box((0.4, 0.3, 0.25), (-3.2, -2.0, PLINTH + 1.07), (0, 0, 15), mat=metal, name="register"))
	for x in (0.4, 2.6):
		parts.append(geo.box((1.6, 0.5, 1.5), (x, 0.2, PLINTH + 0.75), mat=dark, name="shelf_far"))
	rack = []
	rx, ry = 1.45, -1.55
	rack.append(geo.box((1.3, 0.5, 0.10), (rx, ry, PLINTH + 0.05), mat=shelf_m, name="rack_base"))
	rack.append(geo.box((1.3, 0.05, 1.50), (rx, ry + 0.22, PLINTH + 0.75), mat=shelf_m, name="rack_back"))
	for z in (0.5, 0.95, 1.4):
		rack.append(geo.box((1.3, 0.42, 0.03), (rx, ry, PLINTH + z), mat=shelf_m, name="rack_shelf"))
	for k in range(4):
		rack.append(geo.box((0.18, 0.14, 0.2), (rx - 0.45 + k * 0.3, ry - 0.05, PLINTH + 1.06), mat=cans[k], name="stock"))
	pivot(rack, (rx, ry - 0.25, PLINTH), 'X', 60.0)
	parts += rack
	for k in range(4):
		parts.append(geo.box((0.16, 0.12, 0.2), (rx + rnd.uniform(-0.8, 0.8), rnd.uniform(-2.8, -2.2), PLINTH + 0.06),
							 (rnd.uniform(0, 90), 0, rnd.uniform(0, 180)), mat=cans[k % 5], name="spilled"))

	# The glass front: five bays, the second the double door; the left wall wraps round in two more.
	FU = [-HX + T, -2.40, -0.86, 0.69, 2.23, HX - T]
	gn = -0.06
	for k in range(len(FU) - 1):
		u0, u1 = FU[k], FU[k + 1]
		cu = (u0 + u1) * 0.5
		w = u1 - u0
		parts.append(front.box((w, 0.10, 0.07), cu, gn, GLASS_HI - 0.035, alu, "frame_top"))
		parts.append(front.box((w, 0.12, 0.07), cu, gn + 0.01, TRANSOM, alu, "transom"))
		if k == 1:
			continue
		parts.append(front.box((w, 0.10, GLASS_LO - PLINTH), cu, gn, (GLASS_LO + PLINTH) * 0.5, kick, "kick"))
		parts.append(front.box((w, 0.12, 0.06), cu, gn + 0.01, GLASS_LO + 0.03, alu, "sill"))
	for u in FU[1:-1]:
		parts.append(front.box((0.08, 0.13, GLASS_HI - PLINTH), u, gn, (GLASS_HI + PLINTH) * 0.5, alu, "mullion"))
	# Panes: [lower, transom] per bay; None marks a shattered pane.
	lower_z = (GLASS_LO + 0.06, TRANSOM - 0.035)
	upper_z = (TRANSOM + 0.035, GLASS_HI - 0.07)
	for k, (lo_ok, up_ok) in enumerate(((True, False), (None, True), (True, True), (False, True), (True, True))):
		u0, u1 = FU[k] + 0.04, FU[k + 1] - 0.04
		if lo_ok is True:
			parts.append(front.quad(u0, u1, lower_z[0], lower_z[1], gn, glass, "glass"))
			s = rnd.uniform(0.2, 0.5)
			parts.append(front.quad(u0 + (u1 - u0) * s, u0 + (u1 - u0) * (s + 0.12), lower_z[0] + 0.1, lower_z[1] - 0.25, gn + 0.003, sheen, "sheen",
									u0 + (u1 - u0) * (s + 0.28), u0 + (u1 - u0) * (s + 0.36)))
		elif lo_ok is False:
			parts += shards(front, u0, u1, lower_z[0], lower_z[1], gn, glass, rnd)
		if up_ok:
			parts.append(front.quad(u0, u1, upper_z[0], upper_z[1], gn, glass, "glass"))
		else:
			parts += shards(front, u0, u1, upper_z[0], upper_z[1], gn, glass, rnd)
	# The double door: alloy leaves, the right one's glass gone, pull handles.
	d0, d1 = FU[1] + 0.04, FU[2] - 0.04
	dm = (d0 + d1) * 0.5
	for (a, b, ok) in ((d0, dm - 0.01, True), (dm + 0.01, d1, False)):
		c = (a + b) * 0.5
		parts.append(front.box((b - a, 0.06, 0.12), c, gn + 0.02, PLINTH + 0.06, alu, "door_rail"))
		parts.append(front.box((b - a, 0.06, 0.08), c, gn + 0.02, TRANSOM - 0.08, alu, "door_rail"))
		parts.append(front.box((0.07, 0.06, TRANSOM - PLINTH - 0.04), a + 0.035, gn + 0.02, (TRANSOM + PLINTH) * 0.5 - 0.02, alu, "door_stile"))
		parts.append(front.box((0.07, 0.06, TRANSOM - PLINTH - 0.04), b - 0.035, gn + 0.02, (TRANSOM + PLINTH) * 0.5 - 0.02, alu, "door_stile"))
		if ok:
			parts.append(front.quad(a + 0.07, b - 0.07, PLINTH + 0.12, TRANSOM - 0.12, gn, glass, "glass"))
		else:
			parts += shards(front, a + 0.07, b - 0.07, PLINTH + 0.12, TRANSOM - 0.12, gn, glass, rnd)
		hu = b - 0.12 if ok else a + 0.12
		parts.append(front.box((0.035, 0.05, 0.55), hu, gn + 0.09, 1.05, metal, "handle"))
	parts.append(front.quad(FU[0] + 0.2, FU[0] + 0.62, 0.9, 1.45, gn + 0.004, poster, "poster"))
	parts.append(front.text("1+1", 0.16, FU[0] + 0.41, gn + 0.008, 1.26, poster_ink))
	parts.append(front.quad(FU[0] + 0.24, FU[0] + 0.58, 0.98, 1.04, gn + 0.006, poster_ink, "poster_band"))

	LU = [1.2, 2.08, HY - T]
	for k in range(2):
		u0, u1 = LU[k], LU[k + 1]
		cu = (u0 + u1) * 0.5
		w = u1 - u0
		parts.append(left.box((w, 0.10, 0.07), cu, gn, GLASS_HI - 0.035, alu, "frame_top"))
		parts.append(left.box((w, 0.12, 0.07), cu, gn + 0.01, TRANSOM, alu, "transom"))
		parts.append(left.box((w, 0.10, GLASS_LO - PLINTH), cu, gn, (GLASS_LO + PLINTH) * 0.5, kick, "kick"))
		parts.append(left.box((w, 0.12, 0.06), cu, gn + 0.01, GLASS_LO + 0.03, alu, "sill"))
		parts.append(left.quad(u0 + 0.04, u1 - 0.04, lower_z[0], lower_z[1], gn, glass, "glass"))
		parts.append(left.quad(u0 + 0.04, u1 - 0.04, upper_z[0], upper_z[1], gn, glass, "glass"))
		parts.append(left.quad(u0 + 0.3, u0 + 0.4, lower_z[0] + 0.2, lower_z[1] - 0.3, gn + 0.003, sheen, "sheen", u0 + 0.55, u0 + 0.65))
	parts.append(left.box((0.08, 0.13, GLASS_HI - PLINTH), LU[1], gn, (GLASS_HI + PLINTH) * 0.5, alu, "mullion"))
	parts.append(left.box((0.08, 0.13, GLASS_HI - PLINTH), LU[0], gn, (GLASS_HI + PLINTH) * 0.5, alu, "mullion"))
	parts.append(left.quad(LU[1] + 0.15, LU[1] + 0.6, 0.75, 1.35, gn + 0.004, poster, "poster"))
	parts.append(left.quad(LU[1] + 0.2, LU[1] + 0.55, 1.1, 1.2, gn + 0.006, freezer_band, "poster_band"))

	# Sign band: cream panels with two stripes, the left panel sagging off its frame.
	bz = (BAND_LO + BAND_HI) * 0.5
	bh = BAND_HI - BAND_LO
	sag = []
	for (x0, x1) in ((-HX - 0.12, -2.0), (-2.0, 0.0), (0.0, 2.0), (2.0, HX)):
		grp = [geo.box((x1 - x0 - 0.02, 0.10, bh), ((x0 + x1) * 0.5, BAND_Y + 0.05, bz), mat=sign_m, bevel=0.015, bevel_segments=1, name="sign_panel"),
			   front.quad(x0 + 0.01, x1 - 0.01, BAND_HI - 0.19, BAND_HI - 0.09, 0.128, teal, "stripe"),
			   front.quad(x0 + 0.01, x1 - 0.01, BAND_LO + 0.09, BAND_LO + 0.19, 0.128, orange, "stripe")]
		if x0 < -HX:
			sag = grp
		parts += grp
	pivot(sag, (-2.0, BAND_Y, BAND_HI), 'Y', -6.5)
	for z in (BAND_LO + 0.2, BAND_HI - 0.2):
		parts.append(geo.box((2.0, 0.04, 0.05), (-3.05, -HY - 0.02, z), mat=metal, name="sign_rail"))
	parts.append(geo.cylinder(0.025, 1.7, (-3.0, -HY - 0.05, bz), (0, 90, 0), mat=tube_m, segments=6, name="sign_tube"))
	parts.append(front.text("24시 편의마트", 0.55, 1.05, 0.125, bz - 0.04, letter, 33.0))
	for (x, z, w, h) in ((2.6, BAND_HI - 0.14, 0.35, 0.06), (-0.4, BAND_LO + 0.12, 0.25, 0.08), (3.5, bz + 0.2, 0.2, 0.25)):
		parts.append(front.quad(x, x + w, z - h * 0.5, z + h * 0.5, 0.138, peel, "peel"))
	# The band wraps the left corner over the side windows.
	parts.append(geo.box((0.10, HY - 1.15, bh), (-HX - 0.07, (-HY - 1.15) * 0.5 + 0.0, bz), mat=sign_m, bevel=0.015, bevel_segments=1, name="sign_side"))
	for (zz, m) in ((BAND_HI - 0.14, teal), (BAND_LO + 0.14, orange)):
		parts.append(left.quad(1.16, HY - 0.01, zz - 0.05, zz + 0.05, 0.128, m, "stripe"))
	parts.append(left.text("편의마트", 0.42, 2.1, 0.126, bz - 0.03, letter, 30.0))
	# The two stripes run on round the side walls.
	for w, (u0, u1) in ((right, (-HY, HY)), (left, (-HY, 1.15))):
		parts.append(w.quad(u0, u1, BAND_HI - 0.19, BAND_HI - 0.09, 0.006, teal, "side_stripe"))
		parts.append(w.quad(u0, u1, BAND_LO + 0.09, BAND_LO + 0.19, 0.006, orange, "side_stripe"))

	# A projecting "24" box sign on the right corner, lettered both ways.
	psx, psy = HX + 0.45, -HY + 0.3
	add(geo.box((0.62, 0.14, 0.62), (psx, psy, 2.98), mat=sign_m, bevel=0.015, bevel_segments=1, name="side_sign"))
	parts.append(geo.box((0.62, 0.15, 0.08), (psx, psy, 2.71), mat=orange, name="side_sign_band"))
	for z in (2.78, 3.18):
		parts.append(geo.tube([(HX, psy, z), (HX + 0.15, psy, z)], 0.02, mat=metal, segments=4, name="bracket"))
	for (s, rz) in ((-1.0, 0), (1.0, 180)):
		parts.append(flat_text("24", 0.30, (psx, psy + s * 0.075, 3.0), (90, 0, rz), orange))
	parts.append(right.quad(psy - 0.04, psy + 0.03, 1.9, 2.72, 0.004, rust, "rust_run", psy - 0.02, psy + 0.02))

	# The awning: striped canvas on three arms, one stripe torn short, one hanging, one gone.
	CX = [-0.75 + k * 0.5 for k in range(10)]
	ay, az = -HY - 0.02, BAND_LO - 0.02
	OUT, DROP = 0.64, 0.34

	def cpt(x, t, lift=0.0):
		return (x, ay - OUT * t, az - DROP * t - 0.04 * math.sin(math.pi * t) + lift)

	for k in range(9):
		x0, x1 = CX[k], CX[k + 1]
		m = awning_a if k % 2 == 0 else awning_b
		if k == 6:
			continue
		if k == 4:
			# Torn loose from the bar: hangs from the wall in a long flap.
			vs = []
			for i in range(5):
				s = i / 4.0
				curl = 0.12 * s * s
				vs += [(x0 + 0.02, ay - 0.04 - curl, az - 0.95 * s), (x1 - 0.04, ay - 0.04 - curl * 1.4, az - 0.85 * s)]
			faces = [(2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2) for i in range(4)]
			parts.append(mesh_obj("awning_flap", vs, faces, m, True))
			continue
		reach = [1.0, 1.0, 1.0]
		if k == 2:
			reach = [0.62, 0.38, 0.55]
		vs = []
		for i in range(4):
			t = i / 3.0
			for c, xx in enumerate((x0, (x0 + x1) * 0.5, x1)):
				vs.append(cpt(xx, t * reach[c]))
		faces = []
		for i in range(3):
			for c in range(2):
				a = i * 3 + c
				faces.append((a, a + 1, a + 4, a + 3))
		add(mesh_obj("awning", vs, faces, m, True))
		if k != 2:
			p0, p1 = cpt(x0, 1.0), cpt(x1, 1.0)
			parts.append(mesh_obj("valance", [p0, p1, (p1[0], p1[1], p1[2] - 0.17), (p0[0], p0[1], p0[2] - 0.17)], [(0, 1, 2, 3)], m))
	for x in (CX[0], 0.75, 2.25, CX[-1]):
		parts.append(geo.tube([cpt(x, 0.0, 0.02), cpt(x, 1.0, 0.03)], 0.02, mat=metal, segments=4, name="awning_arm"))
	parts.append(geo.tube([cpt(CX[0], 1.0, 0.03), cpt(CX[-1], 1.0, 0.03)], 0.02, mat=metal, segments=4, name="awning_bar"))
	for x in (-0.05, 1.95):
		parts.append(front.quad(x, x + 0.05, BAND_LO + 0.2, BAND_HI - 0.2, 0.139, rust, "rust_run", x + 0.01, x + 0.04))

	# Out front: crates by the door, the ice-cream freezer, the table with one chair knocked over.
	for (x, y, z, rz, m) in ((-3.45, -3.32, 0.08, 4, crate_y), (-3.43, -3.33, 0.36, -6, crate_b), (-2.85, -3.40, 0.08, 18, crate_y)):
		c = geo.box((0.52, 0.36, 0.28), (x, y, z + 0.14), (0, 0, rz), mat=m, name="crate")
		add(c)
		parts.append(flat_rect(0.44, 0.28, (x, y, z + 0.282), rz, dark, "crate_hollow"))
	tipped = geo.box((0.52, 0.36, 0.28), (-2.55, -3.5, 0.34), (0, 90, -24), mat=crate_b, name="crate")
	add(tipped)

	fx, fy = -0.1, -HY - 0.36
	add(geo.box((1.12, 0.6, 0.78), (fx, fy, 0.08 + 0.39), mat=freezer_m, bevel=0.03, bevel_segments=1, name="freezer"))
	parts.append(geo.box((1.0, 0.48, 0.01), (fx, fy, 0.865), mat=glass, name="freezer_lid"))
	parts.append(geo.box((0.03, 0.5, 0.015), (fx + 0.02, fy, 0.872), mat=metal, name="freezer_rail"))
	parts.append(geo.box((0.9, 0.01, 0.2), (fx, fy - 0.305, 0.55), mat=freezer_band, name="freezer_band"))

	tx, ty = 3.0, -HY - 0.38
	add(geo.box((0.7, 0.7, 0.04), (tx, ty, 0.74), mat=plastic_w, bevel=0.015, bevel_segments=1, name="table"))
	for (dx, dy) in ((-0.3, -0.3), (0.3, -0.3), (-0.3, 0.3), (0.3, 0.3)):
		parts.append(geo.box((0.04, 0.04, 0.66), (tx + dx, ty + dy, 0.41), (dy * 10, -dx * 10, 0), mat=plastic_w, name="table_leg"))

	def chair(loc, rz, tip=None):
		cp = [geo.box((0.42, 0.40, 0.04), (0, 0, 0.44), mat=plastic_g, bevel=0.01, bevel_segments=1, name="chair_seat"),
			  geo.box((0.42, 0.04, 0.42), (0, 0.2, 0.68), (-8, 0, 0), mat=plastic_g, name="chair_back")]
		for (dx, dy) in ((-0.18, -0.17), (0.18, -0.17), (-0.18, 0.17), (0.18, 0.17)):
			cp.append(geo.box((0.035, 0.035, 0.44), (dx, dy, 0.22), mat=plastic_g, name="chair_leg"))
		if tip is not None:
			pivot(cp, (0, 0.2, 0.0), 'X', tip)
			for o in cp:
				o.location.z += 0.05
				geo.apply_transform(o)
		for o in cp:
			o.matrix_basis = Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(rz), 4, 'Z') @ o.matrix_basis
			geo.apply_transform(o)
		return cp

	parts += chair((tx - 0.62, ty + 0.02, 0.08), 90)
	parts += chair((tx + 0.72, ty - 0.18, 0.08), 75, tip=-90)

	# Right side: the dead drink machine, a barred storeroom window, a downpipe.
	vx, vy = HX + 0.38, -1.95
	add(geo.box((0.72, 1.0, 1.82), (vx, vy, 0.91 + 0.02), mat=vend_m, bevel=0.03, bevel_segments=1, name="vending"))
	parts.append(geo.box((0.02, 0.96, 0.22), (vx + 0.36, vy, 1.68), mat=vend_top, name="vend_head"))
	parts.append(geo.box((0.02, 0.66, 0.62), (vx + 0.362, vy - 0.12, 1.22), mat=glass, name="vend_window"))
	for row, z in enumerate((1.04, 1.34)):
		for k in range(5):
			yc, zc = vy - 0.38 + k * 0.13, z + 0.06
			parts.append(mesh_obj("vend_can", [(vx + 0.375, yc - 0.04, zc - 0.085), (vx + 0.375, yc + 0.04, zc - 0.085), (vx + 0.375, yc + 0.04, zc + 0.085),
												(vx + 0.375, yc - 0.04, zc + 0.085)], [(0, 1, 2, 3)], cans[(k + row * 2) % 5]))
		parts.append(geo.box((0.03, 0.62, 0.03), (vx + 0.37, vy - 0.12, z - 0.06), mat=vend_top, name="vend_buttons"))
	parts.append(geo.box((0.02, 0.18, 0.5), (vx + 0.362, vy + 0.36, 1.2), mat=vend_top, name="vend_coin"))
	parts.append(geo.box((0.03, 0.62, 0.2), (vx + 0.36, vy - 0.08, 0.38), mat=dark, name="vend_hatch"))
	parts.append(right.quad(vy - 0.3, vy + 0.3, PLINTH, 0.5, 0.003, stain, "stain"))

	def barred_window(w, u, z0, z1, width):
		cu = u
		ps = [w.box((width + 0.12, 0.08, 0.06), cu, 0.02, z1 + 0.03, cope_m, "win_head"),
			  w.box((width + 0.14, 0.12, 0.06), cu, 0.04, z0 - 0.03, cope_m, "win_sill"),
			  w.quad(cu - width * 0.5, cu + width * 0.5, z0, z1, 0.005, glass, "win_glass"),
			  w.box((0.05, 0.05, z1 - z0), cu, 0.03, (z0 + z1) * 0.5, alu, "win_mid")]
		for k in range(5):
			uu = cu - width * 0.5 + width * (k + 0.5) / 5.0
			ps.append(w.quad(uu - 0.014, uu + 0.014, z0 - 0.05, z1 + 0.05, 0.09, metal, "bar"))
		for zz in (z0 + 0.05, z1 - 0.05):
			ps.append(w.quad(cu - width * 0.5 - 0.05, cu + width * 0.5 + 0.05, zz - 0.015, zz + 0.015, 0.095, metal, "bar_h"))
		ps.append(w.quad(cu - 0.15, cu + 0.1, max(PLINTH, z0 - 0.9), z0 - 0.06, 0.004, stain, "stain", cu - 0.25, cu + 0.2))
		return ps

	parts += barred_window(right, 0.9, 1.85, 2.4, 0.9)
	parts += barred_window(back, 0.4, 1.85, 2.4, 0.9)

	def downpipe(x, y, ox, oy):
		top = (x, y, DECK + 0.05)
		return [geo.tube([(x - ox * 0.25, y - oy * 0.25, DECK + 0.08), (x, y, DECK - 0.1), (x, y, 0.3), (x + ox * 0.12, y + oy * 0.12, 0.12)], 0.05,
						 mat=pvc, segments=6, name="downpipe"),
				geo.box((0.14, 0.14, 0.12), top, mat=pvc, name="hopper")]

	parts += downpipe(HX + 0.08, HY - 0.25, 1, 0)
	parts += downpipe(-HX - 0.08, HY - 0.25, -1, 0)
	for y in (-0.6, 1.8):
		parts.append(right.quad(y, y + 0.12, 1.2, TOP, 0.003, stain, "stain", y - 0.05, y + 0.2))

	# Back: steel door under a hood, meter box, refrigeration units and pipes, bin and bags.
	du = 2.4
	parts.append(back.box((1.12, 0.08, 2.2), du, 0.02, PLINTH + 1.1, metal, "door_frame"))
	add(back.box((0.94, 0.06, 2.04), du, 0.05, PLINTH + 1.03, door_m, "back_door"))
	parts.append(back.box((0.5, 0.02, 0.22), du, 0.085, PLINTH + 0.3, dark, "door_louvre"))
	for k in range(4):
		parts.append(back.box((0.48, 0.03, 0.02), du, 0.095, PLINTH + 0.22 + k * 0.055, door_m, "louvre_slat"))
	parts.append(back.box((0.16, 0.06, 0.04), du - 0.33, 0.1, 1.08, metal, "lever"))
	add(back.box((1.4, 0.44, 0.15), du, 0.22, 0.075, base_m, "step"))
	hood = add(back.box((1.36, 0.55, 0.04), du, 0.26, 2.52, unit_m, "hood"))
	pivot([hood], back.at(du, 0.0, 2.55), 'X', -10.0)
	for s in (-0.6, 0.6):
		p0, p1 = back.at(du + s, 0.02, 2.2), back.at(du + s, 0.5, 2.44)
		parts.append(geo.tube([p0, p1], 0.015, mat=metal, segments=4, name="hood_stay"))
	parts.append(back.box((0.18, 0.1, 0.12), du, 0.06, 2.34, metal, "lamp"))
	parts.append(back.quad(du + 0.45, du + 0.6, 1.2, 2.3, 0.004, rust, "rust_run", du + 0.5, du + 0.56))
	add(back.box((0.42, 0.14, 0.55), 1.3, 0.07, 1.78, unit_m, "meter_box"))
	parts.append(back.box((0.3, 0.02, 0.18), 1.3, 0.145, 1.85, glass, "meter_window"))
	parts.append(back.quad(1.25, 1.35, 1.1, 1.5, 0.003, rust, "rust_run", 1.2, 1.4))

	add(back.box((2.9, 0.75, 0.1), -1.55, 0.42, 0.05, base_m, "unit_pad"))
	for k, u in enumerate((-0.75, -2.3)):
		add(back.box((1.05, 0.42, 0.76), u, 0.38, 0.1 + 0.38, unit_m, "condenser"))
		fan = back.at(u - 0.12, 0.6, 0.5)
		parts.append(geo.cylinder(0.27, 0.02, fan, (90, 0, 0), mat=dark, segments=10, name="fan"))
		parts.append(geo.cylinder(0.08, 0.03, back.at(u - 0.12, 0.61, 0.5), (90, 0, 0), mat=unit_m, segments=6, name="fan_hub"))
		for a in (45, 135):
			parts.append(geo.box((0.56, 0.02, 0.02), back.at(u - 0.12, 0.615, 0.5), (0, a, 0), mat=unit_m, name="grille"))
		parts.append(back.box((0.18, 0.02, 0.55), u + 0.38, 0.6, 0.5, metal, "fins"))
		x0 = back.at(u + 0.38, 0.2, 0.62)
		px = back.at(u + 0.38 + 0.08 * k, 0.06, 0.0)
		parts.append(geo.tube([x0, (px[0], px[1], 0.75), (px[0], px[1], 2.6), back.at(u + 0.38 + 0.08 * k, -0.05, 2.62)], 0.035,
							  mat=pipe_m, segments=6, name="ref_pipe"))
		parts.append(geo.tube([back.at(u - 0.5, 0.2, 0.25), back.at(u - 0.62, 0.32, 0.12), back.at(u - 0.7, 0.55, 0.11)], 0.015,
							  mat=pvc, segments=4, name="drain_hose"))
		for zz in (1.3, 2.1):
			parts.append(back.box((0.1, 0.05, 0.04), u + 0.38 + 0.08 * k, 0.07, zz, metal, "clamp"))

	bx, by = -1.2, HY + 0.38
	add(geo.box((0.6, 0.66, 0.92), (bx, by, 0.08 + 0.46), (0, 0, 6), mat=bin_m, bevel=0.03, bevel_segments=1, name="bin"))
	lid = add(geo.box((0.64, 0.70, 0.05), (bx, by, 1.0), (0, 0, 6), mat=bin_m, name="bin_lid"))
	pivot([lid], (bx, by + 0.35, 1.0), 'X', -18.0)
	for s in (-0.22, 0.22):
		parts.append(geo.cylinder(0.09, 0.06, (bx + s, by + 0.28, 0.1), (0, 90, 6), mat=mat.get("rubber"), segments=6, name="wheel"))
	for (x, y, r, m, seed) in ((-3.65, HY + 0.34, (0.28, 0.25, 0.24), bag_w, 1), (-3.3, HY + 0.58, (0.25, 0.22, 0.2), bag_k, 2),
							   (-0.68, HY + 0.35, (0.22, 0.2, 0.18), bag_w, 4)):
		b = geo.blob(r, (x, y, r[2] * 0.8), mat=m, subdivisions=2, lumpy=0.25, seed=seed, name="bag")
		parts.append(b)
		parts.append(geo.cone(0.06, 0.12, (x, y, r[2] * 1.75), (rnd.uniform(-20, 20), 0, 0), mat=m, segments=5, name="bag_knot"))
	for (x, y) in ((-2.0, HY + 0.6), (-3.0, HY + 0.62)):
		parts.append(geo.box((0.16, 0.12, 0.2), (x, y, 0.06), (rnd.uniform(70, 110), 0, rnd.uniform(0, 180)), mat=cans[int(x) % 5], name="litter"))

	# A grimy skirting round the plain walls, and stains under the copings.
	for (w, u0, u1) in ((back, -HX, HX), (right, -HY, HY), (left, -HY, 1.2)):
		parts.append(w.quad(u0, u1, PLINTH, PLINTH + 0.42, 0.004, base_m, "skirting"))
	for (w, us) in ((back, (-3.2, -0.5, 0.9, 3.4)), (left, (-2.5, -0.4, 0.6)), (right, (-1.2, 2.4))):
		for u in us:
			wd = rnd.uniform(0.08, 0.16)
			parts.append(w.quad(u - wd * 0.2, u + wd * 0.2, TOP - rnd.uniform(0.9, 1.6), TOP, 0.003, stain, "stain", u - wd, u + wd))

	# Left side: the outdoor unit for the shop's air conditioning.
	au = -0.3
	add(left.box((0.86, 0.32, 0.62), au, 0.2, 0.14 + 0.31, unit_m, "ac_unit"))
	parts.append(left.box((0.9, 0.36, 0.14), au, 0.2, 0.07, base_m, "ac_block"))
	fan = left.at(au - 0.12, 0.37, 0.46)
	parts.append(geo.cylinder(0.22, 0.02, fan, (0, 90, 0), mat=dark, segments=10, name="fan"))
	for a in (45, 135):
		parts.append(geo.box((0.02, 0.46, 0.02), left.at(au - 0.12, 0.38, 0.46), (a, 0, 0), mat=unit_m, name="grille"))
	ap = left.at(au + 0.3, 0.04, 0.0)
	parts.append(geo.tube([left.at(au + 0.3, 0.2, 0.6), (ap[0], ap[1], 0.7), (ap[0], ap[1], 2.3), left.at(au + 0.3, -0.05, 2.32)], 0.03,
						  mat=pipe_m, segments=6, name="ac_pipe"))
	parts.append(left.quad(au + 0.25, au + 0.35, 0.82, 2.3, 0.002, rust, "rust_run", au + 0.28, au + 0.32))

	# Back crates of empties against the right wall.
	for (x, y, z, rz, m) in ((HX + 0.3, 2.0, 0.0, 88, crate_b), (HX + 0.32, 2.05, 0.28, 92, crate_y), (HX + 0.3, 1.45, 0.0, 84, crate_y)):
		c = geo.box((0.52, 0.36, 0.28), (x, y, z + 0.14), (0, 0, rz), mat=m, name="crate")
		add(c)
		parts.append(flat_rect(0.44, 0.28, (x, y, z + 0.282), rz, dark, "crate_hollow"))

	# Roof: a squat water tank, damp patches, moss banked against the parapets and over their lips.
	tank = (2.75, 1.85)
	for dy in (-0.28, 0.28):
		add(geo.box((1.0, 0.16, 0.1), (tank[0], tank[1] + dy, DECK + 0.05), mat=base_m, name="tank_sleeper"))
	add(geo.lathe([(0.40, 0.0), (0.44, 0.04), (0.44, 0.40), (0.38, 0.47), (0.16, 0.52), (0.16, 0.57), (0.0, 0.58)],
				  (tank[0], tank[1], DECK + 0.1), mat=tank_m, segments=12, name="water_tank"))
	parts.append(geo.tube([(tank[0] - 0.44, tank[1], DECK + 0.2), (tank[0] - 0.75, tank[1], DECK + 0.2), (tank[0] - 0.75, tank[1], DECK + 0.02)],
						  0.03, mat=pvc, segments=5, name="tank_pipe"))
	# The green waterproof paint every roof had, faded, worn through to grey, pooled dark where water stands.
	for (x, y, rx, ry) in ((0.2, 0.9, 1.3, 0.55), (-2.3, -1.2, 0.8, 0.5), (2.6, -1.7, 0.7, 0.35), (-0.9, -2.2, 0.5, 0.25), (1.9, 1.0, 0.35, 0.5)):
		parts.append(stain_patch(x, y, DECK + 0.002, rx, ry, bare, rnd, "bare"))
	for (x, y, rx, ry) in ((-1.6, 0.4, 1.0, 0.7), (1.2, -1.3, 0.7, 0.45), (-3.1, 1.9, 0.6, 0.7)):
		parts.append(stain_patch(x, y, DECK + 0.004, rx, ry, wet, rnd))
	ix2, iy2 = HX - T - 0.004, HY - T - 0.004
	for (a, b) in (((-ix2, -iy2), (ix2, -iy2)), ((ix2, -iy2), (ix2, iy2)), ((ix2, iy2), (-ix2, iy2)), ((-ix2, iy2), (-ix2, -iy2))):
		parts.append(mesh_obj("deck_upstand", [(a[0], a[1], DECK), (b[0], b[1], DECK), (b[0], b[1], DECK + 0.3), (a[0], a[1], DECK + 0.3)], [(0, 1, 2, 3)], deck_m))
	bank = [((-HX + T + 0.08, 1.2), (0.3, 1.5, 0.12), (0, 0, 0), moss), ((-1.6, HY - T - 0.08), (1.7, 0.3, 0.13), (0, 0, 0), moss_dark),
			((HX - T - 0.08, -0.6), (0.25, 1.0, 0.1), (0, 0, 0), moss), ((-2.6, -HY + T + 0.08), (0.9, 0.25, 0.1), (0, 0, 0), moss_dark),
			((1.4, HY - T - 0.1), (0.7, 0.28, 0.1), (0, 0, 0), moss), ((-3.3, 2.4), (0.55, 0.5, 0.12), (0, 0, 0), moss),
			((-1.7, 0.5), (0.8, 0.55, 0.07), (0, 0, 0), moss_dark), ((1.3, -1.3), (0.5, 0.35, 0.06), (0, 0, 0), moss)]
	for k, ((x, y), r, rot, m) in enumerate(bank):
		b = moss_pad(r, x, y, DECK - 0.02, rot, m, k + 3)
		if k in (0, 5):
			geo.tag(b, "pulse")
		parts.append(b)
	ledge = [((-HX + 0.11, 1.9), (0.2, 0.8, 0.09), moss), ((-HX + 0.11, -0.5), (0.18, 0.45, 0.07), moss_dark), ((HX - 0.11, 2.4), (0.2, 0.55, 0.08), moss),
			 ((-2.9, HY - 0.11), (0.85, 0.2, 0.09), moss), ((1.0, HY - 0.11), (0.5, 0.18, 0.07), moss_dark), ((-3.6, -HY + 0.11), (0.45, 0.17, 0.07), moss_dark),
			 ]
	for k, ((x, y), r, m) in enumerate(ledge):
		b = moss_pad(r, x, y, TOP + 0.05, (0, 0, 0), m, k + 20)
		if k == 3:
			geo.tag(b, "pulse")
		parts.append(b)
	for (x, y, r, rot, m) in ((-3.0, HY + 0.02, (0.4, 0.5, 0.06), (-90, 0, 0), moss_dark), (-HX - 0.02, 1.9, (0.45, 0.4, 0.06), (0, -90, 0), moss),
							  (HX + 0.02, 2.4, (0.3, 0.35, 0.05), (0, 90, 0), moss_dark)):
		parts.append(moss_pad(r, x, y, TOP - 0.25, rot, m, int(x * 7) + 60))
	for (cx, cy, n) in ((-3.5, 2.5, 4), (3.45, 2.6, 3), (-3.45, -2.45, 3), (0.6, 2.55, 3), (-1.7, 0.6, 3), (3.4, -0.6, 2)):
		weeds(parts, cx, cy, n, rnd, (grass, leaf, leaf_dark), 0.2, 0.45, 0.22, DECK)
	g = geo.blob(0.05, (-3.2, 2.3, DECK + 0.3), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)
	parts.append(moss_pad((0.42, 0.25, 0.07), 3.25, -HY - 0.36, 2.36, (-28, 0, 0), moss, 31))

	# Ground: moss along the wall feet, weeds in the corners and the apron cracks.
	for k, (x, y, r) in enumerate(((HX + 0.05, 0.4, (0.22, 0.9, 0.14)), (-HX - 0.05, 1.8, (0.22, 0.8, 0.14)), (0.5, HY + 0.05, (0.9, 0.22, 0.14)),
								   (-1.6, -HY - 0.6, (0.6, 0.14, 0.08)), (3.7, HY + 0.1, (0.35, 0.35, 0.12)))):
		parts.append(moss_pad(r, x, y, -0.02, (0, 0, 0), moss if k % 2 else moss_dark, k + 40))
	for (cx, cy, n) in ((HX + 0.15, -HY - 0.1, 3), (-HX - 0.15, -HY - 0.2, 3), (HX + 0.12, HY + 0.12, 4), (-HX - 0.12, HY + 0.12, 4),
						(-1.2, -HY - 0.5, 2), (1.6, -HY - 0.45, 2), (HX + 0.15, 0.2, 3), (-HX - 0.15, 1.0, 3), (0.2, HY + 0.15, 3), (-1.0, HY + 0.6, 2)):
		weeds(parts, cx, cy, n, rnd, (leaf, leaf_dark, grass))

	# Ivy up the back-left corner onto the parapet, and up the right wall behind the crates.
	leaves = (leaf, leaf_dark)
	path = [(-HX - 0.03, 2.6, 0.05), (-HX - 0.03, 2.45, 0.8), (-HX - 0.03, 2.7, 1.6), (-HX - 0.03, 2.4, 2.4), (-HX - 0.03, 2.55, 3.2), (-HX + 0.05, 2.5, TOP + 0.1)]
	ivy(parts, path, [0.03, 0.026, 0.022, 0.018, 0.014, 0.01], (-1, 0, 0), vine_m, leaves, rnd, 3)
	path = [(-HX - 0.03, 1.6, 0.05), (-HX - 0.03, 1.8, 0.7), (-HX - 0.03, 1.5, 1.3), (-HX - 0.03, 1.7, 1.9)]
	ivy(parts, path, [0.022, 0.018, 0.014, 0.01], (-1, 0, 0), vine_m, leaves, rnd, 2)
	path = [(-3.4, HY + 0.03, 0.05), (-3.6, HY + 0.03, 0.9), (-3.3, HY + 0.03, 1.7), (-3.55, HY + 0.03, 2.6), (-3.4, HY + 0.03, TOP)]
	ivy(parts, path, [0.026, 0.022, 0.018, 0.014, 0.01], (0, 1, 0), vine_m, leaves, rnd, 3)
	path = [(HX + 0.03, 2.4, 0.5), (HX + 0.03, 2.6, 1.2), (HX + 0.03, 2.3, 1.9), (HX + 0.03, 2.5, 2.6)]
	ivy(parts, path, [0.022, 0.018, 0.014, 0.01], (1, 0, 0), vine_m, leaves, rnd, 2)
	for (x, y, z, r) in ((-HX - 0.05, 2.55, TOP + 0.05, 0.24), (-3.45, HY + 0.05, TOP + 0.02, 0.22), (-HX - 0.06, 2.5, 2.9, 0.2)):
		c = geo.blob((r, r, r * 0.8), (x, y, z), mat=leaf, subdivisions=1, lumpy=0.3, seed=int(z * 10), name="ivy_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	for i, p in enumerate(shell):
		geo.moss_mask(p, seed=i + 3)

	res = pipeline.static("store_1f", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
