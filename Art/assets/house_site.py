# What is left where a house stood: the concrete foundation with its raised edge, the floor slab cracked into
# plates with grass in every crack, cement-block wall stubs on three sides and a low partition, the stone
# doorstep at the front, the kitchen's old agungi firebox with its iron pot and blackened stone mouth, a broken
# brick chimney at the back, slate and roof-tile shards, a rusted washbasin, moss, and a young tree in the corner.
import math

import bmesh
import bpy
from mathutils import Euler, Vector

from assets.block_wall import blocks, plate
from assets.house_a import drop_faces, moss_pad
from gse import core, geo, mat, pipeline

XO = 3.45
FRONT, BACK = -2.40, 2.75
CW = 0.25
XI = XO - CW
YF, YB = FRONT + CW, BACK - CW
CURB = 0.30
SLAB = 0.22
FILL = 0.15
# Crack lines through the room's floor slab; the plates between them.
CRACKS = [[(-1.0, YF), (-0.7, -1.0), (-0.4, 0.3), (-1.1, 1.3), (-1.6, YB)],
		  [(-XI, 0.6), (-2.0, 0.2), (-0.4, 0.3)],
		  [(-0.4, 0.3), (0.5, -0.1), (1.6, -0.6)]]
PLATES = [([(-XI, YF), (-1.0, YF), (-0.7, -1.0), (-0.4, 0.3), (-2.0, 0.2), (-XI, 0.6)], (0.0, 0.0, 0.0), 0.0),
		  ([(-XI, 0.6), (-2.0, 0.2), (-0.4, 0.3), (-1.1, 1.3), (-1.6, YB), (-XI, YB)], (1.2, -0.8, 0.0), 0.01),
		  ([(-0.4, 0.3), (0.5, -0.1), (1.6, -0.6), (1.6, YB), (-1.6, YB), (-1.1, 1.3)], (0.0, 0.0, 0.0), 0.0),
		  ([(-1.0, YF), (1.6, YF), (1.6, -0.6), (0.5, -0.1), (-0.4, 0.3), (-0.7, -1.0)], (-1.5, 1.0, 0.0), -0.03)]
H1, H2, H3 = CURB + 0.2, CURB + 0.4, CURB + 0.6
# Wall stubs: (axis, centre line, base, outline in (along, z)). Tops step down along the block joints and break.
WALLS = [
	('X', BACK - CW * 0.5, CURB, [(-XO, CURB), (0.42, CURB), (0.42, 0.40), (0.22, H1), (-0.6, H1), (-0.6, H2), (-1.4, H2), (-1.48, 0.62),
								  (-1.72, 0.62), (-1.8, H2), (-2.2, H2), (-2.2, H3), (-XO, H3)]),
	('X', BACK - CW * 0.5, CURB, [(1.3, CURB), (XO, CURB), (XO, H2), (2.6, H2), (2.6, H1), (1.45, H1), (1.3, 0.40)]),
	('X', FRONT + CW * 0.5, CURB, [(-XO, CURB), (-2.2, CURB), (-2.3, 0.42), (-2.6, H1), (-3.0, H1), (-3.0, H2), (-XO, H2)]),
	('X', FRONT + CW * 0.5, CURB, [(2.7, CURB), (XO, CURB), (XO, H1), (2.82, H1)]),
	('Y', -XO + CW * 0.5, CURB, [(YF, CURB), (-1.2, CURB), (-1.35, H1), (YF, H1)]),
	('Y', -XO + CW * 0.5, CURB, [(-0.2, CURB), (YB, CURB), (YB, H3), (1.6, H3), (1.6, H2), (0.8, H2), (0.7, 0.58), (0.6, H1), (-0.05, H1), (-0.2, 0.40)]),
	('Y', XO - CW * 0.5, CURB, [(-1.0, CURB), (YB, CURB), (YB, H2), (1.6, H2), (1.45, 0.60), (1.3, H1), (-0.8, H1), (-1.0, 0.38)]),
	('Y', 1.7, 0.10, [(YF, 0.10), (-1.55, 0.10), (-1.55, 0.42), (-1.7, H1), (YF, H1)]),
	('Y', 1.7, 0.10, [(-0.7, 0.10), (YB, 0.10), (YB, H1), (1.5, H1), (1.5, H3), (0.35, H3), (0.2, 0.76), (0.2, H1), (-0.3, H1), (-0.45, 0.42), (-0.7, 0.42)]),
]
FIRE_X0, FIRE_X1 = 1.8, 2.6
FIRE_Y = 0.85
FIRE_W = 1.1
FIRE_TOP = 0.62
TREE = (2.85, 2.15)


