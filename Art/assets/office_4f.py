# A four-storey shop building on the high street (상가 빌딩): a pharmacy, an estate agent and a shuttered laundry
# under the tenants' signs, band windows with the third floor blown out and a tree growing from it, a vertical
# sign down the front corner, a rusty rooftop name frame missing letters, an FRP water tank and the stair hut;
# steel fire stairs, AC units and the yellow gas pipe behind; vines off the right parapet, moss on the ledges.
# The helpers above build() are shared with villa_3f and shop_2f.
import math
import os

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector, noise

from gse import bake, core, geo, mat

FONT = os.path.join(core.GAME_DIR, "Fonts", "NanumGothic-Bold.ttf")


def mesh_obj(name, verts, faces, m, smooth=False):
	bm = bmesh.new()
	used = sorted({i for f in faces for i in f})
	bv = {i: bm.verts.new(verts[i]) for i in used}
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


def drop_faces(obj, fn):
	# Faces nobody sees (undersides, backs against a wall) would only waste texture space.
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


def turn(objs, pivot, axis, deg):
	bpy.context.view_layer.update()
	p = Vector(pivot)
	m = Matrix.Translation(p) @ Matrix.Rotation(math.radians(deg), 4, Vector(axis)) @ Matrix.Translation(-p)
	for o in objs:
		o.matrix_world = m @ o.matrix_world
	return objs


def lettering(body, size, loc, rot, m, res=1, straight=25.0, limit=400, name="letters"):
	# Flat lettering: only the font's fill, curves cut to their control points and nearly straight outline
	# points dropped, so a glyph costs some 35 triangles.
	curve = bpy.data.curves.new(name, 'FONT')
	curve.body = body
	if os.path.exists(FONT):
		curve.font = bpy.data.fonts.load(FONT, check_existing=True)
	curve.size = size
	curve.resolution_u = res
	curve.align_x = 'CENTER'
	curve.align_y = 'CENTER'
	obj = core.link(bpy.data.objects.new(name, curve))
	obj.location = Vector(loc)
	obj.rotation_euler = Euler([math.radians(a) for a in rot], 'XYZ')
	curve.materials.append(m)
	core.select_only([obj])
	bpy.ops.object.convert(target='MESH')
	obj = bpy.context.view_layer.objects.active
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(1.0), use_dissolve_boundaries=False, verts=[], edges=bm.edges[:])
	for _ in range(3):
		dead = []
		for v in bm.verts:
			es = [e for e in v.link_edges if e.is_boundary]
			if len(v.link_edges) == 2 and len(es) == 2:
				a = (es[0].other_vert(v).co - v.co).normalized()
				b = (es[1].other_vert(v).co - v.co).normalized()
				if a.dot(b) < -math.cos(math.radians(straight)):
					dead.append(v)
		dead = dead[::2]
		if dead:
			bmesh.ops.dissolve_verts(bm, verts=dead)
	bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
	bm.to_mesh(obj.data)
	bm.free()
	obj["uvs"] = 1.0
	return geo.decimate(obj, limit)


class Face:
	# One side of a building. u runs along the wall as seen from outside, n points out of it and is measured
	# from the wall plane, z is height. rz 0 = front (-Y), 90 = right (+X), 180 = back, 270 = left.
	def __init__(self, rz, plane, half=0.0):
		a = math.radians(rz)
		self.rz = rz
		self.r = Vector((math.cos(a), math.sin(a), 0.0))
		self.n = Vector((math.sin(a), -math.cos(a), 0.0))
		self.plane = plane
		self.half = half

	def at(self, u, n, z):
		p = self.r * u + self.n * (self.plane + n)
		return (p.x, p.y, z)

	def mesh(self, verts, faces, m, name="part"):
		# verts as (u, z, n); faces wound counter-clockwise as seen from outside.
		return mesh_obj(name, [self.at(u, n, z) for (u, z, n) in verts], faces, m)

	def box(self, size, u, n, z, m, name="part", tilt=(0.0, 0.0), bevel=0.0):
		# size (along, out, up); n and z are the box centre.
		return geo.box(size, self.at(u, n, z), (tilt[0], tilt[1], self.rz), mat=m, name=name, bevel=bevel, bevel_segments=1)

	def quad(self, u0, u1, z0, z1, n, m, name="quad"):
		return self.mesh([(u0, z0, n), (u1, z0, n), (u1, z1, n), (u0, z1, n)], [(0, 1, 2, 3)], m, name)

	def wall(self, u0, u1, z0, z1, m, n=0.0, cols=1, rows=1, name="wall"):
		vs = [(u0 + (u1 - u0) * i / cols, z0 + (z1 - z0) * j / rows, n) for j in range(rows + 1) for i in range(cols + 1)]
		w = cols + 1
		faces = [(j * w + i, j * w + i + 1, (j + 1) * w + i + 1, (j + 1) * w + i) for j in range(rows) for i in range(cols)]
		return self.mesh(vs, faces, m, name)

	def text(self, body, size, u, n, z, m, limit=400, name="letters"):
		return lettering(body, size, self.at(u, n, z), (90, 0, self.rz), m, limit=limit, name=name)

	def frame(self, u, z0, w, h, f, d, m, name="frame"):
		# A window or door frame standing d proud of the wall: its face, outer sides and inner reveal.
		O = [(u - w / 2 - f, z0 - f), (u + w / 2 + f, z0 - f), (u + w / 2 + f, z0 + h + f), (u - w / 2 - f, z0 + h + f)]
		I = [(u - w / 2, z0), (u + w / 2, z0), (u + w / 2, z0 + h), (u - w / 2, z0 + h)]
		vs = [(a, b, d) for a, b in O] + [(a, b, d) for a, b in I] + [(a, b, 0.0) for a, b in O] + [(a, b, 0.0) for a, b in I]
		faces = []
		for k in range(4):
			k1 = (k + 1) % 4
			faces += [(k, k1, 4 + k1, 4 + k), (8 + k, 8 + k1, k1, k), (12 + k1, 12 + k, 4 + k, 4 + k1)]
		return self.mesh(vs, faces, m, name)

	def hole(self, u0, u1, z0, z1, depth, m, name="hole"):
		# The dark room behind an empty window: back wall, jambs and floor (never a ceiling: the camera looks down).
		vs = [(u0, z0, -depth), (u1, z0, -depth), (u1, z1, -depth), (u0, z1, -depth), (u0, z0, 0.0), (u1, z0, 0.0), (u1, z1, 0.0), (u0, z1, 0.0)]
		return self.mesh(vs, [(0, 1, 2, 3), (4, 5, 1, 0), (3, 7, 4, 0), (1, 5, 6, 2)], m, name)

	def shards(self, u0, u1, z0, z1, n, m, rnd, count=4):
		# Glass left in the frame of a broken pane: jagged triangles in from the corners.
		vs = []
		faces = []
		corners = [(u0, z0, 1, 1), (u1, z0, -1, 1), (u1, z1, -1, -1), (u0, z1, 1, -1)]
		for k in rnd.sample(range(4), min(count, 4)):
			cu, cz, su, sz = corners[k]
			a = rnd.uniform(0.15, 0.45) * (u1 - u0)
			b = rnd.uniform(0.2, 0.5) * (z1 - z0)
			base = len(vs)
			vs += [(cu, cz, n), (cu + su * a, cz, n), (cu + su * a * rnd.uniform(0.2, 0.5), cz + sz * b * rnd.uniform(0.3, 0.6), n), (cu, cz + sz * b, n)]
			tri = [(base, base + 1, base + 2), (base, base + 2, base + 3)]
			faces += [t if su * sz > 0 else (t[0], t[2], t[1]) for t in tri]
		return self.mesh(vs, faces, m, "shards")

	def cracks(self, u, z, n, m, rnd, count=6, reach=0.5):
		# Cracks running out from an impact point, each a thin sliver.
		vs = [(u, z, n)]
		faces = []
		for k in range(count):
			a = k / float(count) * math.tau + rnd.uniform(-0.3, 0.3)
			r = reach * rnd.uniform(0.5, 1.0)
			ca, sa = math.cos(a), math.sin(a)
			w = 0.012
			vs += [(u + ca * r - sa * w, z + sa * r + ca * w, n), (u + ca * r + sa * w, z + sa * r - ca * w, n)]
			faces.append((0, len(vs) - 1, len(vs) - 2))
		return self.mesh(vs, faces, m, "cracks")


