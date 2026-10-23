# One 4 m segment of a cement-block garden wall on a low concrete footing: the block courses and their
# mortar joints, a mortar cap along the top, a stretch of the top fallen in with the broken blocks lying
# at its foot, "주차금지" painted long ago on the front, ivy over one half, moss on the top and at the base.
# The ends are cut flat on a block joint, so segments chain end to end or stand alone.
import math

import bmesh
import bpy
from mathutils import Euler, Matrix, Vector

from assets.ruin import flat_text
from gse import core, geo, mat, pipeline

L = 4.0
Z0 = 0.14
COURSE = 0.2
BLOCK = 0.4
TOP = Z0 + 7 * COURSE
# Front outline of the wall (x, z): the fallen-in stretch steps down along the block joints, with a few blocks broken.
OUTLINE = [(-2.0, Z0), (2.0, Z0), (2.0, TOP), (1.6, TOP), (1.6, 1.34), (1.4, 1.34), (1.4, 1.14), (1.32, 1.14), (1.2, 1.0),
		   (1.2, 0.94), (1.0, 0.94), (1.0, 0.74), (0.6, 0.74), (0.6, 0.94), (0.4, 0.94), (0.4, 1.06), (0.3, 1.14), (-0.05, 1.14),
		   (-0.15, 1.34), (-0.4, 1.34), (-0.4, TOP), (-2.0, TOP)]
# Exposed tops of broken courses, where the blocks' hollow cores show: (x, z).
CORES = [(0.5, 0.94), (1.1, 0.94), (0.7, 0.74), (0.9, 0.74), (0.1, 1.14), (-0.3, 1.34), (1.5, 1.34)]


def blocks(name, color, axis='X', z0=Z0, var=0.12, joint=(0.004, 0.013)):
	# Cement blocks in running bond, drawn by the shader from the model's own coordinates: bed joints every
	# course, head joints every block, alternate courses shifted half a block, each block a slightly different grey.
	m = mat.make(name, color, pattern="concrete", rough=0.93, bump=0.35)
	g = mat.Graph(m)
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
	c = bsdf.inputs["Base Color"].links[0].from_socket
	co = g.node("ShaderNodeTexCoord").outputs["Object"]
	sep = g.node("ShaderNodeSeparateXYZ")
	g.link(co, sep.inputs[0])
	nrm = g.node("ShaderNodeSeparateXYZ")
	g.link(g.node("ShaderNodeNewGeometry").outputs["Normal"], nrm.inputs[0])
	along = sep.outputs["X"] if axis == 'X' else sep.outputs["Y"]
	facing = g.math('ABSOLUTE', nrm.outputs["Y"] if axis == 'X' else nrm.outputs["X"])
	zc = g.math('DIVIDE', g.math('SUBTRACT', sep.outputs["Z"], z0), COURSE)
	row = g.math('FLOOR', zc)
	fz = g.math('FRACT', zc)
	u = g.math('ADD', g.math('DIVIDE', along, BLOCK), g.math('MULTIPLY', g.math('MODULO', row, 2.0), 0.5))
	cell = g.math('FLOOR', u)
	fu = g.math('FRACT', u)
	dz = g.math('MULTIPLY', g.math('MINIMUM', fz, g.math('SUBTRACT', 1.0, fz)), COURSE)
	du = g.math('MULTIPLY', g.math('MINIMUM', fu, g.math('SUBTRACT', 1.0, fu)), BLOCK)
	# No head joints on faces looking along the wall (its ends and tops).
	du = g.math('ADD', du, g.math('MULTIPLY', g.math('SUBTRACT', 1.0, facing), 0.5))
	face = g.ramp(g.math('MINIMUM', dz, du), joint[0], joint[1])
	pick = g.node("ShaderNodeCombineXYZ")
	g.link(cell, pick.inputs[0])
	g.link(row, pick.inputs[1])
	wn = g.node("ShaderNodeTexWhiteNoise", noise_dimensions='3D')
	g.link(pick.outputs[0], wn.inputs["Vector"])
	tint = g.ramp(wn.outputs["Value"], 0.0, 1.0, 1.0 - var, 1.0 + var)
	c = g.mix(1.0, c, tint, 'MULTIPLY')
	c = g.mix(face, g.mix(1.0, c, (0.58, 0.58, 0.56, 1.0), 'MULTIPLY'), c)
	nt.links.new(c, bsdf.inputs["Base Color"])
	bump = next(n for n in nt.nodes if n.type == 'BUMP')
	h = bump.inputs["Height"].links[0].from_socket
	nt.links.new(g.math('ADD', h, g.math('MULTIPLY', face, 0.7)), bump.inputs["Height"])
	return m


def wall_slab(outline, thickness, y, m, name="wall"):
	# Outline in (x, z), extruded toward -Y from y, its edges softened with a single bevel segment.
	obj = geo.polygon_slab(outline, thickness, loc=(0, y, 0), rot=(90, 0, 0), mat=m, name=name)
	mod = obj.modifiers.new("bevel", 'BEVEL')
	mod.width = 0.01
	mod.segments = 1
	mod.limit_method = 'ANGLE'
	mod.angle_limit = math.radians(40.0)
	return obj