def ngon(points, m, toward, name="ngon"):
	bm = bmesh.new()
	f = bm.faces.new([bm.verts.new(p) for p in points])
	bm.normal_update()
	if f.normal.dot(Vector(toward)) < 0.0:
		f.normal_flip()
	me = bpy.data.meshes.new(name)
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, me))
	me.materials.append(m)
	return obj


def stub(axis, centre, outline, m):
	# A block wall stub 0.2 thick standing on its centre line, edges softened, the hidden underside dropped.
	if axis == 'X':
		obj = geo.polygon_slab(outline, 0.2, loc=(0, centre + 0.1, 0), rot=(90, 0, 0), mat=m, name="wall")
	else:
		obj = geo.polygon_slab(outline, 0.2, loc=(centre - 0.1, 0, 0), rot=(90, 0, 90), mat=m, name="wall")
	mod = obj.modifiers.new("bevel", 'BEVEL')
	mod.width = 0.012
	mod.segments = 1
	mod.limit_method = 'ANGLE'
	mod.angle_limit = math.radians(40.0)
	return drop_faces(obj, lambda n: n.z < -0.9)


def cores(axis, centre, outline, base, m, rnd):
	# The hollow cores of the top blocks wherever a stub's top runs level; some have filled up with dirt.
	out = []
	for (u0, z0), (u1, z1) in zip(outline, outline[1:]):
		if abs(z0 - z1) > 1e-6 or z0 < base + 0.05:
			continue
		lo, hi = min(u0, u1) + 0.07, max(u0, u1) - 0.07
		k = math.ceil((lo - 0.1) / 0.2)
		while 0.1 + 0.2 * k <= hi:
			u = 0.1 + 0.2 * k
			loc = (u, centre, z0 + 0.003) if axis == 'X' else (centre, u, z0 + 0.003)
			if rnd.random() > 0.4:
				out.append(plate(0.12, 0.10, loc, (0, 0, 0 if axis == 'X' else 90), m, "core"))
			k += 1
	return out


def overgrow(m, scale=1.1):
	# Lichen and moss stains spreading over the concrete, and darker damp patches.
	g = mat.Graph(m)
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
	c = bsdf.inputs["Base Color"].links[0].from_socket
	co = g.node("ShaderNodeTexCoord").outputs["Object"]
	damp = mat._noise(g, co, scale * 0.7, 3.0, 0.55)
	c = g.mix(g.ramp(damp, 0.42, 0.60, 0.0, 0.5), c, (0.24, 0.24, 0.22, 1.0))
	patch = mat._noise(g, mat._stretch(g, co, 1.0, 1.0, 0.0), scale, 5.0, 0.65)
	c = g.mix(g.ramp(patch, 0.52, 0.63, 0.0, 0.8), c, mat.col((0.27, 0.34, 0.21)))
	nt.links.new(c, bsdf.inputs["Base Color"])
	return m


def crack_strip(line, m):
	# Grass grown thick along a crack, flush with the floor.
	pts = [(x, y, FILL + 0.03) for (x, y) in line]
	strip = geo.tube(pts, 0.055, mat=m, segments=4, name="crack_grass")
	return geo.jitter(strip, 0.012, 3.0, seed=len(line))


def smudge(m, color, z0, z1, amount=0.85):
	# Soot darkening a material upward from z0 to z1.
	g = mat.Graph(m)
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
	c = bsdf.inputs["Base Color"].links[0].from_socket
	sep = g.node("ShaderNodeSeparateXYZ")
	g.link(g.node("ShaderNodeTexCoord").outputs["Object"], sep.inputs[0])
	h = g.math('ADD', sep.outputs["Z"], g.math('MULTIPLY', mat._noise(g, g.node("ShaderNodeTexCoord").outputs["Object"], 6.0, 3.0, 0.6), 0.2))
	c = g.mix(g.ramp(h, z0, z1, 0.0, amount), c, mat.col(color))
	nt.links.new(c, bsdf.inputs["Base Color"])
	return m