def ring(x0, x1, y0, y1, z0, z1, depth, m, name="band", top=True, inner=None, outer=True):
	# A band round the building: outer faces and the ledge on top; inner faces from `inner` up for a parapet.
	# Each side is its own strip, so the ledges unwrap as thin islands and not as one hollow frame.
	o = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
	i = [(x0 + depth, y0 + depth), (x1 - depth, y0 + depth), (x1 - depth, y1 - depth), (x0 + depth, y1 - depth)]
	verts = []
	faces = []
	for k in range(4):
		k1 = (k + 1) % 4
		b = len(verts)
		verts += [o[k] + (z0,), o[k1] + (z0,), o[k1] + (z1,), o[k] + (z1,), i[k1] + (z1,), i[k] + (z1,)]
		if outer:
			faces.append((b, b + 1, b + 2, b + 3))
		if top or inner is not None:
			faces.append((b + 3, b + 2, b + 4, b + 5))
		if inner is not None:
			verts += [i[k1] + (inner,), i[k] + (inner,)]
			faces.append((b + 6, b + 7, b + 5, b + 4))
	return mesh_obj(name, verts, faces, m)


def corner_pier(sx, sy, hx, hy, w, inset, z0, z1, m, name="pier"):
	# A corner column of width w standing `inset` proud of both walls: only the faces that show.
	ox, oy = sx * hx, sy * hy
	ix, iy = sx * (hx - w), sy * (hy - w)
	wx, wy = sx * (hx - inset), sy * (hy - inset)
	pts = [(ox, oy), (ix, oy), (ix, wy), (wx, wy), (wx, iy), (ox, iy)]
	if sx * sy < 0:
		pts.reverse()
	verts = [p + (z0,) for p in pts] + [p + (z1,) for p in pts]
	faces = [(k, (k + 1) % 6, 6 + (k + 1) % 6, 6 + k) for k in range(6)]
	obj = mesh_obj(name, verts, faces, m)
	mod = obj.modifiers.new("bevel", 'BEVEL')
	mod.width = 0.02
	mod.segments = 1
	mod.limit_method = 'ANGLE'
	mod.angle_limit = math.radians(40.0)
	return obj


def lowres(objs, s):
	# Texture share for parts that need little detail (dark rooms, glass): their islands shrink before packing.
	for o in objs if isinstance(objs, (list, tuple)) else [objs]:
		o["uvs"] = s
	return objs


def flat_grid(x0, x1, y0, y1, z, cols, rows, m, name="floor"):
	vs = [(x0 + (x1 - x0) * i / cols, y0 + (y1 - y0) * j / rows, z) for j in range(rows + 1) for i in range(cols + 1)]
	w = cols + 1
	faces = [(j * w + i, j * w + i + 1, (j + 1) * w + i + 1, (j + 1) * w + i) for j in range(rows) for i in range(cols)]
	return mesh_obj(name, vs, faces, m)


def wall_coords(g):
	# (x + y, z): one running coordinate round all four walls, so a pattern wraps the corners unbroken.
	tc = g.node("ShaderNodeTexCoord")
	sep = g.node("ShaderNodeSeparateXYZ")
	g.link(tc.outputs["Object"], sep.inputs[0])
	comb = g.node("ShaderNodeCombineXYZ")
	g.link(g.math('ADD', sep.outputs[0], sep.outputs[1]), comb.inputs[0])
	g.link(sep.outputs[2], comb.inputs[1])
	return comb.outputs[0], sep


def _overlay(m, g, fac, color, dent, tint=None):
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.bl_idname == "ShaderNodeBsdfPrincipled")
	src = bsdf.inputs["Base Color"].links[0].from_socket
	if tint is not None:
		src = g.mix(1.0, src, tint, 'MULTIPLY')
	g.link(g.mix(fac, src, color), bsdf.inputs["Base Color"])
	if dent > 0.0:
		bn = next(n for n in nt.nodes if n.bl_idname == "ShaderNodeBump")
		h = bn.inputs["Height"].links[0].from_socket
		g.link(g.math('SUBTRACT', h, g.math('MULTIPLY', fac, dent)), bn.inputs["Height"])
	return m


def bricks(name, color, mortar, width, height, offset=0.5, joint=0.012, vary=0.12, dent=0.8, **kw):
	# Bricks or tiles: the per-unit tone varies, joints are sunk and painted the mortar colour.
	if bpy.data.materials.get(name) is not None:
		return bpy.data.materials[name]
	m = mat.make(name, color, **kw)
	g = mat.Graph(m)
	co, _ = wall_coords(g)
	br = g.node("ShaderNodeTexBrick", offset=offset, offset_frequency=2, squash=1.0, squash_frequency=2)
	g.link(co, br.inputs["Vector"])
	br.inputs["Scale"].default_value = 1.0
	br.inputs["Mortar Size"].default_value = joint
	br.inputs["Mortar Smooth"].default_value = 0.2
	br.inputs["Brick Width"].default_value = width
	br.inputs["Row Height"].default_value = height
	br.inputs["Color1"].default_value = (1.0, 1.0, 1.0, 1.0)
	br.inputs["Color2"].default_value = (1.0 - vary, 1.0 - vary, 1.0 - vary * 1.2, 1.0)
	br.inputs["Mortar"].default_value = (1.0, 1.0, 1.0, 1.0)
	return _overlay(m, g, br.outputs["Fac"], mat.col(mortar), dent, br.outputs["Color"])


def ribs(name, color, period, groove=0.55, dent=0.6, axis=2, soft=(0.35, 0.95), color2=None, **kw):
	# Ribbing along one axis (z: roll shutters, blinds; x: awning stripes, with color2 and hard edges).
	if bpy.data.materials.get(name) is not None:
		return bpy.data.materials[name]
	m = mat.make(name, color, **kw)
	g = mat.Graph(m)
	_, sep = wall_coords(g)
	wave = g.math('SINE', g.math('MULTIPLY', sep.outputs[axis], math.tau / period))
	fac = g.ramp(wave, soft[0], soft[1])
	c = mat.col(color)
	c2 = mat.col(color2) if color2 is not None else (c[0] * groove, c[1] * groove, c[2] * groove, 1.0)
	return _overlay(m, g, fac, c2, dent)


def panel_sign(face, u0, u1, z0, z1, n0, depth, panel_m, body=None, size=0.4, letter_m=None, du=0.0, name="sign"):
	# A box sign on the wall with its lettering; the face against the wall and the underside are dropped.
	w = u1 - u0
	back = -face.n
	box = face.box((w, depth, z1 - z0), (u0 + u1) * 0.5, n0 + depth * 0.5, (z0 + z1) * 0.5, panel_m, name, bevel=0.02)
	parts = [drop_faces(box, lambda nn: nn.dot(back) > 0.9 or nn.z < -0.9)]
	if body:
		parts.append(face.text(body, size, (u0 + u1) * 0.5 + du, n0 + depth + 0.004, (z0 + z1) * 0.5, letter_m))
	return parts


