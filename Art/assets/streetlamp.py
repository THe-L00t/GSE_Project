# A galvanised street lamp from the city stretch of route 32: a tapered steel pole on a bolted base plate
# with an access hatch, a curved arm out over the road (-Y) holding a dark cobra-head lamp with a cracked
# lens, torn flyers banded round it at eye height, ivy up the lower pole, moss on the head and at the foot.
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from gse import core, geo, mat, pipeline

POLE_TOP = 5.75
HEAD_PIVOT = Vector((0.0, -1.37, 6.40))
TILT = -6.0


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


def floor(obj):
	# Ground pieces stop just under the ground plane, so the model's bottom stays at z = 0.
	geo.apply_transform(obj)
	for v in obj.data.vertices:
		v.co.z = max(v.co.z, -0.01)
	obj.data.update()
	return obj


def pole_radius(z):
	if z < 0.38:
		return 0.114
	return 0.114 - 0.034 * min(1.0, (z - 0.38) / (POLE_TOP - 0.38))


def on_pole(size, z, a, m, name, lift=0.0, rz=0.0):
	# A flat piece laid tangent on the pole at angle a (radians), its thin side pointing out.
	r = pole_radius(z) + size[1] * 0.5 + lift
	return geo.box(size, (math.cos(a) * r, math.sin(a) * r, z), (0, 0, math.degrees(a) - 90.0 + rz), mat=m, name=name)


