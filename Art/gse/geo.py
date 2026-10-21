import bpy
import bmesh
import math
import random

from mathutils import Euler, Matrix, Vector, noise

from . import core


def _finish(bm, name, loc, rot, mat, bevel, bevel_segments, smooth):
	mesh = bpy.data.meshes.new(name)
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, mesh))
	obj.location = Vector(loc)
	obj.rotation_euler = Euler([math.radians(a) for a in rot], 'XYZ')
	if mat is not None:
		mesh.materials.append(mat)
	for p in mesh.polygons:
		p.use_smooth = smooth
	if smooth:
		mesh.set_sharp_from_angle(angle=math.radians(50.0))
	if bevel > 0.0:
		mod = obj.modifiers.new("bevel", 'BEVEL')
		mod.width = bevel
		mod.segments = bevel_segments
		mod.limit_method = 'ANGLE'
		mod.angle_limit = math.radians(40.0)
		mod.harden_normals = False
	return obj


def box(size, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, bevel=0.0, bevel_segments=2, name="box", subdiv=0):
	bm = bmesh.new()
	bmesh.ops.create_cube(bm, size=1.0)
	bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
	if subdiv > 0:
		bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=subdiv, use_grid_fill=True)
	return _finish(bm, name, loc, rot, mat, bevel, bevel_segments, False)


def cylinder(radius, height, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, segments=16, radius_top=None,
			 bevel=0.0, bevel_segments=2, name="cyl", smooth=True, caps=True):
	bm = bmesh.new()
	r2 = radius if radius_top is None else radius_top
	bmesh.ops.create_cone(bm, cap_ends=caps, cap_tris=False, segments=segments, radius1=radius, radius2=r2, depth=height)
	return _finish(bm, name, loc, rot, mat, bevel, bevel_segments, smooth)


def cone(radius, height, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, segments=16, name="cone", smooth=True):
	return cylinder(radius, height, loc, rot, mat, segments, radius_top=0.0, name=name, smooth=smooth)


def sphere(radii, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, segments=16, rings=10, name="sphere"):
	bm = bmesh.new()
	bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=1.0)
	if isinstance(radii, (int, float)):
		radii = (radii, radii, radii)
	bmesh.ops.scale(bm, vec=Vector(radii), verts=bm.verts)
	return _finish(bm, name, loc, rot, mat, 0.0, 0, True)


def blob(radii, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, subdivisions=3, name="blob", lumpy=0.0, seed=0):
	bm = bmesh.new()
	bmesh.ops.create_icosphere(bm, subdivisions=subdivisions, radius=1.0)
	if isinstance(radii, (int, float)):
		radii = (radii, radii, radii)
	if lumpy > 0.0:
		off = Vector((seed * 7.31, seed * 3.17, seed * 5.53))
		for v in bm.verts:
			n = noise.noise(v.co * 1.7 + off)
			v.co *= 1.0 + lumpy * n
	bmesh.ops.scale(bm, vec=Vector(radii), verts=bm.verts)
	return _finish(bm, name, loc, rot, mat, 0.0, 0, True)


def lathe(profile, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, segments=24, name="lathe", smooth=True, close_top=True, close_bottom=True):
	# profile: list of (radius, z) from bottom to top.
	bm = bmesh.new()
	rings = []
	for (r, z) in profile:
		ring = []
		for i in range(segments):
			a = 2.0 * math.pi * i / segments
			ring.append(bm.verts.new((math.cos(a) * r, math.sin(a) * r, z)))
		rings.append(ring)
	for k in range(len(rings) - 1):
		a, b = rings[k], rings[k + 1]
		for i in range(segments):
			j = (i + 1) % segments
			bm.faces.new((a[i], a[j], b[j], b[i]))
	if close_bottom and profile[0][0] > 1e-4:
		bm.faces.new(list(reversed(rings[0])))
	if close_top and profile[-1][0] > 1e-4:
		bm.faces.new(rings[-1])
	bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	return _finish(bm, name, loc, rot, mat, 0.0, 0, smooth)