def window(face, u, z0, w, h, frame_m, glass_m, kind="glass", sill_m=None, grille_m=None, split=True, rnd=None, dark_m=None,
		   d=0.06, f=0.06):
	# An aluminium window set on the wall: frame, pane(s), an optional sill and 방범창 grille.
	# kind: glass, broken (dark room behind jagged shards), cracked.
	parts = [face.frame(u, z0, w, h, f, d, frame_m)]
	if kind == "broken":
		parts.append(lowres(face.quad(u - w / 2, u + w / 2, z0, z0 + h, 0.008, dark_m), 0.25))
		parts.append(face.shards(u - w / 2, u + w / 2, z0, z0 + h, 0.012, glass_m, rnd or geo.rng(1), 3))
	else:
		parts.append(lowres(face.quad(u - w / 2, u + w / 2, z0, z0 + h, 0.01, glass_m), 0.5))
		if kind == "cracked":
			parts.append(face.cracks(u + w * 0.15, z0 + h * 0.55, 0.014, frame_m, rnd or geo.rng(2), 6, min(w, h) * 0.45))
	if split:
		parts.append(drop_faces(face.box((0.05, 0.05, h), u, 0.035, z0 + h / 2, frame_m, "mullion"), lambda nn: nn.dot(-face.n) > 0.9))
	if sill_m is not None:
		parts.append(drop_faces(face.box((w + 2 * f + 0.1, 0.12, 0.05), u, 0.06, z0 - f - 0.025, sill_m, "sill"), lambda nn: nn.z < -0.9 or nn.dot(-face.n) > 0.9))
	if grille_m is not None:
		parts += grille(face, u, z0, w, h, grille_m, d + 0.05)
	return parts


def grille(face, u, z0, w, h, m, n, gap=0.13):
	# Security bars (방범창): flat bars on two rails, in one mesh.
	vs = []
	faces = []
	count = max(2, int(w / gap))
	bw = 0.03
	for k in range(count + 1):
		x = u - w / 2 + w * k / count
		b = len(vs)
		vs += [(x - bw / 2, z0 - 0.03, n), (x + bw / 2, z0 - 0.03, n), (x + bw / 2, z0 + h + 0.03, n), (x - bw / 2, z0 + h + 0.03, n)]
		faces.append((b, b + 1, b + 2, b + 3))
	for zz in (z0 + 0.05, z0 + h - 0.05):
		b = len(vs)
		vs += [(u - w / 2 - 0.04, zz - 0.022, n + 0.01), (u + w / 2 + 0.04, zz - 0.022, n + 0.01), (u + w / 2 + 0.04, zz + 0.022, n + 0.01), (u - w / 2 - 0.04, zz + 0.022, n + 0.01)]
		faces.append((b, b + 1, b + 2, b + 3))
	return [face.mesh(vs, faces, m, "grille")]


def ac_unit(loc, rz, body_m, fan_m, metal_m, bracket=True, name="ac"):
	# An outdoor AC unit, its back at loc (bottom centre) and its face turned to rz; on angle brackets if it hangs.
	f = Face(rz, 0.0)
	base = Vector(loc)
	def at(u, n, z):
		return tuple(base + f.r * u + f.n * n + Vector((0, 0, z)))
	w, d, h = 0.80, 0.30, 0.56
	parts = [geo.box((w, d, h), at(0.0, d / 2, h / 2), (0, 0, rz), mat=body_m, name=name, bevel=0.02, bevel_segments=1)]
	parts.append(geo.cylinder(0.2, 0.02, at(-0.12, d, h / 2), (90, 0, rz), mat=fan_m, segments=8, name="ac_fan"))
	parts.append(geo.box((0.18, 0.012, 0.36), at(0.24, d + 0.004, h / 2), (0, 0, rz), mat=fan_m, name="ac_vent"))
	if bracket:
		for s in (-0.28, 0.28):
			parts.append(geo.box((0.04, d + 0.06, 0.04), at(s, d / 2 - 0.03, -0.02), (0, 0, rz), mat=metal_m, name="ac_bracket"))
			parts.append(geo.box((0.035, 0.035, 0.42), at(s, d * 0.45, -0.2), (-35, 0, rz), mat=metal_m, name="ac_strut"))
	return parts


def moss_pad(radii, loc, rot, m, seed, segments=12):
	# A soft cushion: domed, edges rolled under, lumped by noise (gently in height, so a flat pad stays whole).
	pad = geo.lathe([(0.92, -0.2), (1.0, 0.3), (0.62, 0.85), (0.0, 1.0)], loc, rot, mat=m, segments=segments,
					name="moss_pad", close_bottom=False)
	pad.scale = radii
	geo.apply_transform(pad)
	return geo.jitter(pad, min(radii[0], radii[1]) * 0.25, 2.6, seed=seed, axes=(1.0, 1.0, min(1.0, radii[2] / min(radii[0], radii[1]))))


def sweep(name, points, profile, m, scales=None, smooth=True):
	# Sweeps a 2D (side, up) profile along a path; the side points to the right of travel, profiles run clockwise.
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
		for k in range(n - 1):
			faces.append((i * n + k, i * n + k + 1, (i + 1) * n + k + 1, (i + 1) * n + k))
	return mesh_obj(name, verts, faces, m, smooth)


def moss_strip(p0, p1, width, height, m, seed, droop=0.1, segs=6, name="moss_strip"):
	# Moss along a ledge from p0 to p1 (outside on the right of travel), thin at the ends, spilling over the edge.
	a, b = Vector(p0), Vector(p1)
	rnd = geo.rng(seed)
	pts = []
	sc = []
	for k in range(segs + 1):
		t = k / float(segs)
		pts.append(a + (b - a) * t + Vector((0, 0, rnd.uniform(-0.01, 0.01))))
		sc.append(0.35 + 0.65 * math.sin(math.pi * t) ** 0.6)
	prof = [(-width, -0.03), (-width * 0.6, height * 0.75), (0.0, height), (width * 0.65, height * 0.8), (width + 0.03, -0.02), (width + 0.04, -droop)]
	strip = sweep(name, pts, prof, m, sc)
	return geo.jitter(strip, min(width, 0.08) * 0.5, 2.5, seed=seed)


def leaf_card(p, out, size, m, rnd, droop=0.0):
	# A small cupped leaf standing off a wall along `out`, tipped down by droop.
	out = Vector(out).normalized()
	up = Vector((0, 0, 1))
	side = out.cross(up).normalized()
	along = (up * (1.0 - droop) - up * droop * 1.6 + side * rnd.uniform(-0.6, 0.6) + out * 0.3).normalized()
	nrm = side.cross(along).normalized()
	if nrm.dot(out) < 0.0:
		nrm = -nrm
	p = Vector(p)
	c = p + along * size * 0.45 + nrm * size * 0.12
	vs = [p, p + along * size * 0.4 + side * size * 0.38, p + along * size, p + along * size * 0.4 - side * size * 0.38, c]
	obj = mesh_obj("leaf", [tuple(v) for v in vs], [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4)], m, True)
	for f in obj.data.polygons:
		if Vector(f.normal).dot(nrm) < 0.0:
			f.flip()
	return obj


def vine_curtain(face, u0, u1, z_top, drops, n0, m, seed, rows=4, name="vines"):
	# Vines hanging from a parapet: a ragged leafy sheet, each column falling to its own length.
	cols = len(drops) - 1
	off = Vector((seed * 3.1, seed * 1.7, 0.0))
	vs = []
	for i in range(cols + 1):
		u = u0 + (u1 - u0) * i / cols
		for j in range(rows + 1):
			t = j / float(rows)
			z = z_top - drops[i] * t
			wob = noise.noise(Vector((u * 0.9, z * 0.7, 0.0)) + off) * 0.05
			vs.append((u + wob * 0.6, z, n0 + 0.03 + wob))
	faces = []
	for i in range(cols):
		for j in range(rows):
			a, b = i * (rows + 1) + j, (i + 1) * (rows + 1) + j
			faces.append((a, a + 1, b + 1, b))
	return face.mesh(vs, faces, m, name)


def weeds(spots, mats, rnd, count=4, height=(0.2, 0.45), sway=True):
	# Grass and weed tufts; only those near the ground sway (the game's sway grows with height).
	out = []
	for (cx, cy, cz) in spots:
		for k in range(count):
			h = rnd.uniform(*height)
			w = geo.cone(rnd.uniform(0.04, 0.075), h, (cx + rnd.uniform(-0.18, 0.18), cy + rnd.uniform(-0.12, 0.12), cz + h * 0.5),
						 (rnd.uniform(-16, 16), rnd.uniform(-16, 16), 0), mat=mats[k % len(mats)], segments=4, name="weed")
			if sway and cz < 0.6:
				geo.tag(w, "sway", "foliage")
			else:
				geo.tag(w, "foliage")
			out.append(w)
	return out