def shrink(poly, d):
	cx = sum(p[0] for p in poly) / len(poly)
	cy = sum(p[1] for p in poly) / len(poly)
	out = []
	for (x, y) in poly:
		v = Vector((cx - x, cy - y))
		out.append((x + v.normalized().x * d - cx, y + v.normalized().y * d - cy))
	return out, (cx, cy)


def tile_shard(r, length, arc, loc, rot, m):
	# A broken piece of a curved roof tile: a short shell of a cylinder.
	bm = bmesh.new()
	n = 4
	rows = []
	for rr in (r, r - 0.018):
		for y in (-length * 0.5, length * 0.5):
			rows.append([bm.verts.new((math.cos(math.radians(90 - arc * 0.5 + arc * k / n)) * rr, y,
									   math.sin(math.radians(90 - arc * 0.5 + arc * k / n)) * rr - r)) for k in range(n + 1)])
	oa, ob, ia, ib = rows
	for k in range(n):
		bm.faces.new((oa[k], oa[k + 1], ob[k + 1], ob[k]))
		bm.faces.new((ia[k + 1], ia[k], ib[k], ib[k + 1]))
		bm.faces.new((oa[k + 1], oa[k], ia[k], ia[k + 1]))
		bm.faces.new((ob[k], ob[k + 1], ib[k + 1], ib[k]))
	for (a, b, c, d) in ((oa[0], ob[0], ib[0], ia[0]), (ob[n], oa[n], ia[n], ib[n])):
		bm.faces.new((a, b, c, d))
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	me = bpy.data.meshes.new("tile_shard")
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new("tile_shard", me))
	obj.location = Vector(loc)
	obj.rotation_euler = Euler([math.radians(a) for a in rot], 'XYZ')
	me.materials.append(m)
	return obj