def tube(points, radii, mat=None, segments=8, name="tube", caps=True, smooth=True, twist=0.0):
	# A swept circle along a polyline; radii per point (or one value).
	if isinstance(radii, (int, float)):
		radii = [radii] * len(points)
	pts = [Vector(p) for p in points]
	bm = bmesh.new()
	rings = []
	prev_side = None
	for i, p in enumerate(pts):
		if i == 0:
			t = (pts[1] - pts[0]).normalized()
		elif i == len(pts) - 1:
			t = (pts[-1] - pts[-2]).normalized()
		else:
			t = ((pts[i + 1] - pts[i]).normalized() + (pts[i] - pts[i - 1]).normalized()).normalized()
		if prev_side is None:
			ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
			side = t.cross(ref).normalized()
		else:
			side = (prev_side - t * prev_side.dot(t)).normalized()
		prev_side = side
		up = side.cross(t).normalized()
		ring = []
		for k in range(segments):
			a = 2.0 * math.pi * k / segments + twist * i
			ring.append(bm.verts.new(p + (side * math.cos(a) + up * math.sin(a)) * radii[i]))
		rings.append(ring)
	for k in range(len(rings) - 1):
		a, b = rings[k], rings[k + 1]
		for i in range(segments):
			j = (i + 1) % segments
			bm.faces.new((a[i], a[j], b[j], b[i]))
	if caps:
		if radii[0] > 1e-4:
			bm.faces.new(list(reversed(rings[0])))
		if radii[-1] > 1e-4:
			bm.faces.new(rings[-1])
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	return _finish(bm, name, (0, 0, 0), (0, 0, 0), mat, 0.0, 0, smooth)


def prism(size, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, name="prism", bevel=0.0):
	# Triangular prism, ridge along X at the top; size = (length x, depth y, height z).
	sx, sy, sz = size
	bm = bmesh.new()
	v = [bm.verts.new((x, y, z)) for (x, y, z) in (
		(-sx / 2, -sy / 2, 0), (-sx / 2, sy / 2, 0), (-sx / 2, 0, sz),
		(sx / 2, -sy / 2, 0), (sx / 2, sy / 2, 0), (sx / 2, 0, sz))]
	bm.faces.new((v[0], v[1], v[2]))
	bm.faces.new((v[3], v[5], v[4]))
	bm.faces.new((v[0], v[3], v[4], v[1]))
	bm.faces.new((v[0], v[2], v[5], v[3]))
	bm.faces.new((v[1], v[4], v[5], v[2]))
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	return _finish(bm, name, loc, rot, mat, bevel, 2, False)


def polygon_slab(outline, thickness, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, name="slab", bevel=0.0):
	# Extrudes a 2D outline (x, y list) upward by thickness.
	bm = bmesh.new()
	verts = [bm.verts.new((x, y, 0.0)) for (x, y) in outline]
	face = bm.faces.new(verts)
	res = bmesh.ops.extrude_face_region(bm, geom=[face])
	moved = [e for e in res["geom"] if isinstance(e, bmesh.types.BMVert)]
	bmesh.ops.translate(bm, vec=Vector((0, 0, thickness)), verts=moved)
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	return _finish(bm, name, loc, rot, mat, bevel, 2, False)


def text(body, size, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, depth=0.01, font="NanumGothic-Bold.ttf", name="text", align='CENTER'):
	# Lettering as geometry (Hangul works through the game's own Nanum font): signs, labels, badges.
	import os
	curve = bpy.data.curves.new(name, 'FONT')
	curve.body = body
	path = os.path.join(core.GAME_DIR, "Fonts", font)
	if os.path.exists(path):
		curve.font = bpy.data.fonts.load(path, check_existing=True)
	curve.size = size
	curve.extrude = depth
	curve.align_x = align
	curve.align_y = 'CENTER'
	obj = core.link(bpy.data.objects.new(name, curve))
	obj.location = Vector(loc)
	obj.rotation_euler = Euler([math.radians(a) for a in rot], 'XYZ')
	if mat is not None:
		curve.materials.append(mat)
	core.select_only([obj])
	bpy.ops.object.convert(target='MESH')
	return bpy.context.view_layer.objects.active


def grid(size_x, size_y, cuts_x, cuts_y, loc=(0, 0, 0), rot=(0, 0, 0), mat=None, name="grid"):
	bm = bmesh.new()
	bmesh.ops.create_grid(bm, x_segments=cuts_x, y_segments=cuts_y, size=0.5)
	bmesh.ops.scale(bm, vec=Vector((size_x, size_y, 1.0)), verts=bm.verts)
	return _finish(bm, name, loc, rot, mat, 0.0, 0, True)


# ---- deformation ---------------------------------------------------------------------------

def apply_transform(obj):
	core.select_only([obj])
	bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def apply_modifiers(obj):
	core.select_only([obj])
	bpy.ops.object.convert(target='MESH')
	return bpy.context.view_layer.objects.active


