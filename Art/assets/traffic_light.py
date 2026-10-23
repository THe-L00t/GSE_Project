# A Korean signal post at a city crossing on route 32: a galvanised pole with a mast arm out along +X
# holding a dead four-lamp horizontal signal (red, amber, left arrow, green; the amber lens cracked) and a
# blue street-name plate, a pedestrian head with the standing and walking figures, a yellow push-button box,
# rust at every joint, ivy up the lower pole, moss at the foot and on the signal hoods.
import math

import bmesh
import bpy
from mathutils import Vector

from gse import core, geo, mat, pipeline

POLE_H = 5.5
ARM_Z = 5.05
ARM_LEN = 3.30
HEAD = (2.45, 0.0, 4.72)
LAMP_R = 0.12
PED_Z = 2.62


def mesh_obj(name, verts, faces, m, smooth=True, recalc=True):
	bm = bmesh.new()
	bv = [bm.verts.new(v) for v in verts]
	for f in faces:
		bm.faces.new([bv[i] for i in f])
	if recalc:
		bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	me = bpy.data.meshes.new(name)
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, me))
	if m is not None:
		me.materials.append(m)
	for p in me.polygons:
		p.use_smooth = smooth
	return obj


def hood(cx, cz, y0, r, length, m, t=0.014, seg=6, a0=-25.0, a1=205.0):
	# A signal visor: a bent sheet round the top of a lamp, reaching out toward -Y.
	verts = []
	for y in (y0, y0 - length):
		for rr in (r + t, r):
			for k in range(seg + 1):
				a = math.radians(a0 + (a1 - a0) * k / seg)
				verts.append((cx + math.cos(a) * rr, y, cz + math.sin(a) * rr))
	n = seg + 1
	o0, i0, o1, i1 = 0, n, 2 * n, 3 * n
	faces = []
	for k in range(seg):
		faces.append((o0 + k, o0 + k + 1, o1 + k + 1, o1 + k))
		faces.append((i0 + k + 1, i0 + k, i1 + k, i1 + k + 1))
		faces.append((o1 + k, o1 + k + 1, i1 + k + 1, i1 + k))
		faces.append((i0 + k, i0 + k + 1, o0 + k + 1, o0 + k))
	for e in (0, seg):
		faces.append((o0 + e, o1 + e, i1 + e, i0 + e))
	return mesh_obj("hood", verts, faces, m, False)


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
	obj = mesh_obj("ivy_leaf", [tuple(v) for v in vs], faces, m, True, False)
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
		return 0.125
	return 0.125 - 0.025 * min(1.0, (z - 0.38) / (POLE_H - 0.48))


def on_pole(size, z, a, m, name, lift=0.0, rz=0.0):
	# A flat piece laid tangent on the pole at angle a (radians), its thin side pointing out.
	r = pole_radius(z) + size[1] * 0.5 + lift
	return geo.box(size, (math.cos(a) * r, math.sin(a) * r, z), (0, 0, math.degrees(a) - 90.0 + rz), mat=m, name=name)


def face_shape(outline, x, y, z, m, name, scale=1.0):
	# A single flat face lying on a -Y facing surface; outline in (x, z), wound to face -Y.
	area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(outline, outline[1:] + outline[:1]))
	pts = outline if area > 0.0 else list(reversed(outline))
	verts = [(x + px * scale, y, z + pz * scale) for (px, pz) in pts]
	return mesh_obj(name, verts, [tuple(range(len(verts)))], m, False, False)


def lettering(body, size, loc, rot, m, tris):
	# Painted lettering: only the front faces of the glyphs, thinned and capped at a triangle budget.
	obj = geo.text(body, size, loc, rot, m, depth=0.003)
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bm.normal_update()
	bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < 0.5], context='FACES')
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(16.0), use_dissolve_boundaries=False, verts=bm.verts[:], edges=bm.edges[:])
	bmesh.ops.triangulate(bm, faces=bm.faces[:])
	bm.to_mesh(obj.data)
	bm.free()
	return geo.decimate(obj, tris)


