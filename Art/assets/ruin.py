# A collapsed shop wall: a brick core with a broken top, plaster flaking off both faces, a window hole,
# rusted rebar sticking up, the shop's faded painted name and phone number, fallen chunks,
# moss on the top and creeping up from the ground, a sapling rooted in the rubble on top.
import math

import bmesh
import bpy

from gse import geo, mat, pipeline

TOP = [(-1.5, 1.52), (-1.32, 1.70), (-1.18, 1.62), (-1.0, 1.78), (-0.85, 1.70), (-0.62, 1.80), (-0.45, 1.74),
	   (-0.22, 1.80), (-0.05, 1.60), (0.10, 1.66), (0.28, 1.42), (0.45, 1.48), (0.62, 1.22), (0.80, 1.30),
	   (0.92, 1.02), (1.10, 0.95), (1.22, 0.66), (1.38, 0.72), (1.5, 0.55)]


def top_at(x):
	for (x0, z0), (x1, z1) in zip(TOP, TOP[1:]):
		if x0 <= x <= x1:
			return z0 + (z1 - z0) * (x - x0) / (x1 - x0)
	return TOP[-1][1]


def wall_slab(outline, thickness, y, m, name, bevel=0.0):
	# Outline in (x, z), extruded toward -Y from y.
	return geo.polygon_slab(outline, thickness, loc=(0, y, 0), rot=(90, 0, 0), mat=m, name=name, bevel=bevel)


def flat_text(body, size, loc, rot, m, angle=20.0):
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


def punch(obj, cutter):
	obj = geo.apply_modifiers(obj)
	mod = obj.modifiers.new("hole", 'BOOLEAN')
	mod.operation = 'DIFFERENCE'
	mod.object = cutter
	mod.solver = 'EXACT'
	obj = geo.apply_modifiers(obj)
	# A boolean can leave an empty material slot behind; the bake needs every slot filled.
	slots = obj.data.materials
	for i in reversed(range(len(slots))):
		if slots[i] is None:
			for p in obj.data.polygons:
				if p.material_index >= i:
					p.material_index = max(0, p.material_index - 1)
			slots.pop(index=i)
	return obj


def chunk(size, loc, rot, m, seed, name="chunk"):
	c = geo.box(size, loc, rot, mat=m, subdiv=1, name=name)
	geo.jitter(c, min(size) * 0.18, scale=3.0, seed=seed)
	return c