def flight(x0, x1, z0, z1, y_out, width, steps, m, name="flight"):
	# One straight run of steel stairs in the XZ plane, rising from x0 to x1, as a sawtooth slab.
	sx = 1.0 if x1 > x0 else -1.0
	run = abs(x1 - x0) / steps
	rise = (z1 - z0) / steps
	pts = [(x0, z0)]
	for k in range(steps):
		x = x0 + sx * run * k
		z = z0 + rise * (k + 1)
		pts += [(x, z), (x + sx * run, z)]
	pts += [(x1, z1 - 0.18), (x0 + sx * run * 0.4, z0 - 0.04)]
	if sx < 0:
		pts.reverse()
	slab = geo.polygon_slab(pts, width, (0.0, y_out, 0.0), (90, 0, 0), mat=m, name=name)
	return slab


def finish(name, parts, size=1024, margin=0.002, small=0.8, small_share=0.5):
	# pipeline.static, but a building has some two thousand islands: small single-coloured parts (and those
	# marked with lowres) get their islands shrunk, then everything is repacked with a fixed pixel margin,
	# so the walls and signs keep most of the sheet.
	for p in parts:
		p = geo.apply_modifiers(p)
		s = p.get("uvs")
		if s is None:
			s = small_share if sum(f.area for f in p.data.polygons) < small else 1.0
		a = p.data.attributes.new("uvs", 'FLOAT', 'FACE')
		a.data.foreach_set("value", [float(s)] * len(p.data.polygons))
	low = geo.join(parts, name)
	low = geo.triangulate(low)
	bake.unwrap(low, margin=margin)
	me = low.data
	share = [d.value for d in me.attributes["uvs"].data]
	uv = me.uv_layers.active.data
	for poly in me.polygons:
		s = share[poly.index]
		if s != 1.0:
			for li in poly.loop_indices:
				uv[li].uv = uv[li].uv * s
	me.attributes.remove(me.attributes["uvs"])
	core.set_mode(low, 'EDIT')
	bpy.ops.mesh.select_all(action='SELECT')
	bpy.ops.uv.select_all(action='SELECT')
	bpy.ops.uv.pack_islands(rotate=True, shape_method='CONCAVE', margin_method='ADD', margin=margin)
	bpy.ops.object.mode_set(mode='OBJECT')
	files = bake.bake(low, None, size, name)
	return dict(mesh=low, files=files, sources=None)


HX, HY = 5.0, 4.0
INSET = 0.12
FLOORS = (3.4, 6.25, 9.1)
ROOF = 11.95
TOP = 12.5
PANES = (-4.55, -3.25, -1.95, -0.65, 0.65, 1.95, 3.25, 4.55)