def jitter(obj, amount, scale=2.0, seed=0, axes=(1.0, 1.0, 1.0)):
	# Hand-made wobble: low-frequency noise pushes vertices around, so nothing is machine straight.
	apply_modifiers(obj)
	bpy.context.view_layer.update()
	mw = obj.matrix_world
	off = Vector((seed * 13.1, seed * 7.7, seed * 3.3))
	for v in obj.data.vertices:
		w = mw @ v.co
		d = Vector((noise.noise(w * scale + off), noise.noise(w * scale + off + Vector((19.1, 0, 0))),
					noise.noise(w * scale + off + Vector((0, 31.7, 0)))))
		v.co += Vector((d.x * axes[0], d.y * axes[1], d.z * axes[2])) * amount
	obj.data.update()
	return obj


def subdivide(obj, levels=1, simple=False):
	mod = obj.modifiers.new("subd", 'SUBSURF')
	mod.levels = levels
	mod.render_levels = levels
	mod.subdivision_type = 'SIMPLE' if simple else 'CATMULL_CLARK'
	return apply_modifiers(obj)


def remesh(obj, voxel=0.02, smooth_iterations=0, adaptivity=0.0):
	mod = obj.modifiers.new("remesh", 'REMESH')
	mod.mode = 'VOXEL'
	mod.voxel_size = voxel
	mod.adaptivity = adaptivity
	mod.use_smooth_shade = True
	obj = apply_modifiers(obj)
	if smooth_iterations > 0:
		mod = obj.modifiers.new("smooth", 'CORRECTIVE_SMOOTH')
		mod.iterations = smooth_iterations
		mod.smooth_type = 'SIMPLE'
		mod.use_only_smooth = True
		mod.rest_source = 'ORCO'
		obj = apply_modifiers(obj)
		smooth = obj.modifiers.new("lsmooth", 'SMOOTH')
		smooth.factor = 0.5
		smooth.iterations = max(1, smooth_iterations // 2)
		obj = apply_modifiers(obj)
	for p in obj.data.polygons:
		p.use_smooth = True
	return obj


def decimate(obj, target_tris):
	tris = sum(len(p.vertices) - 2 for p in obj.data.polygons)
	if tris <= target_tris:
		return obj
	mod = obj.modifiers.new("decimate", 'DECIMATE')
	mod.ratio = max(0.01, target_tris / float(tris))
	mod.use_collapse_triangulate = True
	return apply_modifiers(obj)


def triangulate(obj):
	mod = obj.modifiers.new("tri", 'TRIANGULATE')
	mod.quad_method = 'BEAUTY'
	mod.keep_custom_normals = True
	return apply_modifiers(obj)


def bend_down(obj, amount, axis_len):
	# Sags a long horizontal part (a gutter, a rail, a roof edge) in the middle.
	apply_modifiers(obj)
	for v in obj.data.vertices:
		t = v.co.x / max(axis_len * 0.5, 1e-4)
		v.co.z -= amount * (1.0 - t * t)
	obj.data.update()
	return obj


def join(objs, name):
	objs = [o for o in objs if o is not None]
	for o in objs:
		if o.modifiers:
			apply_modifiers(o)
	core.select_only(objs, objs[0])
	bpy.ops.object.join()
	obj = bpy.context.view_layer.objects.active
	obj.name = name
	obj.data.name = name
	apply_transform(obj)
	return obj


def duplicate(obj, name):
	copy = obj.copy()
	copy.data = obj.data.copy()
	copy.name = name
	core.link(copy)
	return copy


# ---- vertex groups: bones, shader animation flags, moss mask ---------------------------------

def tag(obj, *names, weight=1.0):
	idx = [v.index for v in obj.data.vertices]
	for n in names:
		g = obj.vertex_groups.get(n) or obj.vertex_groups.new(name=n)
		g.add(idx, weight, 'REPLACE')
	return obj


def tag_fn(obj, name, fn):
	# fn(world_co, world_normal) -> weight in [0, 1]
	g = obj.vertex_groups.get(name) or obj.vertex_groups.new(name=name)
	bpy.context.view_layer.update()
	mw = obj.matrix_world
	nm = mw.to_3x3().inverted().transposed()
	for v in obj.data.vertices:
		w = fn(mw @ v.co, (nm @ v.normal).normalized())
		if w > 0.0:
			g.add([v.index], min(1.0, w), 'REPLACE')
	return obj


def moss_mask(obj, up=1.0, low=0.6, grain=0.5, seed=0):
	# Where moss settles first: faces looking up, places near the ground, and noisy patches.
	off = Vector((seed * 3.7, seed * 1.3, 0.0))
	def fn(co, n):
		facing = max(n.z, 0.0) * up
		ground = max(0.0, 1.0 - co.z / 1.2) * low
		patch = (noise.noise(co * 1.6 + off) * 0.5 + 0.5) * grain
		return max(0.0, min(1.0, facing * 0.55 + ground * 0.45 + patch - 0.25))
	return tag_fn(obj, "moss", fn)


def rng(seed):
	return random.Random(seed)