def build():
	brick = mat.make("ruin_brick", (0.44, 0.31, 0.25), pattern="stone", rough=0.92, bump=0.6)
	plaster = mat.make("ruin_plaster", (0.44, 0.45, 0.42), pattern="plaster", rough=0.93, bump=0.35)
	paint_name = mat.make("ruin_paint_red", (0.52, 0.32, 0.28), pattern="plaster", rough=0.9, bump=0.1, edge=0.0)
	paint_num = mat.make("ruin_paint_blue", (0.30, 0.35, 0.40), pattern="plaster", rough=0.9, bump=0.1, edge=0.0)
	concrete = mat.get("concrete")
	rust = mat.get("rust")
	wood = mat.get("wood_grey")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	bark = mat.get("bark")
	glow = mat.get("spore_glow")
	rnd = geo.rng(9)

	parts = []

	# Brick core with the broken top, plaster skins front and back with their own ragged edges.
	core = wall_slab([(-1.5, 0.0), (1.5, 0.0)] + list(reversed(TOP)), 0.31, 0.155, brick, "core", bevel=0.02)
	front = wall_slab([(-1.48, 0.04), (0.90, 0.04), (0.95, 0.28), (1.10, 0.36), (1.22, 0.25), (1.26, 0.04), (1.48, 0.04),
					   (1.48, 0.40), (1.30, 0.52), (1.15, 0.48), (1.0, 0.80), (0.85, 0.88), (0.70, 1.12), (0.52, 1.08),
					   (0.40, 1.28), (0.15, 1.30), (0.0, 1.50), (-0.25, 1.62), (-0.45, 1.55), (-0.70, 1.66), (-0.95, 1.52),
					   (-1.15, 1.56), (-1.30, 1.45), (-1.48, 1.40)], 0.025, -0.155, plaster, "skin_front")
	back = wall_slab([(-1.48, 0.04), (1.48, 0.04), (1.48, 0.42), (1.25, 0.50), (1.05, 0.78), (0.70, 0.84), (0.55, 1.10),
					  (0.20, 1.20), (-0.10, 1.48), (-0.40, 1.52), (-0.55, 1.20), (-0.80, 1.10), (-0.95, 1.40), (-1.20, 1.50),
					  (-1.48, 1.38)], 0.025, 0.18, plaster, "skin_back")
	# The window hole, knocked through each layer with ragged edges.
	cutter = geo.box((0.54, 0.8, 0.50), (0.48, 0.0, 0.79), mat=brick, subdiv=2, name="cutter")
	geo.jitter(cutter, 0.05, scale=4.0, seed=3, axes=(1.0, 0.0, 1.0))
	layers = [punch(o, cutter) for o in (core, front, back)]
	bpy.data.objects.remove(cutter)
	wall = geo.join(layers, "wall")
	geo.moss_mask(wall, seed=2)
	parts.append(wall)
	parts.append(geo.box((0.66, 0.44, 0.06), (0.48, -0.03, 0.53), (0, 0, 0), mat=concrete, bevel=0.01, bevel_segments=1, name="sill"))
	parts.append(geo.box((0.05, 0.06, 0.40), (0.30, -0.10, 0.76), (0, 8, 0), mat=wood, name="frame_left"))
	parts.append(geo.box((0.34, 0.06, 0.05), (0.42, -0.10, 0.58), (0, -6, 0), mat=wood, name="frame_sill"))

	# The shop's painted name and number, faded into the plaster.
	parts.append(flat_text("물안개 상회", 0.24, (-0.78, -0.185, 1.22), (90, 0, 0), paint_name))
	parts.append(flat_text("전화 832-1170", 0.11, (-0.78, -0.185, 0.98), (90, 0, 0), paint_num))

	# Rebar bent up out of the broken top.
	for (x, rise, bx, by) in ((-0.88, 0.32, 0.10, -0.04), (-0.30, 0.45, -0.08, 0.06), (0.20, 0.30, 0.14, 0.02), (0.96, 0.38, 0.05, -0.10), (1.30, 0.25, -0.12, 0.05)):
		z = top_at(x)
		for dy in (-0.07, 0.07) if x < 0.0 else (0.0,):
			parts.append(geo.tube([(x, dy, z - 0.12), (x, dy, z + rise * 0.6), (x + bx, dy + by, z + rise)], 0.014, mat=rust, segments=5, name="rebar"))

	# Fallen pieces: a slab of the top leaning at the end, chunks and loose bricks.
	parts.append(chunk((0.78, 0.30, 0.62), (1.86, -0.12, 0.30), (0, 30, 14), brick, 1, "slab"))
	parts.append(chunk((0.70, 0.04, 0.50), (1.70, -0.29, 0.38), (0, 30, 14), plaster, 2, "slab_skin"))
	for (size, loc, rz, m, s) in (((0.36, 0.28, 0.20), (-0.92, -0.62, 0.09), 25, brick, 3), ((0.26, 0.20, 0.16), (0.42, -0.72, 0.07), -15, plaster, 4),
								  ((0.32, 0.26, 0.20), (-1.72, 0.25, 0.09), 40, brick, 5), ((0.22, 0.18, 0.14), (0.95, 0.58, 0.06), 10, concrete, 6),
								  ((0.40, 0.30, 0.18), (-0.35, 0.62, 0.08), -30, brick, 7)):
		c = chunk(size, loc, (rnd.uniform(-10, 10), rnd.uniform(-10, 10), rz), m, s)
		geo.moss_mask(c, seed=s)
		parts.append(c)
	for k in range(7):
		a = rnd.uniform(0, math.tau)
		parts.append(geo.box((0.22, 0.10, 0.07), (rnd.uniform(-1.6, 1.6), rnd.choice((-1, 1)) * rnd.uniform(0.35, 0.85), 0.03),
							 (rnd.uniform(-8, 8), rnd.uniform(-8, 8), math.degrees(a)), mat=brick, name="brick"))

	# Moss along the broken top and creeping up both faces from the ground.
	for i, x in enumerate((-1.30, -0.95, -0.55, -0.15, 0.30, 0.85, 1.30)):
		slope = math.degrees(math.atan2(top_at(x + 0.1) - top_at(x - 0.1), 0.2))
		m = geo.blob((0.26, 0.21, 0.10), (x, 0.0, top_at(x) - 0.02), (0, -slope, 0), mat=moss if i % 2 else moss_dark,
					 subdivisions=2, lumpy=0.4, seed=i, name="moss_top")
		if i == 2:
			geo.tag(m, "pulse")
		parts.append(m)
	base = geo.blob((1.10, 0.30, 0.16), (-0.55, -0.28, 0.04), mat=moss, subdivisions=2, lumpy=0.3, seed=11, name="moss_base")
	geo.tag(base, "pulse")
	parts.append(base)
	parts.append(geo.blob((0.90, 0.28, 0.14), (0.40, 0.30, 0.04), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=12, name="moss_base_back"))
	for (x, y, h, s) in ((-1.15, -0.19, 0.60, 13), (1.05, -0.19, 0.40, 15), (-0.7, 0.20, 0.70, 16), (0.9, 0.20, 0.45, 17)):
		parts.append(geo.blob((0.34, 0.045, h * 0.75), (x, y, h * 0.2), mat=moss_dark if s % 2 else moss, subdivisions=2, lumpy=0.5, seed=s, name="moss_creep"))

	# A sapling rooted in the top, ferns and weeds at the foot.
	parts.append(geo.tube([(-0.62, 0.0, 1.72), (-0.60, -0.02, 1.86), (-0.55, -0.04, 1.98)], [0.025, 0.018, 0.012], mat=bark, segments=5, name="sapling"))
	for (dx, dy, dz, r) in ((0.0, 0.0, 0.0, 0.10), (0.09, 0.05, -0.05, 0.08), (-0.08, -0.06, -0.04, 0.08)):
		lf = geo.blob((r, r, r * 0.8), (-0.55 + dx, -0.04 + dy, 2.02 + dz), mat=leaf, subdivisions=1, lumpy=0.3, seed=int(r * 100), name="sapling_leaf")
		geo.tag(lf, "sway", "foliage")
		parts.append(lf)
	for (cx, cy) in ((-1.35, -0.40), (0.15, -0.45), (1.25, -0.42), (-0.9, 0.45), (1.45, 0.35), (2.1, -0.3)):
		for k in range(4):
			h = rnd.uniform(0.22, 0.48)
			w = geo.cone(rnd.uniform(0.05, 0.08), h, (cx + rnd.uniform(-0.12, 0.12), cy + rnd.uniform(-0.1, 0.1), h * 0.5),
						 (rnd.uniform(-18, 18), rnd.uniform(-18, 18), 0), mat=leaf if k % 2 else leaf_dark, segments=5, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	g = geo.blob(0.05, (-0.48, 0.05, 1.86), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	return pipeline.static("ruin", parts, size=512)