def build():
	tile = bricks("office_tile", (0.60, 0.55, 0.46), (0.72, 0.69, 0.62), 0.30, 0.15, offset=0.0, pattern="plaster", rough=0.8, bump=0.3)
	band = mat.make("office_band", (0.45, 0.43, 0.39), pattern="concrete", rough=0.9, bump=0.3)
	side = mat.make("office_side", (0.50, 0.49, 0.46), pattern="plaster", rough=0.92, bump=0.3)
	granite = mat.make("office_granite", (0.30, 0.30, 0.30), pattern="stone", rough=0.8, bump=0.4)
	roof_m = mat.make("office_roof", (0.40, 0.41, 0.39), pattern="concrete", rough=0.95, bump=0.3)
	alu = mat.make("office_alu", (0.60, 0.61, 0.60), rough=0.5, bump=0.1, grime=0.45)
	glass = mat.make("office_glass", (0.16, 0.20, 0.23), pattern="plaster", var=0.3, rough=0.2, grime=0.15, edge=0.05, bump=0.05)
	blind = ribs("office_blind", (0.52, 0.51, 0.46), 0.06, groove=0.8, dent=0.2, rough=0.8, grime=0.3, edge=0.0)
	dark = mat.make("office_dark", (0.05, 0.045, 0.04), rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	room = mat.make("office_room", (0.12, 0.11, 0.10), pattern="concrete", rough=0.95, grime=0.6, edge=0.0)
	shutter_m = ribs("office_shutter", (0.50, 0.50, 0.47), 0.09, groove=0.6, pattern="rust", amount=0.35, rough=0.7)
	paper = mat.make("office_paper", (0.78, 0.76, 0.68), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	ink_red = mat.make("office_ink_red", (0.55, 0.22, 0.18), rough=0.9, bump=0.0, edge=0.0)
	steel = mat.make("office_steel", (0.30, 0.31, 0.30), pattern="rust", accent="rust", amount=1.1, rough=0.75, bump=0.4)
	stair_m = mat.make("office_stair", (0.36, 0.26, 0.20), pattern="rust", accent="rust_dark", amount=1.2, rough=0.85, bump=0.45)
	rust = mat.get("rust")
	door_m = mat.make("office_door", (0.32, 0.40, 0.38), pattern="rust", amount=0.9, rough=0.7, bump=0.35)
	ac_m = mat.make("office_ac", (0.66, 0.66, 0.62), pattern="rust", amount=0.3, rough=0.6, bump=0.2, grime=0.5)
	fan_m = mat.make("office_fan", (0.20, 0.20, 0.20), rough=0.7, bump=0.1)
	gas = mat.make("office_gas", (0.62, 0.52, 0.22), pattern="rust", amount=0.5, rough=0.6, bump=0.2)
	pipe_m = mat.make("office_pipe", (0.56, 0.56, 0.53), rough=0.6, bump=0.1, grime=0.5)
	tank_m = bricks("office_tank", (0.32, 0.42, 0.47), (0.24, 0.31, 0.35), 0.6, 0.6, offset=0.0, joint=0.035, vary=0.05, dent=1.2,
					pattern="plaster", rough=0.6, bump=0.25)
	stone = mat.get("stone")
	# Sign faces and their lettering.
	cream = mat.make("sign_cream", (0.74, 0.71, 0.62), pattern="rust", accent=(0.62, 0.60, 0.53), amount=0.5, rough=0.6, bump=0.15)
	white = mat.make("sign_white", (0.80, 0.79, 0.74), pattern="rust", accent=(0.66, 0.65, 0.60), amount=0.45, rough=0.6, bump=0.15)
	green_l = mat.make("sign_green_letter", (0.18, 0.42, 0.30), rough=0.6, bump=0.05, edge=0.05)
	blue_p = mat.make("sign_blue", (0.20, 0.32, 0.47), pattern="rust", accent=(0.30, 0.38, 0.46), amount=0.5, rough=0.6, bump=0.15)
	blue_l = mat.make("sign_blue_letter", (0.18, 0.30, 0.50), rough=0.6, bump=0.05, edge=0.05)
	cream_l = mat.make("sign_cream_letter", (0.84, 0.82, 0.74), rough=0.6, bump=0.05, edge=0.05)
	red_p = mat.make("sign_red", (0.48, 0.27, 0.22), pattern="rust", accent=(0.52, 0.40, 0.34), amount=0.8, rough=0.7, bump=0.2)
	green_p = mat.make("sign_green", (0.22, 0.38, 0.30), pattern="rust", accent=(0.34, 0.42, 0.37), amount=0.6, rough=0.65, bump=0.15)
	plum_p = mat.make("sign_plum", (0.40, 0.23, 0.32), pattern="rust", accent=(0.44, 0.34, 0.38), amount=0.55, rough=0.65, bump=0.15)
	yellow_l = mat.make("sign_yellow_letter", (0.82, 0.74, 0.48), rough=0.6, bump=0.05, edge=0.05)
	navy_p = mat.make("sign_navy", (0.17, 0.25, 0.40), pattern="rust", accent=(0.27, 0.33, 0.42), amount=0.5, rough=0.6, bump=0.15)
	brown_l = mat.make("sign_brown_letter", (0.50, 0.22, 0.17), rough=0.6, bump=0.05, edge=0.05)
	plate_m = mat.make("plate_granite", (0.22, 0.22, 0.23), pattern="stone", rough=0.5, bump=0.2)
	brass = mat.make("plate_brass", (0.64, 0.55, 0.34), rough=0.45, bump=0.05, edge=0.1)
	banner_m = mat.make("office_banner", (0.56, 0.38, 0.30), pattern="cloth", rough=0.95, bump=0.3)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	ivy = mat.make("office_ivy", (0.20, 0.33, 0.20), pattern="leaf", rough=0.8, bump=0.45)
	bark = mat.get("bark")
	glow = mat.get("spore_glow")
	rnd = geo.rng(41)

	parts = []
	shell = []

	def add(objs, mossy=False):
		if not isinstance(objs, (list, tuple)):
			objs = [objs]
		parts.extend(objs)
		if mossy:
			shell.extend(objs)
		return objs

	front = Face(0, HY - INSET, HX - INSET)
	right = Face(90, HX - INSET, HY - INSET)
	back = Face(180, HY - INSET, HX - INSET)
	left = Face(270, HX - INSET, HY - INSET)

	# ---- shell: bands round every floor, wall planes set back between them, corner piers -------------------------
	add(ring(-HX - 0.04, HX + 0.04, -HY - 0.04, HY + 0.04, 0.0, 0.2, 0.16, granite, "plinth"), True)
	add(ring(-HX, HX, -HY, HY, 2.7, FLOORS[0], INSET, band, "fascia", top=False), True)
	for k, f in enumerate(FLOORS):
		add(ring(-HX, HX, -HY, HY, f, f + 0.95, INSET, tile, "spandrel"), True)
		if k < len(FLOORS) - 1:
			add(ring(-HX, HX, -HY, HY, f + 2.55, FLOORS[k + 1], INSET, band, "head", top=False), True)
	add(ring(-HX, HX, -HY, HY, FLOORS[-1] + 2.55, TOP, 0.22, band, "parapet", inner=ROOF), True)
	add(flat_grid(-HX + 0.22, HX - 0.22, -HY + 0.22, HY - 0.22, ROOF, 5, 4, roof_m, "roof"), True)

	for fc in (right, back, left):
		add(fc.wall(-fc.half, fc.half, 0.2, 2.7, side, cols=3, name="wall"), True)
		for f in FLOORS:
			add(fc.wall(-fc.half, fc.half, f + 0.95, f + 2.55, side, cols=3, name="wall"), True)
	for (sx, sy) in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
		add(corner_pier(sx, sy, HX, HY, 0.3, INSET, 0.2, 2.7, tile), True)
		for f in FLOORS:
			add(corner_pier(sx, sy, HX, HY, 0.45, INSET, f + 0.95, f + 2.55, tile), True)
	for (u0, u1) in ((-1.6, -1.4), (-0.1, 0.1), (2.6, 2.8)):
		add(drop_faces(front.box((u1 - u0, INSET, 2.5), (u0 + u1) * 0.5, INSET * 0.5, 1.45, tile, "pier"), lambda nn: abs(nn.z) > 0.9 or nn.y > 0.9), True)

	# ---- ground floor: pharmacy, the stair entrance, the estate agent, the shuttered laundry --------------------
	# Pharmacy: glass front in aluminium, shelves and posters seen on the glass, a double door.
	add(lowres(front.quad(-4.7, -1.6, 0.2, 2.7, 0.0, glass), 0.5))
	add(front.box((3.1, 0.05, 0.28), -3.15, 0.03, 0.34, alu, "kick"))
	add(front.box((3.1, 0.06, 0.06), -3.15, 0.03, 2.25, alu, "transom"))
	for u in (-4.67, -3.65, -2.65, -1.63):
		add(drop_faces(front.box((0.06, 0.06, 2.5), u, 0.03, 1.45, alu, "mullion"), lambda nn: nn.y > 0.9 or abs(nn.z) > 0.9))
	for u in (-3.22, -3.08):
		add(front.box((0.03, 0.04, 0.5), u, 0.08, 1.15, alu, "handle"))
	for z in (0.95, 1.35, 1.75):
		add(front.quad(-4.55, -3.75, z, z + 0.05, 0.004, mat.get("wood_grey"), "shelf"))
		add(front.quad(-2.55, -1.75, z, z + 0.05, 0.004, mat.get("wood_grey"), "shelf"))
	add(front.quad(-4.45, -3.95, 1.95, 2.18, 0.005, paper, "poster"))
	add(front.quad(-2.4, -1.85, 1.3, 2.0, 0.005, paper, "poster"))
	add(front.quad(-2.36, -1.89, 1.78, 1.94, 0.006, ink_red, "poster_band"))

	# Stair entrance: the door gone, the stairs rising into the dark; one glass leaf swung out.
	add(front.frame(-0.75, 0.2, 1.1, 2.25, 0.08, 0.1, alu))
	add(lowres(front.hole(-1.3, -0.2, 0.2, 2.45, 2.1, room), 0.25))
	for k in range(6):
		add(lowres(drop_faces(front.box((1.08, 0.3, 0.2 * (k + 1)), -0.75, -0.45 - 0.3 * k, 0.2 + 0.1 * (k + 1), stone, "step"),
							  lambda nn: nn.z < -0.9 or abs(nn.x) > 0.9 or nn.y > 0.9), 0.3))
	add(front.quad(-1.4, -0.1, 2.45, 2.7, 0.0, band, "lintel"))
	leaf_parts = [front.box((0.5, 0.03, 2.0), -1.03, 0.05, 1.22, glass, "door_leaf"), front.box((0.05, 0.05, 2.06), -0.78, 0.05, 1.22, alu, "door_stile"),
				  front.box((0.5, 0.05, 0.08), -1.03, 0.05, 0.26, alu, "door_rail")]
	turn(leaf_parts, front.at(-1.28, 0.05, 0.0), (0, 0, 1), -62)
	add(leaf_parts)

	# Estate agent: glass front plastered with listing sheets, the door glass cracked.
	add(lowres(front.quad(0.1, 2.6, 0.2, 2.7, 0.0, glass), 0.5))
	add(front.box((2.5, 0.05, 0.28), 1.35, 0.03, 0.34, alu, "kick"))
	add(front.box((2.5, 0.06, 0.06), 1.35, 0.03, 2.25, alu, "transom"))
	for u in (0.13, 1.4, 2.57):
		add(drop_faces(front.box((0.06, 0.06, 2.5), u, 0.03, 1.45, alu, "mullion"), lambda nn: nn.y > 0.9 or abs(nn.z) > 0.9))
	add(front.box((0.03, 0.04, 0.5), 1.55, 0.08, 1.15, alu, "handle"))
	for i in range(3):
		for j in range(3):
			u = 0.36 + i * 0.36
			z = 1.12 + j * 0.36
			add(front.quad(u - 0.14, u + 0.14, z - 0.15, z + 0.15, 0.005, paper, "listing"))
	add(front.cracks(2.05, 1.45, 0.006, cream_l, rnd, 7, 0.55))

	# Laundry: shutter down, a for-let notice taped to it.
	add(front.box((1.9, 0.05, 2.28), 3.75, 0.03, 1.34, shutter_m, "shutter"))
	add(front.box((1.95, 0.06, 0.06), 3.75, 0.04, 0.23, steel, "shutter_bar"))
	add(drop_faces(front.box((1.96, 0.24, 0.22), 3.75, 0.12, 2.59, steel, "shutter_box"), lambda nn: nn.z < -0.9))
	add(front.quad(3.3, 3.85, 1.0, 1.75, 0.06, paper, "notice"))
	add(front.text("임대", 0.22, 3.575, 0.064, 1.52, ink_red))
	add(front.text("832-1170", 0.085, 3.575, 0.064, 1.2, ink_red, limit=200))
	add(front.quad(4.3, 4.34, 0.6, 2.3, 0.058, rust, "streak"))

	# Fascia signs.
	add(panel_sign(front, -4.92, -1.5, 2.76, 3.34, INSET, 0.14, cream, "약국", 0.42, green_l, du=0.45))
	for (w, h) in ((0.36, 0.11), (0.11, 0.36)):
		add(front.box((w, 0.012, h), -4.05, INSET + 0.146, 3.05, green_l, "cross"))
	add(panel_sign(front, -1.38, -0.12, 2.8, 3.3, INSET, 0.05, plate_m, "새빛빌딩", 0.25, brass))
	add(panel_sign(front, 0.06, 2.64, 2.76, 3.34, INSET, 0.14, blue_p, "부동산", 0.42, cream_l))
	add(panel_sign(front, 2.82, 4.94, 2.76, 3.34, INSET, 0.14, red_p, "세탁", 0.40, cream_l, du=-0.3))
	add(front.quad(4.12, 4.52, 2.85, 3.25, INSET + 0.144, cream, "ghost"))

	# The pharmacy's green cross light box, standing out from the corner.
	add(geo.box((0.12, 0.62, 0.62), (-4.80, -HY - 0.31, 3.93), mat=white, name="cross_box"))
	for sx in (-1.0, 1.0):
		for (dy, dz) in ((0.42, 0.13), (0.13, 0.42)):
			add(geo.box((0.012, dy, dz), (-4.80 + sx * 0.066, -HY - 0.31, 3.93), mat=green_l, name="cross"))

	# ---- upper floors: tenants' signs on the spandrels, band windows ------------------------------------------
	add(panel_sign(front, -4.3, 1.3, 3.48, 4.27, INSET, 0.12, white, "치과의원", 0.5, blue_l))
	banner = mesh_obj("banner", [front.at(1.75 + 2.5 * i / 6.0, INSET + 0.03, z - 0.12 * math.sin(math.pi * i / 6.0)) for z in (3.62, 4.18) for i in range(7)],
					  [(i, i + 1, 8 + i, 7 + i) for i in range(6)], banner_m)
	add(banner)
	add(front.quad(1.9, 4.1, 3.84, 3.95, INSET + 0.035, cream, "banner_band"))

	tilted = panel_sign(front, -3.4, 0.8, 6.33, 7.12, INSET, 0.12, green_p, "당구장", 0.5, cream_l, du=0.35)
	for (u, m) in ((-2.95, mat.get("paint_red")), (-2.55, white)):
		tilted.append(geo.cylinder(0.15, 0.02, front.at(u, INSET + 0.13, 6.72), (90, 0, 0), mat=m, segments=10, name="ball"))
	turn(tilted, front.at(0.8, INSET, 7.12), front.n, 4.0)
	add(tilted)
	add(front.box((0.06, 0.12, 0.1), 0.6, INSET + 0.06, 7.06, steel, "bracket"))

	add(panel_sign(front, -0.8, 3.8, 9.18, 9.97, INSET, 0.12, plum_p, "노래방", 0.5, yellow_l))

	kinds = {FLOORS[0]: "gbggbgg", FLOORS[1]: "xtxgxcx", FLOORS[2]: "bggbgxg"}
	for f in FLOORS:
		z0, z1 = f + 0.95, f + 2.55
		add(front.box((9.1, 0.07, 0.06), 0.0, 0.035, z1 - 0.03, alu, "head_strip"))
		for k, kind in enumerate(kinds[f]):
			u0, u1 = PANES[k], PANES[k + 1]
			if kind in "xt":
				add(lowres(front.hole(u0, u1, z0, z1 - 0.06, 0.55, room), 0.25))
				add(front.shards(u0, u1, z0, z1 - 0.06, 0.0, glass, rnd, 2 if kind == "t" else 3))
			else:
				add(lowres(front.quad(u0, u1, z0, z1, 0.0, blind if kind == "b" else glass), 0.5))
				if kind == "c":
					add(front.cracks((u0 + u1) * 0.5, (z0 + z1) * 0.5, 0.004, cream_l, rnd, 6, 0.6))
		for u in PANES[1:-1]:
			add(drop_faces(front.box((0.07, 0.07, 1.6), u, 0.035, (z0 + z1) * 0.5, alu, "mullion"), lambda nn: nn.y > 0.9 or abs(nn.z) > 0.9))

	# The vertical sign down the front right corner, lettered on both sides.
	for (z0, z1, pm, lm, chars, step) in ((3.6, 7.45, cream, brown_l, "영어학원", 0.86), (7.6, 11.4, navy_p, cream_l, "태권도", 1.12)):
		add(geo.box((0.2, 0.7, z1 - z0), (4.72, -HY - 0.35, (z0 + z1) * 0.5), mat=pm, name="vsign", bevel=0.02, bevel_segments=1))
		for (fc, u) in ((Face(90, 4.82), -HY - 0.35), (Face(270, -4.62), HY + 0.35)):
			for k, ch in enumerate(chars):
				z = (z0 + z1) * 0.5 + (len(chars) - 1) * step * 0.5 - k * step
				add(fc.text(ch, 0.5, u, 0.004, z, lm, name="vletter"))
	for z in (4.2, 7.52, 10.8):
		add(geo.box((0.08, 0.2, 0.08), (4.72, -HY - 0.05, z), mat=steel, name="vsign_bracket"))

	# ---- right side --------------------------------------------------------------------------------------------
	add(window(right, 1.6, 1.5, 1.2, 0.8, alu, glass, grille_m=steel, rnd=rnd))
	add(right.box((0.4, 0.16, 0.55), -1.0, 0.08, 1.5, ac_m, "meter_box"))
	for k, f in enumerate(FLOORS):
		add(window(right, -1.9, f + 1.05, 1.6, 1.3, alu, glass, kind="broken" if k == 1 else "glass", sill_m=band, rnd=rnd, dark_m=dark))
		add(window(right, 1.5, f + 1.05, 1.2, 1.3, alu, blind if k == 2 else glass, sill_m=band, rnd=rnd, dark_m=dark))
	add(ac_unit(right.at(-0.2, INSET, 3.5), 90, ac_m, fan_m, steel))
	add(geo.tube([(HX + 0.07, 3.72, 12.3), (HX + 0.07, 3.72, 0.25), (HX + 0.2, 3.72, 0.08)], 0.05, mat=pipe_m, segments=6, name="drain"))
	add(geo.box((0.2, 0.2, 0.2), (HX + 0.08, 3.72, 12.35), mat=pipe_m, name="hopper"))

	# ---- left side ---------------------------------------------------------------------------------------------
	add(window(left, -2.2, 1.6, 0.8, 0.7, alu, glass, grille_m=steel, split=False, rnd=rnd))
	add(left.text("주차금지", 0.42, 1.0, 0.004, 1.3, mat.make("office_paint_red", (0.52, 0.30, 0.26), pattern="plaster", rough=0.9, bump=0.05, edge=0.0)))
	for k, f in enumerate(FLOORS):
		add(window(left, 1.9, f + 1.05, 1.6, 1.3, alu, glass if k != 1 else blind, sill_m=band, rnd=rnd, dark_m=dark))
		add(window(left, -1.6, f + 1.05, 1.2, 1.3, alu, glass, kind="broken" if k == 1 else "glass", sill_m=band, rnd=rnd, dark_m=dark))

	# ---- back: doors, windows, AC units, the gas pipe and the steel fire stairs ---------------------------------
	add(back.frame(3.2, 0.2, 0.95, 2.1, 0.07, 0.07, steel))
	add(back.box((0.95, 0.04, 2.1), 3.2, 0.03, 1.25, door_m, "back_door"))
	add(drop_faces(back.box((1.4, 0.45, 0.16), 3.2, 0.22, 0.08, mat.get("concrete"), "step"), lambda nn: nn.z < -0.9))
	add(window(back, 1.2, 1.6, 0.9, 0.7, alu, glass, grille_m=steel, split=False, rnd=rnd))
	add(back.box((0.36, 0.2, 0.46), 4.3, 0.1, 1.3, ac_m, "gas_meter"))
	add(geo.tube([(-4.3, HY - INSET + 0.06, 0.2), (-4.3, HY - INSET + 0.06, 1.06)], 0.03, mat=gas, segments=6, name="gas"))
	add(geo.tube([(-4.3, HY - INSET + 0.06, 1.54), (-4.3, HY - INSET + 0.06, 2.55), (-1.6, HY - INSET + 0.06, 2.55), (-1.6, HY - INSET, 2.55)], 0.03, mat=gas, segments=6, name="gas"))
	for k, f in enumerate(FLOORS):
		add(window(back, 3.7, f + 1.2, 1.0, 1.1, alu, glass, kind="broken" if k == 1 else "glass", sill_m=band, rnd=rnd, dark_m=dark))
		add(window(back, 2.3, f + 1.2, 1.0, 1.1, alu, blind if k == 0 else glass, sill_m=band, rnd=rnd, dark_m=dark))
	add(ac_unit(back.at(3.0, INSET, 3.5), 180, ac_m, fan_m, steel))
	add(ac_unit(back.at(3.0, INSET, 6.35), 180, ac_m, fan_m, steel))
	add(geo.tube([(-4.72, HY + 0.07, 12.3), (-4.72, HY + 0.07, 0.25), (-4.72, HY + 0.2, 0.08)], 0.05, mat=pipe_m, segments=6, name="drain"))
	add(geo.box((0.2, 0.2, 0.2), (-4.72, HY + 0.08, 12.35), mat=pipe_m, name="hopper"))

	# Fire stairs: three runs zigzagging up the back, landings at the floor doors.
	SY0, SY1 = HY + 0.03, HY + 0.72
	XL, XR = -0.2, 3.9
	runs = ((XR, XL, 0.0, FLOORS[0], 12), (XL, XR, FLOORS[0], FLOORS[1], 10), (XR, XL, FLOORS[1], FLOORS[2], 10))
	for (xa, xb, za, zb, n) in runs:
		add(flight(xa, xb, za, zb, SY1, SY1 - SY0, n, stair_m))
		add(geo.tube([(xa, SY1 - 0.03, za + 0.95), (xb, SY1 - 0.03, zb + 0.95)], 0.025, mat=steel, segments=4, name="rail"))
		for x in (xa, xb):
			zz = za if x == xa else zb
			add(geo.box((0.04, 0.04, 0.95), (x, SY1 - 0.03, zz + 0.47), mat=steel, name="post"))
	for (x0, x1, z) in ((-1.2, XL, FLOORS[0]), (XR, 4.9, FLOORS[1]), (-1.2, XL, FLOORS[2])):
		add(geo.box((x1 - x0, SY1 - SY0, 0.06), ((x0 + x1) * 0.5, (SY0 + SY1) * 0.5, z - 0.03), mat=stair_m, name="landing"))
		ox = x0 if x0 < -1.0 else x1
		add(geo.tube([(x0, SY1 - 0.03, z + 0.95), (x1, SY1 - 0.03, z + 0.95)], 0.025, mat=steel, segments=4, name="rail"))
		add(geo.tube([(ox, SY1 - 0.03, z + 0.95), (ox, SY0 + 0.02, z + 0.95)], 0.025, mat=steel, segments=4, name="rail"))
		add(geo.box((0.04, 0.04, 0.95), (ox, SY1 - 0.03, z + 0.47), mat=steel, name="post"))
		for sy in (SY0 + 0.05, SY1 - 0.05):
			add(geo.tube([((x0 + x1) * 0.5, sy, z - 0.06), ((x0 + x1) * 0.5, HY - 0.1, z - 0.45)], 0.03, mat=steel, segments=4, name="strut"))
		du = -(x0 + x1) * 0.5
		add(back.frame(du, z, 0.85, 1.95, 0.07, 0.2, steel))
		add(back.box((0.85, 0.04, 1.95), du, 0.15, z + 0.975, door_m, "fire_door"))

	# ---- roof: stair hut, water tank, AC units and the rusty name frame -----------------------------------------
	hx0, hx1, hy0, hy1 = -4.7, -2.1, 1.3, 3.7
	add(drop_faces(geo.box((hx1 - hx0, hy1 - hy0, 1.93), ((hx0 + hx1) * 0.5, (hy0 + hy1) * 0.5, ROOF + 0.965), mat=side, name="hut", bevel=0.03, bevel_segments=1),
				   lambda nn: abs(nn.z) > 0.9), True)
	add(drop_faces(geo.box((hx1 - hx0 + 0.2, hy1 - hy0 + 0.2, 0.12), ((hx0 + hx1) * 0.5, (hy0 + hy1) * 0.5, ROOF + 1.99), mat=band, name="hut_slab"), lambda nn: nn.z < -0.9), True)
	hut_front = Face(0, -hy0)
	add(hut_front.quad(-3.62, -2.78, ROOF, ROOF + 1.85, 0.003, dark, "hut_doorway"))
	add(hut_front.frame(-3.2, ROOF, 0.84, 1.85, 0.06, 0.06, steel))
	hut_door = [hut_front.box((0.84, 0.04, 1.85), -3.2, 0.03, ROOF + 0.925, door_m, "hut_door")]
	turn(hut_door, hut_front.at(-3.62, 0.03, 0.0), (0, 0, 1), -28)
	add(hut_door)
	add(window(Face(90, hx1), 2.5, ROOF + 0.9, 0.7, 0.6, alu, glass, split=False, rnd=rnd))
	add(geo.tube([(-2.6, 3.4, ROOF + 2.05), (-2.6, 3.4, ROOF + 2.2)], 0.05, mat=pipe_m, segments=6, name="vent"))

	for y in (1.95, 3.3):
		add(drop_faces(geo.box((2.0, 0.16, 0.2), (2.95, y, ROOF + 0.1), mat=steel, name="tank_beam"), lambda nn: nn.z < -0.9))
	add(drop_faces(geo.box((1.8, 1.8, 1.4), (2.95, 2.62, ROOF + 0.9), mat=tank_m, bevel=0.05, name="tank"), lambda nn: nn.z < -0.9), True)
	add(geo.cylinder(0.26, 0.08, (2.7, 2.4, ROOF + 1.64), mat=tank_m, segments=10, name="manhole"))
	add(geo.tube([(2.2, 1.7, ROOF + 0.4), (2.2, 1.2, ROOF + 0.4), (2.2, 1.2, ROOF + 0.02)], 0.05, mat=pipe_m, segments=6, name="tank_pipe"))
	add(ac_unit((-1.5, 3.55, ROOF), 0, ac_m, fan_m, steel, bracket=False))
	add(ac_unit((-0.5, 3.55, ROOF), 0, ac_m, fan_m, steel, bracket=False))
	add(geo.tube([(-1.5, 3.68, ROOF + 0.3), (-1.5, 3.85, ROOF + 0.3), (-1.5, 3.85, ROOF + 0.6)], 0.025, mat=pipe_m, segments=4, name="ac_pipe"))

	# Name frame: "새빛빌딩" on a lattice; 빛 is gone and 딩 hangs from one bolt.
	FY = -3.35
	for x in (-3.3, -1.1, 1.1, 3.3):
		add(geo.box((0.1, 0.1, 2.0), (x, FY, ROOF + 1.0), mat=rust, name="frame_post"))
		add(geo.tube([(x, FY + 0.05, ROOF + 1.5), (x, FY + 1.05, ROOF + 0.02)], 0.035, mat=rust, segments=4, name="frame_kicker"))
	for z in (ROOF + 0.62, ROOF + 1.95):
		add(geo.box((6.7, 0.08, 0.08), (0.0, FY, z), mat=rust, name="frame_rail"))
	for (xa, xb) in ((-3.3, -1.1), (-1.1, 1.1), (1.1, 3.3)):
		add(geo.tube([(xa, FY + 0.06, ROOF + 0.62), (xb, FY + 0.06, ROOF + 1.95)], 0.025, mat=rust, segments=4, name="frame_brace"))
		add(geo.tube([(xb, FY + 0.06, ROOF + 0.62), (xa, FY + 0.06, ROOF + 1.95)], 0.025, mat=rust, segments=4, name="frame_brace"))
	sign_face = Face(0, -FY + 0.05)
	for (u, ch) in ((-2.4, "새"), (0.8, "빌")):
		add(panel_sign(sign_face, u - 0.66, u + 0.66, ROOF + 0.7, ROOF + 1.88, 0.0, 0.06, white, ch, 0.95, ink_red))
	for u in (-1.3, -0.3):
		for z in (ROOF + 0.75, ROOF + 1.83):
			add(sign_face.box((0.12, 0.06, 0.06), u, 0.03, z, rust, "bolt"))
	hang = panel_sign(sign_face, 2.4 - 0.66, 2.4 + 0.66, ROOF + 0.7, ROOF + 1.88, 0.0, 0.06, white, "딩", 0.95, ink_red)
	turn(hang, sign_face.at(1.78, 0.03, ROOF + 1.84), sign_face.n, -24)
	add(hang)
	for u in (-2.9, -2.0, 0.4, 2.6):
		add(front.quad(u, u + 0.05, TOP - 0.75 - rnd.uniform(0, 0.4), TOP - 0.02, INSET + 0.003, rust, "streak"))

	# ---- nature: a tree out of the third floor, vines off the right parapet, moss, weeds -----------------------
	tz = FLOORS[1] + 0.95
	add(geo.tube([(-2.55, -3.6, tz + 0.04), (-2.6, -4.0, tz + 0.28), (-2.72, -4.22, tz + 0.95), (-2.85, -4.28, tz + 1.7)],
				 [0.13, 0.11, 0.085, 0.05], mat=bark, segments=6, name="trunk"))
	for (dx, dy) in ((0.25, -0.1), (-0.22, 0.05)):
		add(geo.tube([(-2.55, -3.62, tz + 0.06), (-2.55 + dx, -3.9 + dy, tz + 0.03), (-2.55 + dx * 1.6, -4.0, tz - 0.25)], [0.05, 0.04, 0.02], mat=bark, segments=4, name="root"))
	for (c, r, m, sub) in (((-2.9, -4.3, 9.3), (0.8, 0.5, 0.62), leaf, 2), ((-2.3, -4.25, 9.05), (0.55, 0.42, 0.45), leaf_dark, 1),
						   ((-3.45, -4.2, 8.9), (0.5, 0.4, 0.45), leaf_dark, 1), ((-2.75, -4.25, 9.85), (0.5, 0.38, 0.4), leaf, 1)):
		b = geo.blob(r, c, mat=m, subdivisions=sub, lumpy=0.3, seed=int(c[0] * 10), name="crown")
		geo.tag(b, "foliage")
		add(b)

	vine_u0, vine_u1 = 0.35, 3.95
	drops = [2.2, 3.4, 4.8, 3.9, 5.6, 6.3, 4.4, 5.2, 3.0, 4.6, 2.6]
	curtain = vine_curtain(right, vine_u0, vine_u1, TOP + 0.02, drops, INSET + 0.02, ivy, 7)
	geo.tag(curtain, "foliage")
	add(curtain)
	lip = moss_strip(right.at(vine_u0 - 0.2, INSET - 0.11, TOP), right.at(vine_u1 + 0.05, INSET - 0.11, TOP), 0.14, 0.12, ivy, 3, droop=0.25)
	geo.tag(lip, "foliage")
	add(lip)
	for i, dl in enumerate(drops):
		u = vine_u0 + (vine_u1 - vine_u0) * i / (len(drops) - 1)
		for k in range(3):
			p = right.at(u + rnd.uniform(-0.18, 0.18), INSET + 0.08, TOP - dl * rnd.uniform(0.35, 1.02))
			lf = leaf_card(p, right.n, rnd.uniform(0.14, 0.22), leaf if (i + k) % 2 else leaf_dark, rnd, droop=0.6)
			geo.tag(lf, "foliage")
			add(lf)

	ledges = [
		(front, -4.5, -0.6, FLOORS[1] + 0.95, moss), (front, 1.2, 4.5, FLOORS[2] + 0.95, moss_dark), (front, -4.4, -2.2, FLOORS[0] + 0.95, moss_dark),
		(right, -3.6, -0.8, FLOORS[1] + 0.95, moss), (right, -0.5, 3.6, FLOORS[2] + 0.95, moss_dark),
		(back, -3.0, 0.5, FLOORS[2] + 0.95, moss), (left, -3.4, -0.6, FLOORS[0] + 0.95, moss_dark),
	]
	for k, (fc, u0, u1, z, m) in enumerate(ledges):
		add(moss_strip(fc.at(u0, INSET * 0.5, z), fc.at(u1, INSET * 0.5, z), 0.09, 0.09, m, 20 + k, droop=0.14))
	tops = [(front, -4.95, -2.4, moss), (front, 2.2, 4.95, moss_dark), (right, -3.95, -1.2, moss), (back, -2.5, 2.0, moss_dark), (left, 0.5, 3.9, moss)]
	for k, (fc, u0, u1, m) in enumerate(tops):
		s = moss_strip(fc.at(u0, INSET - 0.11, TOP), fc.at(u1, INSET - 0.11, TOP), 0.13, 0.1, m, 40 + k, droop=0.16)
		if k == 0:
			geo.tag(s, "pulse")
		add(s)
	for k, (x, y, r) in enumerate(((0.8, 0.2, (1.1, 0.8, 0.09)), (-3.6, -2.4, (0.8, 0.6, 0.08)), (3.9, -1.5, (0.6, 0.9, 0.08)))):
		pad = moss_pad(r, (x, y, ROOF - 0.03), (0, 0, k * 40), moss if k != 1 else moss_dark, 60 + k)
		if k == 0:
			geo.tag(pad, "pulse")
		add(pad)
	add(moss_pad((0.6, 0.4, 0.08), (-3.4, 2.5, ROOF + 2.02), (0, 0, 10), moss, 66))
	for k, (x, y, r) in enumerate(((-HX - 0.1, -2.8, (0.22, 0.9, 0.16)), (HX + 0.1, 1.2, (0.2, 1.1, 0.15)), (-2.6, -HY - 0.12, (0.9, 0.2, 0.14)),
								   (4.2, -HY - 0.1, (0.5, 0.18, 0.12)), (-3.2, HY + 0.12, (1.0, 0.2, 0.15)))):
		add(moss_pad(r, (x, y, -0.02), (0, 0, 0), moss_dark if k % 2 else moss, 70 + k, segments=8))

	low_weeds = [(-4.95, -4.25, 0.0), (-1.5, -4.22, 0.0), (2.7, -4.25, 0.0), (4.95, -4.22, 0.0), (5.22, -2.0, 0.0), (5.2, 1.0, 0.0),
				 (5.22, 3.6, 0.0), (2.2, 4.25, 0.0), (-2.0, 4.25, 0.0), (-4.9, 4.22, 0.0), (-5.22, -1.0, 0.0), (-5.2, 2.5, 0.0)]
	add(weeds(low_weeds, (leaf, leaf_dark, moss), rnd, 4))
	high_weeds = [(-3.9, -HY + 0.06, FLOORS[1] + 0.95), (-0.1, -HY + 0.06, FLOORS[1] + 0.95), (2.6, -HY + 0.06, FLOORS[1] + 0.95),
				  (-4.6, -HY + 0.1, TOP), (4.4, HY - 0.1, TOP)]
	add(weeds(high_weeds, (leaf, leaf_dark), rnd, 3, (0.15, 0.35), sway=False))

	for (p, r) in (((-2.2, -4.5, 8.7), 0.05), ((1.4, -0.4, ROOF + 0.35), 0.05)):
		g = geo.blob(r, p, mat=glow, subdivisions=1, name="spore")
		geo.tag(g, "pulse")
		add(g)

	for i, p in enumerate(shell):
		geo.moss_mask(p, seed=i + 3)

	res = finish("office_4f", parts, size=1024)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