def build():
	galv = mat.make("streetlamp_galv", (0.47, 0.48, 0.46), pattern="rust", rough=0.6, bump=0.3, amount=0.45)
	housing = mat.make("streetlamp_housing", (0.31, 0.33, 0.33), pattern="rust", rough=0.62, bump=0.3, amount=0.5)
	lens = mat.make("streetlamp_lens", (0.60, 0.58, 0.48), rough=0.3, bump=0.05, grime=0.5, edge=0.1)
	crack = mat.make("streetlamp_crack", (0.16, 0.16, 0.14), rough=0.5, bump=0.0, edge=0.0)
	hole = mat.make("streetlamp_hole", (0.05, 0.05, 0.05), rough=0.9, bump=0.0, edge=0.0, grime=0.0)
	paper = mat.make("streetlamp_paper", (0.66, 0.62, 0.50), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	flyer_red = mat.make("streetlamp_flyer_red", (0.56, 0.34, 0.30), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	flyer_blue = mat.make("streetlamp_flyer_blue", (0.40, 0.46, 0.50), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	ink = mat.make("streetlamp_ink", (0.26, 0.24, 0.24), rough=0.9, bump=0.0, edge=0.0)
	wire = mat.make("streetlamp_wire", (0.12, 0.12, 0.12), rough=0.6, bump=0.0, edge=0.05)
	rust = mat.get("rust")
	metal = mat.get("metal")
	vine = mat.get("bark")
	ivy = mat.make("streetlamp_ivy", (0.20, 0.33, 0.20), pattern="leaf", rough=0.75, bump=0.4)
	ivy_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	glow = mat.get("spore_glow")
	rnd = geo.rng(17)

	parts = []
	steel = []

	# Pole: a flared foot, a long taper and a slip joint halfway up.
	pole = geo.lathe([(0.17, 0.05), (0.17, 0.10), (0.135, 0.20), (0.114, 0.38), (0.098, 3.0),
					  (0.104, 3.0), (0.104, 3.07), (0.096, 3.10), (0.080, POLE_TOP)], mat=galv, segments=10, name="pole")
	steel.append(pole)
	steel.append(geo.box((0.46, 0.46, 0.05), (0, 0, 0.025), mat=galv, bevel=0.012, bevel_segments=1, name="base_plate"))
	for sx in (-1.0, 1.0):
		for sy in (-1.0, 1.0):
			parts.append(geo.box((0.05, 0.05, 0.045), (sx * 0.17, sy * 0.17, 0.07), (0, 0, 20), mat=rust, name="nut"))
	parts.append(geo.cylinder(0.118, 0.03, (0, 0, 0.36), mat=rust, segments=10, caps=False, name="collar_rust"))

	# Access hatch near the foot, sprung a little open; two cut wires hang out of it.
	parts.append(on_pole((0.135, 0.02, 0.35), 0.76, -math.pi * 0.5, hole, "hatch_hole"))
	hatch = on_pole((0.13, 0.022, 0.34), 0.76, -math.pi * 0.5, galv, "hatch", lift=0.010, rz=-7.0)
	steel.append(hatch)
	parts.append(on_pole((0.025, 0.012, 0.025), 0.90, -math.pi * 0.5 + 0.12, metal, "hatch_screw", lift=0.03))
	for (dx, drop) in ((-0.03, 0.30),):
		parts.append(geo.tube([(dx, -0.11, 0.70), (dx - 0.01, -0.16, 0.66), (dx - 0.02, -0.18, 0.66 - drop * 0.6), (dx - 0.01, -0.17, 0.66 - drop)],
							  0.011, mat=wire, segments=4, name="hatch_wire"))
	parts.append(on_pole((0.03, 0.004, 0.30), 0.44, -math.pi * 0.5 + 0.35, rust, "rust_streak"))

	# Arm: up out of the pole top and over the road, a rusty band at the joint.
	arm_pts = [(0, 0, 5.55), (0, 0, 5.82), (0, -0.09, 6.06), (0, -0.28, 6.26), (0, -0.56, 6.39), (0, -0.88, 6.45),
			   (0, -1.18, 6.45), (0, -1.42, 6.42)]
	arm = geo.tube(arm_pts, [0.072, 0.070, 0.067, 0.064, 0.061, 0.058, 0.056, 0.054], mat=galv, segments=8, name="arm")
	steel.append(arm)
	parts.append(geo.cylinder(0.092, 0.09, (0, 0, POLE_TOP - 0.03), mat=rust, segments=10, caps=False, name="joint_rust"))

	# Cobra head: a lathe swept toward -Y, flattened, tilted a little up; a sagging lens bowl under it,
	# cracked and chipped, with a spore speck caught in the hole.
	head = []
	shell = geo.lathe([(0.05, 0.0), (0.10, 0.04), (0.15, 0.12), (0.178, 0.28), (0.172, 0.44), (0.14, 0.56), (0.08, 0.64), (0.0, 0.67)],
					  tuple(HEAD_PIVOT), (90, 0, 0), mat=housing, segments=12, name="head")
	geo.apply_transform(shell)
	zc = HEAD_PIVOT.z
	for v in shell.data.vertices:
		dz = v.co.z - zc
		v.co.z = zc + dz * (0.62 if dz > 0.0 else 0.35)
	shell.data.update()
	head.append(shell)
	rim = geo.cylinder(0.15, 0.03, (0, -1.72, zc - 0.05), mat=metal, segments=12, caps=False, name="lens_rim")
	rim.scale = (1.0, 1.6, 1.0)
	head.append(rim)
	bowl = geo.lathe([(0.0, -0.13), (0.075, -0.125), (0.122, -0.10), (0.136, -0.05), (0.14, 0.0)], (0, -1.72, zc - 0.04),
					 mat=lens, segments=12, name="lens")
	bowl.scale = (1.0, 1.65, 1.0)
	head.append(bowl)

	def on_bowl(a, dz, out=0.004):
		a = math.radians(a)
		return ((0.136 + out) * math.cos(a), -1.72 + (0.136 + out) * 1.65 * math.sin(a), zc - 0.04 + dz)

	chip = on_bowl(-52.0, -0.075, -0.012)
	head.append(geo.blob((0.05, 0.016, 0.04), chip, (0, 0, 28), mat=hole, subdivisions=1, name="lens_chip"))
	for path in (((-52, -0.075), (-75, -0.06), (-92, -0.085), (-110, -0.07)), ((-52, -0.075), (-30, -0.09), (-12, -0.065), (8, -0.08)),
				 ((-52, -0.075), (-58, -0.035))):
		head.append(geo.tube([on_bowl(a, dz) for (a, dz) in path], 0.0045, mat=crack, segments=3, name="lens_crack"))
	speck = geo.blob(0.024, on_bowl(-52.0, -0.075, -0.02), mat=glow, subdivisions=1, name="spore")
	geo.tag(speck, "pulse")
	head.append(speck)
	top = moss_pad((0.11, 0.19, 0.045), (0.01, -1.66, zc + 0.08), (0, 0, 8), moss, 5)
	head.append(top)
	tilt = Matrix.Translation(HEAD_PIVOT) @ Matrix.Rotation(math.radians(TILT), 4, 'X') @ Matrix.Translation(-HEAD_PIVOT)
	bpy.context.view_layer.update()
	for p in head:
		p.matrix_world = tilt @ p.matrix_world
	steel.append(shell)
	parts += [p for p in head if p is not shell]

	# Flyers banded round the pole at eye height, faded and torn, and one more taped on lower down.
	parts.append(geo.cylinder(pole_radius(1.65) + 0.004, 0.46, (0, 0, 1.65), mat=paper, segments=12, caps=False, name="flyer_band"))
	parts.append(geo.cylinder(pole_radius(1.80) + 0.007, 0.07, (0, 0, 1.80), mat=flyer_red, segments=12, caps=False, name="flyer_stripe"))
	for (a, w) in ((-1.9, 0.07), (-0.9, 0.05), (0.6, 0.06), (2.3, 0.08)):
		parts.append(on_pole((0.12, 0.005, 0.10), 1.56 + w, a, ink, "flyer_print", lift=0.005))
	for k, a in enumerate((-1.4, 0.1, 1.9, 3.0)):
		flap = on_pole((0.07, 0.005, 0.13), 1.40, a, paper if k % 2 else flyer_red, "flyer_flap", lift=0.004)
		flap.rotation_euler.x = math.radians(rnd.uniform(-14, 14))
		parts.append(flap)
	parts.append(on_pole((0.16, 0.007, 0.22), 1.26, 0.25, flyer_blue, "flyer"))
	parts.append(on_pole((0.10, 0.004, 0.05), 1.31, 0.25, ink, "flyer_title", lift=0.006))

	# Ivy: two stems winding up the lower pole, leaves along them, clumps where it thickens.
	for (turns, height, a0) in ((2.0, 2.45, 0.6), (1.2, 1.40, 3.6)):
		pts = []
		n = 10
		for k in range(n + 1):
			t = k / n
			a = a0 + turns * 2.0 * math.pi * t
			z = 0.12 + height * t
			r = pole_radius(z) + 0.012 + 0.005 * math.sin(k * 1.7)
			pts.append((math.cos(a) * r, math.sin(a) * r, z))
		radii = [0.016 * (1.0 - 0.6 * k / n) for k in range(n + 1)]
		parts.append(geo.tube(pts, radii, mat=vine, segments=4, name="vine"))
		for k in range(1, n + 1):
			for j in range(2 if k < n * 0.6 else 1):
				t = k / n
				a = a0 + turns * 2.0 * math.pi * t + rnd.uniform(-0.6, 0.6)
				out = Vector((math.cos(a), math.sin(a), 0.0))
				p = Vector(pts[k]) + out * 0.01 + Vector((0, 0, rnd.uniform(-0.06, 0.06)))
				parts.append(leaf_card(p, out, rnd.uniform(0.13, 0.18) * (1.0 - 0.3 * t), ivy if rnd.random() < 0.6 else ivy_dark))
	for k, (a, z, s) in enumerate(((0.9, 0.28, 0.15), (3.2, 0.45, 0.13), (5.1, 0.95, 0.11), (2.0, 1.30, 0.09))):
		c = geo.blob((s, s * 0.8, s * 0.9), (math.cos(a) * 0.12, math.sin(a) * 0.12, z), mat=ivy if k % 2 else ivy_dark,
					 subdivisions=1, lumpy=0.35, seed=k + 3, name="ivy_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	# Moss over the base plate, a shard of the lens in it, grass round the foot.
	foot = floor(moss_pad((0.40, 0.34, 0.12), (0.04, 0.03, 0.0), (0, 0, 15), moss, 7, segments=12))
	geo.tag(foot, "pulse")
	parts.append(foot)
	parts.append(floor(moss_pad((0.22, 0.17, 0.08), (-0.30, 0.22, 0.0), (0, 0, -30), moss_dark, 8)))
	parts.append(geo.box((0.07, 0.05, 0.012), (0.33, -0.36, 0.01), (0, 0, 35), mat=lens, name="shard"))
	parts.append(geo.box((0.05, 0.04, 0.01), (0.27, -0.42, 0.01), (0, 0, -20), mat=lens, name="shard"))
	for j in range(9):
		h = rnd.uniform(0.18, 0.38)
		a = rnd.uniform(0.0, 2.0 * math.pi)
		r = rnd.uniform(0.24, 0.40)
		g = geo.cone(rnd.uniform(0.03, 0.05), h, (math.cos(a) * r, math.sin(a) * r, h * 0.5),
					 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=4, name="grass")
		geo.tag(g, "sway", "foliage")
		parts.append(g)

	for i, p in enumerate(steel):
		geo.moss_mask(p, seed=i + 2)
	parts += steel

	res = pipeline.static("streetlamp", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