def plate(w, h, loc, rot, m, name="plate"):
	# A single flat quad facing +Z before rotation: painted marks, dark holes.
	bm = bmesh.new()
	vs = [bm.verts.new((x * w * 0.5, y * h * 0.5, 0.0)) for (x, y) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
	bm.faces.new(vs)
	me = bpy.data.meshes.new(name)
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, me))
	obj.location = Vector(loc)
	obj.rotation_euler = Euler([math.radians(a) for a in rot], 'XYZ')
	me.materials.append(m)
	return obj


def loose_block(loc, rot, m, hole, length=0.39, name="loose_block"):
	# A fallen block: the box and its two hollow cores, placed together.
	ps = [geo.box((length, 0.19, 0.19), (0, 0, 0), mat=m, bevel=0.01, bevel_segments=1, name=name)]
	for x in ((-0.095, 0.095) if length > 0.3 else (0.0,)):
		ps.append(plate(0.12, 0.11, (x, 0, 0.0955), (0, 0, 0), hole, "core"))
	M = Matrix.Translation(loc) @ Euler([math.radians(a) for a in rot], 'XYZ').to_matrix().to_4x4()
	for p in ps:
		p.matrix_basis = M @ p.matrix_basis
	return ps


def build():
	block = blocks("block_wall_block", (0.45, 0.45, 0.43))
	loose = mat.make("block_wall_loose", (0.45, 0.45, 0.43), pattern="concrete", rough=0.93, bump=0.35)
	footing_m = mat.make("block_wall_footing", (0.40, 0.40, 0.38), pattern="concrete", rough=0.93, bump=0.4)
	cap_m = mat.make("block_wall_cap", (0.50, 0.49, 0.46), pattern="concrete", rough=0.92, bump=0.4)
	hole = mat.make("block_wall_core", (0.08, 0.075, 0.07), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	paint = mat.make("block_wall_paint", (0.53, 0.27, 0.23), pattern="plaster", rough=0.9, bump=0.05, edge=0.0)
	vine = mat.make("block_wall_vine", (0.22, 0.25, 0.17), pattern="bark", rough=0.9, bump=0.4)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	grass = mat.make("block_wall_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	dry = mat.make("block_wall_dry_grass", (0.46, 0.43, 0.30), pattern="leaf", rough=0.9, bump=0.3)
	glow = mat.get("spore_glow")
	rnd = geo.rng(41)

	parts = []
	man_made = []

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			man_made.append(o)
		return o

	add(geo.box((L, 0.30, Z0), (0, 0, Z0 * 0.5), mat=footing_m, bevel=0.015, bevel_segments=1, name="footing"))
	add(wall_slab(OUTLINE, 0.2, 0.1, block))
	for (x, z) in CORES:
		parts.append(plate(0.12, 0.11, (x, 0.0, z + 0.002), (0, 0, 0), hole, "core"))

	# Mortar cap on what is left of the top; its broken ends stop short of the gap.
	for (x0, x1, seed) in ((-2.0, -0.44, 1), (1.64, 2.0, 2)):
		cap = geo.box((x1 - x0, 0.24, 0.06), ((x0 + x1) * 0.5, 0, TOP + 0.03), mat=cap_m, subdiv=1 if x1 - x0 > 1.0 else 0, name="cap")
		geo.jitter(cap, 0.008, 3.0, seed=seed, axes=(0.0, 1.0, 1.0))
		add(cap)

	# The fallen blocks and pieces of cap at the foot of the gap, on both sides.
	for (loc, rot, length) in (((0.55, -0.40, 0.085), (4, -3, 18), 0.39), ((0.98, -0.36, 0.10), (90, 0, -24), 0.39),
							   ((1.30, -0.40, 0.07), (0, 12, 62), 0.2), ((0.78, 0.34, 0.09), (-5, 2, -12), 0.39)):
		ps = loose_block(Vector(loc), rot, loose, hole, length)
		parts += ps
		man_made.append(ps[0])
	add(geo.box((0.32, 0.22, 0.05), (0.24, -0.34, 0.05), (0, 14, 30), mat=cap_m, name="cap_piece"))
	for (size, loc, seed) in (((0.16, 0.12, 0.09), (0.30, 0.42, 0.04), 3), ((0.12, 0.10, 0.08), (1.18, -0.30, 0.04), 4)):
		c = geo.box(size, loc, (rnd.uniform(-10, 10), rnd.uniform(-10, 10), rnd.uniform(0, 90)), mat=loose, subdiv=1, name="chunk")
		geo.jitter(c, min(size) * 0.2, scale=3.0, seed=seed)
		add(c)

	# "No parking", painted by hand across the left half of the front and faded since.
	text = flat_text("주차금지", 0.34, (-1.10, -0.1025, 0.80), (90, 0, 0), paint)
	text = geo.decimate(text, 380)
	parts.append(text)

	# Ivy over the right half of the front, a few runners over the top onto the back.
	stems = [
		([(1.58, -0.115, 0.14), (1.50, -0.115, 0.50), (1.62, -0.115, 0.90), (1.48, -0.115, 1.25), (1.52, -0.11, TOP + 0.02),
		  (1.50, 0.0, TOP + 0.09), (1.54, 0.11, TOP - 0.05), (1.50, 0.115, 1.20)], 0.022),
		([(1.56, -0.115, 0.55), (1.74, -0.115, 0.80), (1.86, -0.115, 1.12), (1.80, -0.115, 1.40)], 0.014),
		([(1.52, -0.115, 0.85), (1.34, -0.115, 0.98), (1.24, -0.115, 1.0)], 0.012),
	]
	for (pts, r) in stems:
		parts.append(geo.tube(pts, [r] * (len(pts) - 1) + [r * 0.5], mat=vine, segments=4, name="ivy"))
		for k in range(1, len(pts)):
			p0, p1 = pts[k - 1], pts[k]
			for j in range(2):
				t = (j + 0.5) / 2.0
				y =p0[1] + (p1[1] - p0[1]) * t
				face_y = 0.13 if y > 0.05 else (-0.13 if y < -0.05 else y)
				cc = (p0[0] + (p1[0] - p0[0]) * t + rnd.uniform(-0.08, 0.08), face_y, p0[2] + (p1[2] - p0[2]) * t + rnd.uniform(-0.05, 0.05))
				lf = geo.blob((0.08, 0.016, 0.07), cc, (rnd.uniform(-15, 15), 0, rnd.uniform(-30, 30)) if abs(face_y) > 0.1 else (90, 0, rnd.uniform(0, 90)),
							  mat=leaf if (k + j) % 2 else leaf_dark, subdivisions=1, name="ivy_leaf")
				geo.tag(lf, "sway", "foliage")
				parts.append(lf)
	for (loc, radii, m, seed) in (((1.56, -0.14, 0.42), (0.26, 0.05, 0.30), leaf_dark, 1), ((1.66, -0.14, 0.95), (0.24, 0.05, 0.26), leaf, 2),
								  ((1.48, -0.14, 1.36), (0.16, 0.045, 0.16), leaf_dark, 3)):
		b = geo.blob(radii, loc, mat=m, subdivisions=2, lumpy=0.3, seed=seed, name="ivy_mass")
		geo.tag(b, "sway", "foliage")
		parts.append(b)

	# Moss on the cap and the broken steps, creeping up from the footing on both faces.
	for (loc, radii, m, seed, sub) in (((-1.55, 0.0, TOP + 0.07), (0.34, 0.12, 0.05), moss, 1, 2), ((-0.75, 0.02, TOP + 0.07), (0.20, 0.10, 0.04), moss_dark, 2, 2),
									   ((1.86, 0.0, TOP + 0.07), (0.13, 0.10, 0.04), moss, 3, 1), ((0.80, 0.0, 0.75), (0.20, 0.10, 0.05), moss, 4, 2),
									   ((-0.28, 0.0, 1.35), (0.12, 0.09, 0.04), moss_dark, 5, 1)):
		b = geo.blob(radii, loc, mat=m, subdivisions=sub, lumpy=0.35, seed=seed, name="moss_top")
		if seed in (1, 4):
			geo.tag(b, "pulse")
		parts.append(b)
	for (loc, radii, m, seed) in (((-0.70, -0.17, 0.05), (0.40, 0.14, 0.17), moss, 6), ((-1.30, 0.17, 0.05), (0.36, 0.14, 0.18), moss_dark, 8),
								  ((0.40, 0.17, 0.05), (0.34, 0.14, 0.15), moss, 9)):
		parts.append(geo.blob(radii, loc, mat=m, subdivisions=2, lumpy=0.35, seed=seed, name="moss_base"))

	# Grass along the foot of the footing and between the fallen blocks.
	tufts = [(-1.75, -0.20, 6), (-0.95, -0.19, 5), (-0.15, -0.20, 6), (0.75, -0.48, 5), (1.85, -0.20, 5),
			 (-1.55, 0.19, 5), (-0.55, 0.20, 6), (0.55, 0.30, 5), (1.50, 0.20, 5)]
	for (cx, cy, n) in tufts:
		for k in range(n):
			hgt = rnd.uniform(0.16, 0.42)
			a = math.tau * k / n + rnd.uniform(-0.4, 0.4)
			lean = rnd.uniform(10, 28)
			w = geo.cone(rnd.uniform(0.024, 0.036), hgt, (cx + math.cos(a) * 0.03, cy + math.sin(a) * 0.02, hgt * 0.45),
						 (-math.sin(a) * lean, math.cos(a) * lean, 0), mat=(grass, leaf, grass, dry)[k % 4], segments=3, name="grass")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	g = geo.blob(0.04, (0.86, -0.05, 0.92), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 5)

	res = pipeline.static("block_wall", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