def build():
	galv = mat.make("signal_galv", (0.46, 0.47, 0.46), pattern="rust", rough=0.6, bump=0.3, amount=0.5)
	housing = mat.make("signal_housing", (0.22, 0.23, 0.23), pattern="rust", rough=0.65, bump=0.3, amount=0.45)
	visor = mat.make("signal_visor", (0.15, 0.16, 0.16), pattern="rust", rough=0.7, bump=0.25, amount=0.35)
	red = mat.make("signal_red", (0.42, 0.13, 0.11), rough=0.3, bump=0.05, grime=0.45, edge=0.12)
	amber = mat.make("signal_amber", (0.52, 0.37, 0.13), rough=0.3, bump=0.05, grime=0.45, edge=0.12)
	green = mat.make("signal_green", (0.13, 0.35, 0.29), rough=0.3, bump=0.05, grime=0.45, edge=0.12)
	figure_red = mat.make("signal_figure_red", (0.52, 0.20, 0.16), rough=0.4, bump=0.0, edge=0.0)
	figure_green = mat.make("signal_figure_green", (0.24, 0.46, 0.38), rough=0.4, bump=0.0, edge=0.0)
	crack = mat.make("signal_crack", (0.70, 0.66, 0.56), rough=0.5, bump=0.0, edge=0.0)
	hole = mat.make("signal_hole", (0.05, 0.05, 0.05), rough=0.9, bump=0.0, edge=0.0, grime=0.0)
	button_box = mat.make("signal_button_box", (0.62, 0.53, 0.25), pattern="rust", rough=0.65, bump=0.3, amount=0.6)
	plate_blue = mat.make("signal_plate_blue", (0.20, 0.33, 0.50), pattern="rust", rough=0.6, bump=0.25, amount=0.3)
	white = mat.make("signal_white", (0.80, 0.80, 0.75), rough=0.55, bump=0.05, grime=0.4)
	wire = mat.make("signal_wire", (0.12, 0.12, 0.12), rough=0.6, bump=0.0, edge=0.05)
	rust = mat.get("rust")
	metal = mat.get("metal")
	vine = mat.get("bark")
	ivy = mat.make("signal_ivy", (0.20, 0.33, 0.20), pattern="leaf", rough=0.75, bump=0.4)
	ivy_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	glow = mat.get("spore_glow")
	rnd = geo.rng(41)

	parts = []
	steel = []

	# Pole on a bolted base plate, a domed cap on top.
	steel.append(geo.lathe([(0.18, 0.05), (0.18, 0.10), (0.145, 0.20), (0.125, 0.38), (0.100, POLE_H - 0.10),
							(0.106, POLE_H - 0.08), (0.092, POLE_H - 0.03), (0.0, POLE_H)], mat=galv, segments=10, name="pole"))
	steel.append(geo.box((0.48, 0.48, 0.05), (0, 0, 0.025), mat=galv, bevel=0.012, bevel_segments=1, name="base_plate"))
	for sx in (-1.0, 1.0):
		for sy in (-1.0, 1.0):
			parts.append(geo.box((0.05, 0.05, 0.045), (sx * 0.18, sy * 0.18, 0.07), (0, 0, 20), mat=rust, name="nut"))
	parts.append(geo.cylinder(0.13, 0.03, (0, 0, 0.36), mat=rust, segments=10, caps=False, name="collar_rust"))

	# Mast arm along +X: a bolted flange and clamp band on the pole, a tie rod from the cap.
	parts.append(geo.cylinder(pole_radius(ARM_Z) + 0.012, 0.40, (0, 0, ARM_Z), mat=rust, segments=10, caps=False, name="arm_band"))
	steel.append(geo.box((0.05, 0.24, 0.34), (pole_radius(ARM_Z) + 0.03, 0, ARM_Z), mat=galv, bevel=0.01, bevel_segments=1, name="flange"))
	for (dy, dz) in ((-0.08, -0.12), (0.08, -0.12), (-0.08, 0.12), (0.08, 0.12)):
		parts.append(geo.box((0.03, 0.04, 0.04), (pole_radius(ARM_Z) + 0.065, dy, ARM_Z + dz), mat=rust, name="flange_bolt"))
	parts.append(on_pole((0.04, 0.004, 0.45), ARM_Z - 0.42, 0.25, rust, "rust_streak"))
	arm_pts = [(0.12, 0, ARM_Z), (1.10, 0, ARM_Z + 0.035), (2.20, 0, ARM_Z + 0.07), (ARM_LEN, 0, ARM_Z + 0.10)]
	arm = geo.tube(arm_pts, [0.078, 0.07, 0.062, 0.055], mat=galv, segments=8, name="arm")
	steel.append(arm)
	parts.append(geo.cylinder(0.062, 0.04, (ARM_LEN + 0.01, 0, ARM_Z + 0.10), (0, 90, 0), mat=rust, segments=8, name="arm_cap"))
	parts.append(geo.tube([(0.06, 0, POLE_H - 0.12), (1.75, 0, ARM_Z + 0.11)], 0.022, mat=galv, segments=5, name="tie_rod"))
	parts.append(geo.box((0.08, 0.06, 0.06), (1.75, 0, ARM_Z + 0.10), mat=rust, name="tie_clamp"))
	parts.append(geo.cylinder(pole_radius(POLE_H - 0.12) + 0.01, 0.06, (0, 0, POLE_H - 0.12), mat=rust, segments=10, caps=False, name="tie_band"))

	# The vehicle signal hung under the arm on two hangers, its cable run along the arm.
	hx, hy, hz = HEAD
	head = geo.box((1.32, 0.24, 0.36), (hx, hy, hz), mat=housing, bevel=0.025, bevel_segments=1, name="signal_head")
	steel.append(head)
	parts.append(geo.box((1.26, 0.012, 0.30), (hx, hy - 0.122, hz), mat=visor, name="signal_face"))
	for dx in (-0.45, 0.45):
		x = hx + dx
		top = ARM_Z + 0.035 + 0.035 * (x - 1.10) / 1.10
		parts.append(geo.box((0.06, 0.05, top - hz - 0.18), (x, 0, (top + hz + 0.18) * 0.5), mat=metal, name="hanger"))
		parts.append(geo.cylinder(0.072, 0.07, (x, 0, top), (0, 90, 0), mat=rust, segments=8, caps=False, name="hanger_clamp"))
	parts.append(geo.tube([(0.10, 0.07, ARM_Z - 0.04), (0.9, 0.075, ARM_Z - 0.06), (1.6, 0.07, ARM_Z - 0.03), (hx - 0.3, 0.06, ARM_Z - 0.02),
						   (hx - 0.32, 0.06, hz + 0.18)], 0.013, mat=wire, segments=4, name="cable"))
	for b in (-0.06, 0.06):
		parts.append(geo.box((1.2, 0.02, 0.03), (hx, hy + 0.125, hz + b), mat=visor, name="back_rib"))
	lenses = ((red, -0.465), (amber, -0.155), (green, 0.155), (green, 0.465))
	yf = hy - 0.128
	for (m, dx) in lenses:
		x = hx + dx
		parts.append(geo.cylinder(LAMP_R, 0.03, (x, yf, hz), (90, 0, 0), mat=m, segments=10, name="lens"))
		parts.append(hood(x, hz, yf + 0.01, LAMP_R + 0.012, 0.12, visor))
	arrow = [(-0.075, 0.0), (-0.02, 0.05), (-0.02, 0.02), (0.07, 0.02), (0.07, -0.02), (-0.02, -0.02), (-0.02, -0.05)]
	parts.append(face_shape(arrow, hx + 0.155, yf - 0.016, hz, figure_green, "arrow"))
	ax = hx - 0.155
	chip = (ax + 0.045, yf - 0.012, hz + 0.035)
	parts.append(geo.cylinder(0.032, 0.012, chip, (90, 0, 0), mat=hole, segments=6, name="lens_chip"))
	for (ex, ez) in ((-0.09, 0.06), (0.06, -0.075), (-0.06, -0.065)):
		parts.append(geo.tube([chip, (ax + ex * 0.55 + 0.02, yf - 0.017, hz + ez * 0.5 + 0.02), (ax + ex, yf - 0.017, hz + ez)], 0.0045,
							  mat=crack, segments=3, name="lens_crack"))
	top_moss = moss_pad((0.30, 0.10, 0.05), (hx - 0.20, hy - 0.02, hz + 0.17), (0, 0, 4), moss, 3, segments=8)
	parts.append(top_moss)
	parts.append(moss_pad((0.10, 0.07, 0.04), (hx + 0.465, yf - 0.06, hz + LAMP_R + 0.02), (0, 0, 0), moss_dark, 4, segments=8))

	# The blue street-name plate under the arm near the pole.
	px, pz = 1.05, ARM_Z - 0.20
	parts.append(geo.box((0.84, 0.03, 0.22), (px, 0, pz), mat=plate_blue, bevel=0.008, bevel_segments=1, name="name_plate"))
	for dx in (-0.3, 0.3):
		parts.append(geo.box((0.03, 0.03, 0.12), (px + dx, 0, pz + 0.15), mat=metal, name="plate_hanger"))
	for (side, rz) in ((-1.0, 0.0), (1.0, 180.0)):
		parts.append(geo.box((0.80, 0.004, 0.012), (px, side * 0.017, pz - 0.085), mat=white, name="plate_rule"))
		parts.append(lettering("은행나무로", 0.135, (px, side * 0.019, pz + 0.01), (90, 0, rz), white, 120))

	# Pedestrian head on two brackets: standing figure in red above, walking figure in green below.
	ry = -(pole_radius(PED_Z) + 0.04 + 0.11)
	ped = geo.box((0.38, 0.22, 0.78), (0, ry, PED_Z), mat=housing, bevel=0.02, bevel_segments=1, name="ped_head")
	steel.append(ped)
	for dz in (-0.24, 0.24):
		parts.append(geo.box((0.08, 0.06, 0.06), (0, -(pole_radius(PED_Z) + 0.02), PED_Z + dz), mat=metal, name="ped_bracket"))
		parts.append(geo.cylinder(pole_radius(PED_Z + dz) + 0.01, 0.06, (0, 0, PED_Z + dz), mat=rust, segments=10, caps=False, name="ped_band"))
	yp = ry - 0.112
	stand = [(-0.032, -0.10), (-0.010, -0.10), (0.0, -0.035), (0.010, -0.10), (0.032, -0.10), (0.028, 0.0), (0.045, 0.0), (0.040, 0.06),
			 (0.025, 0.07), (-0.025, 0.07), (-0.040, 0.06), (-0.045, 0.0), (-0.028, 0.0)]
	walk = [(-0.065, -0.10), (-0.038, -0.10), (0.0, -0.035), (0.032, -0.10), (0.062, -0.10), (0.022, -0.005), (0.030, 0.03), (0.065, -0.005),
			(0.075, 0.012), (0.036, 0.072), (-0.020, 0.072), (-0.062, 0.020), (-0.050, 0.008), (-0.018, 0.035), (-0.024, -0.02)]
	for (m, fm, outline, dz) in ((red, figure_red, stand, 0.19), (green, figure_green, walk, -0.19)):
		z = PED_Z + dz
		parts.append(geo.box((0.29, 0.02, 0.29), (0, yp, z), mat=m, name="ped_lens"))
		parts.append(face_shape(outline, 0.0, yp - 0.012, z - 0.02, fm, "ped_figure", 1.25))
		parts.append(geo.cylinder(0.034, 0.008, (0.0, yp - 0.014, z + 0.10), (90, 0, 0), mat=fm, segments=6, name="ped_figure_head"))
		parts.append(geo.box((0.34, 0.14, 0.014), (0, yp - 0.07, z + 0.16), (-8, 0, 0), mat=visor, name="ped_hood"))
		for sx in (-1.0, 1.0):
			parts.append(geo.box((0.014, 0.13, 0.26), (sx * 0.165, yp - 0.065, z + 0.03), mat=visor, name="ped_hood"))
	parts.append(moss_pad((0.15, 0.10, 0.045), (0.02, ry, PED_Z + 0.39), (0, 0, 10), moss, 6, segments=8))

	# Push-button box at hand height, turned toward the crossing.
	ba = math.radians(-58.0)
	parts.append(on_pole((0.14, 0.09, 0.22), 1.15, ba, button_box, "button_box"))
	bx, by = math.cos(ba), math.sin(ba)
	r = pole_radius(1.15) + 0.09
	parts.append(geo.cylinder(0.034, 0.03, (bx * r, by * r, 1.12), (90, 0, math.degrees(ba) - 90.0), mat=metal, segments=8, name="button"))
	parts.append(on_pole((0.11, 0.006, 0.05), 1.22, ba, white, "button_label", lift=0.09))

	# Ivy winding up the lower pole.
	for (turns, height, a0) in ((1.8, 2.25, 2.4), (1.1, 1.30, 5.2)):
		pts = []
		n = 10
		for k in range(n + 1):
			t = k / n
			a = a0 + turns * 2.0 * math.pi * t
			z = 0.12 + height * t
			rr = pole_radius(z) + 0.012 + 0.005 * math.sin(k * 1.7)
			pts.append((math.cos(a) * rr, math.sin(a) * rr, z))
		radii = [0.016 * (1.0 - 0.6 * k / n) for k in range(n + 1)]
		parts.append(geo.tube(pts, radii, mat=vine, segments=4, name="vine"))
		for k in range(1, n + 1):
			for j in range(2 if k < n * 0.6 else 1):
				t = k / n
				a = a0 + turns * 2.0 * math.pi * t + rnd.uniform(-0.6, 0.6)
				out = Vector((math.cos(a), math.sin(a), 0.0))
				p = Vector(pts[k]) + out * 0.01 + Vector((0, 0, rnd.uniform(-0.06, 0.06)))
				parts.append(leaf_card(p, out, rnd.uniform(0.13, 0.18) * (1.0 - 0.3 * t), ivy if rnd.random() < 0.6 else ivy_dark))
	for k, (a, z, s) in enumerate(((2.2, 0.30, 0.15), (4.6, 0.50, 0.13), (0.4, 0.85, 0.11))):
		c = geo.blob((s, s * 0.8, s * 0.9), (math.cos(a) * 0.13, math.sin(a) * 0.13, z), mat=ivy if k % 2 else ivy_dark,
					 subdivisions=1, lumpy=0.35, seed=k + 3, name="ivy_clump")
		geo.tag(c, "sway", "foliage")
		parts.append(c)

	# Moss over the base plate, grass round the foot.
	foot = floor(moss_pad((0.42, 0.36, 0.12), (-0.03, 0.04, 0.0), (0, 0, -10), moss, 7, segments=12))
	geo.tag(foot, "pulse")
	parts.append(foot)
	parts.append(floor(moss_pad((0.24, 0.18, 0.08), (0.30, -0.24, 0.0), (0, 0, 30), moss_dark, 8)))
	for j in range(9):
		h = rnd.uniform(0.18, 0.38)
		a = rnd.uniform(0.0, 2.0 * math.pi)
		rr = rnd.uniform(0.26, 0.42)
		g = geo.cone(rnd.uniform(0.03, 0.05), h, (math.cos(a) * rr, math.sin(a) * rr, h * 0.5),
					 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=4, name="grass")
		geo.tag(g, "sway", "foliage")
		parts.append(g)
	g = geo.blob(0.03, (-0.28, -0.20, 0.10), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(steel):
		geo.moss_mask(p, seed=i + 2)
	parts += steel

	res = pipeline.static("traffic_light", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