def build():
	block_x = blocks("house_site_block_x", (0.45, 0.45, 0.43), 'X', z0=CURB, joint=(0.008, 0.02))
	block_y = blocks("house_site_block_y", (0.45, 0.45, 0.43), 'Y', z0=CURB, joint=(0.008, 0.02))
	curb_m = mat.make("house_site_curb", (0.41, 0.41, 0.39), pattern="concrete", rough=0.93, bump=0.4)
	slab_m = overgrow(mat.make("house_site_slab", (0.44, 0.43, 0.40), pattern="concrete", rough=0.93, bump=0.4))
	hole = mat.make("house_site_core", (0.17, 0.155, 0.13), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	soil = mat.get("soil")
	clay = smudge(mat.make("house_site_clay", (0.42, 0.34, 0.27), pattern="plaster", rough=0.94, bump=0.35), (0.12, 0.10, 0.09), 0.40, 0.75, 0.55)
	soot = mat.make("house_site_soot", (0.10, 0.09, 0.08), pattern="plaster", rough=0.95, bump=0.1, grime=0.0, edge=0.0)
	dark = mat.make("house_site_mouth", (0.03, 0.028, 0.025), pattern="plain", rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	burnt = smudge(mat.make("house_site_burnt_stone", (0.30, 0.29, 0.27), pattern="stone", rough=0.92, bump=0.6), (0.08, 0.07, 0.06), 0.2, 0.55, 0.7)
	granite = mat.make("house_site_granite", (0.50, 0.49, 0.46), pattern="stone", rough=0.9, bump=0.55)
	iron = mat.make("house_site_iron", (0.17, 0.16, 0.15), pattern="rust", rough=0.6, bump=0.35, amount=0.55, accent="rust")
	water = mat.make("house_site_water", (0.06, 0.10, 0.11), pattern="plain", rough=0.08, var=0.05, grime=0.0, edge=0.0, bump=0.02)
	ash = mat.make("house_site_ash", (0.38, 0.37, 0.35), pattern="plaster", rough=0.98, bump=0.3)
	char = mat.make("house_site_char", (0.13, 0.11, 0.10), pattern="wood", rough=0.95, bump=0.4)
	brick = smudge(mat.make("house_site_brick", (0.44, 0.31, 0.25), pattern="stone", rough=0.92, bump=0.6), (0.10, 0.09, 0.08), 1.05, 1.35)
	slate = mat.make("house_site_slate", (0.36, 0.38, 0.38), pattern="slate", rough=0.85, bump=0.6)
	giwa = mat.get("giwa")
	basin_m = mat.make("house_site_basin", (0.40, 0.37, 0.32), pattern="rust", rough=0.7, bump=0.4, amount=1.0)
	bark = mat.get("bark")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	leaf_light = mat.make("house_site_leaf_light", (0.26, 0.38, 0.22), pattern="leaf", rough=0.8, bump=0.45)
	grass = mat.make("house_site_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	dry = mat.make("house_site_dry_grass", (0.46, 0.43, 0.30), pattern="leaf", rough=0.9, bump=0.3)
	glow = mat.get("spore_glow")
	rnd = geo.rng(53)

	parts = []
	man_made = []

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			man_made.append(o)
		return o

	# Foundation: the raised concrete edge, one piece of it broken out at the front, earth filling the inside.
	for (size, loc) in (((2 * XO, CW, CURB), (0, BACK - CW * 0.5, CURB * 0.5)), ((XO + 0.95, CW, CURB), ((-XO + 0.95) * 0.5, FRONT + CW * 0.5, CURB * 0.5)),
						((XO - 1.35, CW, CURB), ((XO + 1.35) * 0.5, FRONT + CW * 0.5, CURB * 0.5)), ((CW, YB - YF, CURB), (-XO + CW * 0.5, (YF + YB) * 0.5, CURB * 0.5)),
						((CW, YB - YF, CURB), (XO - CW * 0.5, (YF + YB) * 0.5, CURB * 0.5))):
		add(drop_faces(geo.box(size, loc, mat=curb_m, bevel=0.02, bevel_segments=1, name="curb"), lambda n: n.z < -0.9))
	add(geo.box((0.34, 0.24, 0.16), (1.12, FRONT - 0.05, 0.07), (8, -12, 24), mat=curb_m, bevel=0.015, bevel_segments=1, name="curb_piece"))
	parts.append(drop_faces(geo.box((2 * XI + 0.02, YB - YF + 0.02, FILL), (0, (YF + YB) * 0.5, FILL * 0.5), mat=soil, name="fill"), lambda n: n.z < 0.5))

	# The room's floor slab, cracked into plates; one has sunk and one lifted at its edge.
	for (poly, tilt, dz) in PLATES:
		local, (cx, cy) = shrink(poly, 0.05)
		p = geo.polygon_slab(local, SLAB - 0.12, (cx, cy, 0.12 + dz), tilt, mat=slab_m, name="plate")
		add(drop_faces(p, lambda n: n.z < -0.9))

	# Wall stubs and the partition between the room and the kitchen.
	for (axis, centre, base, outline) in WALLS:
		add(stub(axis, centre, outline, block_x if axis == 'X' else block_y))
		parts += cores(axis, centre, outline, base, hole, rnd)

	# Stone doorstep in front of where the door was.
	step = geo.box((1.1, 0.34, 0.22), (-0.6, FRONT - 0.17, 0.09), (0, 0, 1.5), mat=granite, subdiv=1, name="doorstep")
	add(drop_faces(geo.jitter(step, 0.015, 2.5, seed=4), lambda n: n.z < -0.9))

	# Agungi: the clay firebox against the partition, a blackened stone mouth facing the kitchen, the iron pot on top.
	box = geo.box((FIRE_X1 - FIRE_X0, FIRE_W, FIRE_TOP - 0.10), ((FIRE_X0 + FIRE_X1) * 0.5, FIRE_Y, (FIRE_TOP + 0.10) * 0.5), mat=clay, subdiv=1, name="firebox")
	add(drop_faces(geo.jitter(box, 0.014, 2.0, seed=7), lambda n: n.z < -0.9))
	fx = FIRE_X1 + 0.012
	arch = [(fx, FIRE_Y - 0.16, 0.12), (fx, FIRE_Y + 0.16, 0.12), (fx, FIRE_Y + 0.16, 0.27)]
	for k in range(1, 6):
		a = math.pi * k / 6.0
		arch.append((fx, FIRE_Y + 0.16 * math.cos(a), 0.27 + 0.13 * math.sin(a)))
	arch.append((fx, FIRE_Y - 0.16, 0.27))
	parts.append(ngon(arch, dark, (1, 0, 0), "mouth"))
	plume = [(0.0, 0.30), (0.20, 0.36), (0.26, 0.48), (0.22, 0.58), (0.12, 0.62), (0.0, 0.66), (-0.14, 0.62), (-0.24, 0.55), (-0.22, 0.40)]
	parts.append(ngon([(FIRE_X1 + 0.006, FIRE_Y + u, z) for (u, z) in plume], soot, (1, 0, 0), "soot"))
	stain = [(-0.40, FIRE_TOP), (0.42, FIRE_TOP), (0.36, 0.74), (0.18, 0.86), (0.0, 0.80), (-0.16, 0.88), (-0.34, 0.76)]
	parts.append(ngon([(FIRE_X0 + 0.003, FIRE_Y + u, z) for (u, z) in stain], soot, (1, 0, 0), "soot_wall"))
	for s in (-1.0, 1.0):
		add(geo.box((0.10, 0.12, 0.30), (FIRE_X1 + 0.03, FIRE_Y + s * 0.23, 0.25), (0, 0, s * 4), mat=burnt, bevel=0.015, bevel_segments=1, name="mouth_stone"))
	add(geo.box((0.12, 0.62, 0.09), (FIRE_X1 + 0.035, FIRE_Y, 0.445), (3, 0, 0), mat=burnt, bevel=0.015, bevel_segments=1, name="lintel"))
	pc = ((FIRE_X0 + FIRE_X1) * 0.5, FIRE_Y)
	add(geo.lathe([(0.0, -0.02), (0.23, 0.0), (0.25, 0.08), (0.27, 0.09), (0.29, 0.045), (0.35, 0.012), (0.36, -0.01), (0.29, -0.03)],
				  (pc[0], pc[1], FIRE_TOP - 0.01), mat=iron, segments=9, name="pot"))
	parts.append(geo.cylinder(0.23, 0.01, (pc[0], pc[1], FIRE_TOP + 0.05), mat=water, segments=9, name="pot_water"))
	add(geo.lathe([(0.0, 0.20), (0.04, 0.195), (0.05, 0.17), (0.034, 0.145), (0.08, 0.13), (0.19, 0.098), (0.28, 0.045), (0.30, 0.012), (0.0, 0.012)],
				  (pc[0] - 0.02, pc[1] - 0.07, FIRE_TOP + 0.075), (5, -4, 0), mat=iron, segments=9, name="lid"))
	parts.append(geo.blob((0.26, 0.30, 0.06), (FIRE_X1 + 0.20, FIRE_Y, FILL), mat=ash, subdivisions=2, lumpy=0.3, seed=3, name="ash"))
	for (dy, rz) in ((-0.06, 74), (0.10, 104)):
		parts.append(geo.cylinder(0.035, 0.34, (FIRE_X1 + 0.14, FIRE_Y + dy, FILL + 0.05), (0, 90, rz), mat=char, segments=5, smooth=False, name="charred"))

	# Brick chimney at the back of the room, broken off, its top blackened.
	ch = geo.polygon_slab([(-0.21, 0.0), (0.21, 0.0), (0.21, 1.0), (0.13, 1.07), (0.04, 0.99), (-0.06, 1.08), (-0.21, 1.01)], 0.42,
						  (-2.9, BACK - CW * 0.5 + 0.21, CURB), (90, 0, 0), mat=brick, name="chimney")
	add(drop_faces(ch, lambda n: n.z < -0.9))

	# Roof debris: slate pieces and curved tile shards on the floor and against the walls; the washbasin in the kitchen.
	slates = [((0.52, 0.40), (-2.4, -1.2, SLAB + 0.006), (0, 0, 20)), ((0.44, 0.30), (0.6, 1.4, SLAB + 0.02), (4, -3, -35)),
			  ((0.50, 0.36), (-2.2, 2.36, 0.33), (35, 0, 5)), ((0.36, 0.26), (0.2, -1.6, SLAB + 0.008), (2, 0, 62)),
			  ((0.40, 0.30), (2.9, -1.7, FILL + 0.01), (0, 3, -10)), ((0.30, 0.22), (-1.5, 0.9, SLAB + 0.03), (6, 5, 40)),
			  ((0.46, 0.32), (-2.9, -0.3, SLAB + 0.005), (0, 0, 75))]
	for i, ((w, h), loc, rot) in enumerate(slates):
		r2 = geo.rng(100 + i)
		poly = [(-w * 0.5, -h * 0.5), (w * 0.5 * r2.uniform(0.5, 1.0), -h * 0.5), (w * 0.5, -h * 0.5 * r2.uniform(-0.2, 0.6)),
				(w * 0.5 * r2.uniform(0.3, 0.9), h * 0.5), (-w * 0.5 * r2.uniform(0.2, 1.0), h * 0.5 * r2.uniform(0.6, 1.0)), (-w * 0.5, 0.0)]
		add(geo.polygon_slab(poly, 0.014, loc, rot, mat=slate, name="slate"))
	tiles = [((-1.9, 1.95, SLAB), 35), ((0.95, -1.25, SLAB), -60), ((-0.25, 2.05, SLAB), 10), ((3.0, 0.2, FILL), 80), ((2.3, -1.5, FILL), 15)]
	for (loc, rz) in tiles:
		add(tile_shard(0.11, 0.17, 120.0, (loc[0], loc[1], loc[2] + 0.11 * 0.5), (0, 0, rz), giwa))
	bx, by = 2.85, -0.75
	add(geo.lathe([(0.0, 0.0), (0.17, 0.0), (0.23, 0.09), (0.255, 0.11), (0.245, 0.12), (0.215, 0.10), (0.155, 0.014), (0.0, 0.014)],
				  (bx, by, FILL), (4, -3, 0), mat=basin_m, segments=12, name="basin"))
	parts.append(geo.cylinder(0.19, 0.01, (bx, by, FILL + 0.07), (4, -3, 0), mat=water, segments=10, name="basin_water"))
	lf = geo.blob((0.05, 0.03, 0.006), (bx + 0.06, by - 0.04, FILL + 0.078), (0, 0, 30), mat=leaf, subdivisions=1, name="float_leaf")
	parts.append(lf)

	# A young tree come up through the kitchen floor in the back corner.
	tx, ty = TREE
	parts.append(geo.tube([(tx, ty, FILL - 0.05), (tx + 0.02, ty - 0.03, 0.6), (tx - 0.06, ty - 0.08, 1.1), (tx - 0.14, ty - 0.14, 1.55), (tx - 0.16, ty - 0.16, 1.85)],
						  [0.085, 0.065, 0.05, 0.036, 0.024], mat=bark, segments=6, name="trunk"))
	parts.append(geo.tube([(tx - 0.05, ty - 0.07, 1.05), (tx + 0.18, ty - 0.30, 1.42), (tx + 0.30, ty - 0.48, 1.62)], [0.032, 0.022, 0.014], mat=bark, segments=5, name="branch"))
	parts.append(geo.tube([(tx - 0.12, ty - 0.12, 1.4), (tx - 0.40, ty + 0.0, 1.66), (tx - 0.60, ty + 0.05, 1.78)], [0.026, 0.018, 0.012], mat=bark, segments=5, name="branch"))
	for (c, r, m, seed) in (((tx - 0.16, ty - 0.18, 2.0), (0.46, 0.44, 0.34), leaf, 1), ((tx + 0.30, ty - 0.50, 1.70), (0.34, 0.32, 0.26), leaf_dark, 2),
							((tx - 0.62, ty + 0.04, 1.80), (0.34, 0.30, 0.26), leaf_dark, 3), ((tx + 0.08, ty + 0.12, 1.72), (0.28, 0.26, 0.22), leaf_dark, 4),
							((tx - 0.30, ty - 0.50, 1.82), (0.30, 0.28, 0.24), leaf, 6), ((tx - 0.10, ty - 0.26, 2.24), (0.26, 0.24, 0.22), leaf_light, 5)):
		b = geo.blob(r, c, mat=m, subdivisions=2, lumpy=0.3, seed=seed, name="leaves")
		geo.tag(b, "sway", "foliage")
		parts.append(b)
	parts.append(geo.blob((0.26, 0.22, 0.07), (tx, ty, FILL), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=8, name="root_moss"))

	# Moss: cushions on the slab and in the corners, on the wall tops, the doorstep and the firebox.
	pads = [((0.55, 0.42, 0.07), (-2.5, 1.9, SLAB), moss, True), ((0.40, 0.30, 0.06), (0.9, 0.6, SLAB), moss_dark, False),
			((0.45, 0.30, 0.07), (-1.0, -1.8, SLAB), moss, False), ((0.24, 0.13, 0.06), (-2.45, BACK - CW * 0.5, H3), moss, True),
			((0.14, 0.42, 0.06), (-XO + CW * 0.5, 2.0, H3), moss_dark, False), ((0.30, 0.13, 0.05), (3.1, BACK - CW * 0.5, H2), moss_dark, False),
			((0.13, 0.32, 0.05), (1.7, 0.9, H3), moss, False), ((0.30, 0.13, 0.05), (-0.7, FRONT - 0.17, 0.20), moss, False),
			((0.16, 0.14, 0.05), (FIRE_X0 + 0.12, FIRE_Y + 0.42, FIRE_TOP), moss_dark, False), ((0.50, 0.34, 0.08), (2.7, -1.3, FILL), moss_dark, False)]
	for i, (radii, loc, m, pulse) in enumerate(pads):
		pad = moss_pad(radii, loc[0], loc[1], loc[2] - 0.02, (0, 0, rnd.uniform(-20, 20)), m, 30 + i, segments=8)
		if pulse:
			geo.tag(pad, "pulse")
		parts.append(pad)

	# Grass and weeds in every crack of the slab, in the kitchen earth, and round the outside of the foundation.
	def blades(cx, cy, z, n, hmin, hmax, spread=0.03):
		for k in range(n):
			hgt = rnd.uniform(hmin, hmax)
			a = math.tau * k / n + rnd.uniform(-0.4, 0.4)
			lean = rnd.uniform(10, 30)
			w = geo.cone(rnd.uniform(0.022, 0.034), hgt, (cx + math.cos(a) * spread, cy + math.sin(a) * spread, z + hgt * 0.45),
						 (-math.sin(a) * lean, math.cos(a) * lean, 0), mat=(grass, leaf, grass, dry)[k % 4], segments=3, name="grass")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	for line in CRACKS:
		parts.append(crack_strip(line, grass))
		for (x0, y0), (x1, y1) in zip(line, line[1:]):
			seg = math.hypot(x1 - x0, y1 - y0)
			steps = max(1, int(seg / 0.24))
			for s in range(steps):
				t = (s + rnd.uniform(0.2, 0.8)) / steps
				blades(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, FILL, rnd.choice((2, 3, 3)), 0.16, 0.38, 0.025)
	for (cx, cy) in ((2.2, -1.9), (3.0, -0.2), (2.0, 2.2), (3.05, 1.2), (2.25, -0.7), (1.95, 1.75)):
		blades(cx, cy, FILL, 5, 0.18, 0.40)
	outside = [(-3.55, -2.2), (-3.56, -0.8), (-3.55, 0.5), (-3.56, 1.9), (-2.6, 2.84), (-1.2, 2.83), (0.8, 2.84), (2.2, 2.83), (3.55, 2.4),
			   (3.56, 1.0), (3.55, -0.6), (3.56, -1.9), (2.6, -2.52), (1.5, -2.55), (0.4, -2.52), (-1.6, -2.52), (-2.8, -2.52)]
	for (cx, cy) in outside:
		blades(cx, cy, 0.0, 4, 0.18, 0.42)

	g = geo.blob(0.045, (tx - 0.35, ty - 0.45, 1.4), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 9)

	res = pipeline.static("house_site", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
