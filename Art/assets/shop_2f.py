# A two-storey 상가주택, shop below and home above: 행운슈퍼 under its big faded signboard with the shutter half up
# on the dark inside, a sagging striped awning, a bench, the ice-cream freezer and a stack of plastic crates; the
# hardware shop next door with cracked glass and a faded poster. Upstairs grilled windows, dead flower pots, an AC
# unit and a laundry pole; a concrete stair up the right side to the home's door; LPG cylinders, the back door and
# pipes behind; a water tank and a sapling on the roof, moss on the ledges, weeds round the foot.
# The main block is 7.25 m wide; the side stair fills the last 0.75 m of the 8 m footprint.
import math

import bpy
from mathutils import Matrix, Vector

from assets.office_4f import (Face, ac_unit, drop_faces, finish, flat_grid, flight, lowres, moss_pad, moss_strip, panel_sign,
							  ribs, ring, turn, weeds, window)
from gse import geo, mat

X0, X1 = -4.0, 3.25
SX1 = 4.0
HY = 3.5
SHOP = 2.85
F2 = 3.6
ROOF = 6.05
TOP = 6.6


def awning(face, u0, u1, n0, n1, z0, z1, sag, m, frame_m, tear=True):
	# A canvas awning from the wall (n0, z0) out to its front edge (n1, z1), sagging between the ends, with a
	# short valance; the last bay has torn loose and hangs.
	cols, rows = 8, 2
	pts = {}
	for i in range(cols + 1):
		t = i / float(cols)
		for j in range(rows + 1):
			s = j / float(rows)
			u = u0 + (u1 - u0) * t
			n = n0 + (n1 - n0) * s
			z = z0 + (z1 - z0) * s - sag * math.sin(math.pi * t) * s
			if tear and i == cols:
				z -= 0.45 * s
				n -= 0.25 * s
			pts[(i, j)] = (u, z, n)
		u, z, n = pts[(i, rows)]
		pts[(i, rows + 1)] = (u, z - 0.2 - (0.12 if tear and i == cols else 0.0), n + 0.01)
	keys = list(pts.keys())
	idx = {k: n for n, k in enumerate(keys)}
	verts = [pts[k] for k in keys]
	faces = []
	for i in range(cols):
		for j in range(rows):
			faces.append((idx[(i, j + 1)], idx[(i + 1, j + 1)], idx[(i + 1, j)], idx[(i, j)]))
		faces.append((idx[(i, rows + 1)], idx[(i + 1, rows + 1)], idx[(i + 1, rows)], idx[(i, rows)]))
	cloth = face.mesh(verts, faces, m, "awning")
	front_edge = [face.at(*(lambda p: (p[0], p[2], p[1]))(pts[(i, rows)])) for i in range(cols)]
	parts = [cloth, geo.tube(front_edge, 0.018, mat=frame_m, segments=4, name="awning_bar")]
	for i in (0, cols // 2, cols - 1):
		u, z, n = pts[(i, rows)]
		parts.append(geo.tube([face.at(u, 0.0, z0 - 0.45), face.at(u, n, z)], 0.015, mat=frame_m, segments=4, name="awning_arm"))
	return parts


def crate(loc, rz, m, inside, tilt=0.0):
	# A plastic bottle crate, open on top.
	w, d, h = 0.46, 0.33, 0.27
	parts = [drop_faces(geo.box((w, d, h), (0, 0, h / 2), mat=m, name="crate"), lambda nn: nn.z < -0.9),
			 geo.box((w - 0.06, d - 0.06, 0.004), (0, 0, h + 0.002), mat=inside, name="crate_hole")]
	bpy.context.view_layer.update()
	mm = Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(rz), 4, 'Z') @ Matrix.Rotation(math.radians(tilt), 4, 'X')
	for o in parts:
		o.matrix_world = mm @ o.matrix_world
	return parts


def build():
	wall_m = mat.make("shop_wall", (0.60, 0.56, 0.45), pattern="plaster", rough=0.92, bump=0.3)
	wall_low = mat.make("shop_wall_low", (0.47, 0.49, 0.47), pattern="plaster", rough=0.92, bump=0.3)
	band = mat.make("shop_band", (0.52, 0.51, 0.47), pattern="concrete", rough=0.9, bump=0.3)
	tile = mat.make("shop_pier", (0.44, 0.40, 0.36), pattern="stone", rough=0.8, bump=0.35)
	stone = mat.get("stone_dark")
	roof_m = mat.make("shop_roof", (0.42, 0.42, 0.40), pattern="concrete", rough=0.95, bump=0.3)
	stair_m = mat.make("shop_stair", (0.47, 0.46, 0.43), pattern="concrete", rough=0.95, bump=0.35)
	alu = mat.make("shop_alu", (0.60, 0.61, 0.60), rough=0.5, bump=0.1, grime=0.45)
	glass = mat.make("shop_glass", (0.16, 0.20, 0.23), pattern="plaster", var=0.3, rough=0.2, grime=0.15, edge=0.05, bump=0.05)
	curtain = mat.make("shop_curtain", (0.56, 0.48, 0.42), pattern="cloth", rough=0.9, bump=0.2)
	room = mat.make("shop_room", (0.11, 0.10, 0.09), pattern="concrete", rough=0.95, grime=0.6, edge=0.0)
	shelf_m = mat.make("shop_shelf", (0.16, 0.14, 0.12), pattern="wood", rough=0.9, grime=0.3, edge=0.0)
	dark = mat.make("shop_dark", (0.05, 0.045, 0.04), rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	shutter_m = ribs("shop_shutter", (0.52, 0.52, 0.49), 0.09, groove=0.6, pattern="rust", amount=0.45, rough=0.7)
	steel = mat.make("shop_steel", (0.28, 0.29, 0.29), pattern="rust", accent="rust", amount=1.0, rough=0.75, bump=0.4)
	rust = mat.get("rust")
	door_m = mat.make("shop_door", (0.27, 0.40, 0.33), pattern="rust", amount=0.9, rough=0.7, bump=0.35)
	ac_m = mat.make("shop_ac", (0.66, 0.66, 0.62), pattern="rust", amount=0.3, rough=0.6, bump=0.2, grime=0.5)
	fan_m = mat.make("shop_fan", (0.20, 0.20, 0.20), rough=0.7, bump=0.1)
	gas = mat.make("shop_gas", (0.62, 0.52, 0.22), pattern="rust", amount=0.5, rough=0.6, bump=0.2)
	lpg = mat.make("shop_lpg", (0.54, 0.56, 0.57), pattern="rust", amount=0.8, rough=0.6, bump=0.3)
	copper = mat.make("shop_copper", (0.52, 0.34, 0.22), rough=0.5, bump=0.1)
	pipe_m = mat.make("shop_pipe", (0.56, 0.56, 0.53), rough=0.6, bump=0.1, grime=0.5)
	paper = mat.make("shop_paper", (0.74, 0.71, 0.62), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	poster_red = mat.make("shop_poster_red", (0.60, 0.38, 0.32), pattern="paper", rough=0.95, bump=0.1, edge=0.0)
	awning_m = ribs("shop_awning", (0.54, 0.25, 0.21), 0.5, axis=0, soft=(-0.06, 0.06), color2=(0.74, 0.70, 0.60), dent=0.15,
					pattern="cloth", rough=0.95, bump=0.3)
	cream = mat.make("shop_sign_cream", (0.74, 0.70, 0.60), pattern="rust", accent=(0.62, 0.58, 0.50), amount=0.75, rough=0.65, bump=0.15)
	red_l = mat.make("shop_letter_red", (0.56, 0.21, 0.18), rough=0.6, bump=0.05, edge=0.05)
	teal = mat.make("shop_sign_teal", (0.22, 0.36, 0.40), pattern="rust", accent=(0.34, 0.42, 0.44), amount=0.7, rough=0.65, bump=0.15)
	cream_l = mat.make("shop_letter_cream", (0.82, 0.80, 0.72), rough=0.6, bump=0.05, edge=0.05)
	blue = mat.make("shop_sign_blue", (0.20, 0.32, 0.50), pattern="rust", accent=(0.30, 0.38, 0.48), amount=0.5, rough=0.6, bump=0.15)
	faded = mat.make("shop_faded_paint", (0.50, 0.36, 0.31), pattern="plaster", rough=0.9, bump=0.05, edge=0.0)
	freezer_m = mat.make("shop_freezer", (0.72, 0.72, 0.68), pattern="rust", amount=0.4, rough=0.55, bump=0.15, grime=0.5)
	wood = mat.get("wood_grey")
	crate_mats = [mat.make("shop_crate_" + n, c, pattern="plaster", rough=0.6, bump=0.15, grime=0.45)
				  for n, c in (("red", (0.52, 0.24, 0.20)), ("yellow", (0.66, 0.56, 0.26)), ("green", (0.26, 0.42, 0.30)), ("blue", (0.24, 0.34, 0.48)))]
	tank_m = mat.make("shop_tank", (0.66, 0.66, 0.60), pattern="rust", amount=0.35, rough=0.6, bump=0.2, grime=0.5)
	pot_m = mat.get("onggi")
	dry = mat.make("shop_dry_stalk", (0.46, 0.40, 0.28), pattern="wood", rough=0.9, bump=0.3)
	cloths = (mat.make("shop_cloth_blue", (0.42, 0.50, 0.58), pattern="cloth", rough=0.95, bump=0.3),
			  mat.make("shop_cloth_white", (0.76, 0.75, 0.70), pattern="cloth", rough=0.95, bump=0.3),
			  mat.make("shop_cloth_brown", (0.50, 0.40, 0.32), pattern="cloth", rough=0.95, bump=0.3))
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	bark = mat.get("bark")
	glow = mat.get("spore_glow")
	rnd = geo.rng(29)

	parts = []
	shell = []

	def add(objs, mossy=False):
		if not isinstance(objs, (list, tuple)):
			objs = [objs]
		parts.extend(objs)
		if mossy:
			shell.extend(objs)
		return objs

	front = Face(0, HY)
	right = Face(90, X1)
	back = Face(180, HY)
	left = Face(270, -X0)

	# ---- shell -----------------------------------------------------------------------------------------------------
	add(ring(X0 - 0.03, X1 + 0.03, -HY - 0.03, HY + 0.03, 0.0, 0.15, 0.06, stone, "plinth"), True)
	add(ring(X0 - 0.04, X1 + 0.04, -HY - 0.04, HY + 0.04, SHOP, F2 - 0.2, 0.04, band, "fascia", top=False), True)
	add(ring(X0 - 0.07, X1 + 0.07, -HY - 0.07, HY + 0.07, F2 - 0.2, F2, 0.07, band, "slab_band"), True)
	add(ring(X0, X1, -HY, HY, ROOF, TOP - 0.1, 0.2, band, "parapet_in", top=False, inner=ROOF, outer=False))
	add(ring(X0 - 0.04, X1 + 0.04, -HY - 0.04, HY + 0.04, TOP - 0.1, TOP, 0.28, band, "coping", inner=TOP - 0.1), True)
	add(flat_grid(X0 + 0.2, X1 - 0.2, -HY + 0.2, HY - 0.2, ROOF, 4, 3, roof_m, "roof"), True)
	for (fc, u0, u1) in ((front, X0, X1), (right, -HY, HY), (back, -X1, -X0), (left, -HY, HY)):
		add(fc.wall(u0, u1, F2, TOP - 0.1, wall_m, cols=3, rows=2, name="wall"), True)
		if fc is not front:
			add(fc.wall(u0, u1, 0.15, SHOP, wall_low, cols=3, name="wall"), True)
	for (u0, u1) in ((X0, X0 + 0.15), (-0.25, 0.05), (X1 - 0.15, X1)):
		add(drop_faces(front.box((u1 - u0, 0.08, SHOP - 0.15), (u0 + u1) * 0.5, 0.04, (SHOP + 0.15) * 0.5, tile, "pier"), lambda nn: abs(nn.z) > 0.9 or nn.y > 0.9), True)

	# ---- 행운슈퍼: shutter half up on the dark shop, awning, bench, freezer, crates ----------------------------------
	add(lowres(front.hole(X0 + 0.15, -0.25, 0.15, 2.6, 2.4, room), 0.25))
	for (u, w, h) in ((-3.0, 1.3, 1.6), (-1.2, 1.1, 1.3)):
		add(lowres(drop_faces(front.box((w, 0.4, h), u, -2.0, 0.15 + h / 2, shelf_m, "shelf"), lambda nn: nn.y > 0.9 or nn.z < -0.9), 0.3))
	add(front.box((3.6, 0.04, 1.1), -2.05, 0.03, 2.05, shutter_m, "shutter"))
	add(front.box((3.6, 0.06, 0.05), -2.05, 0.04, 1.5, steel, "shutter_bar"))
	add(drop_faces(front.box((3.66, 0.22, 0.25), -2.05, 0.11, 2.725, steel, "shutter_box"), lambda nn: nn.z < -0.9))
	add(panel_sign(front, X0 + 0.02, -0.2, 2.88, 3.54, 0.04, 0.14, cream, "행운슈퍼", 0.5, red_l))
	add(awning(front, X0 + 0.1, -0.3, 0.06, 0.78, SHOP, 2.4, 0.12, awning_m, steel))
	m = moss_pad((0.5, 0.25, 0.06), front.at(-2.2, 0.55, 2.47), (33, 0, 0), moss_dark, 3)
	add(m)
	bx, by = -2.4, -HY - 0.4
	add(geo.box((1.4, 0.36, 0.05), (bx, by, 0.43), mat=wood, name="bench"))
	add(geo.box((1.4, 0.04, 0.2), (bx, by + 0.17, 0.62), (8, 0, 0), mat=wood, name="bench_back"))
	for dx in (-0.58, 0.58):
		add(geo.box((0.05, 0.34, 0.41), (bx + dx, by, 0.205), mat=wood, name="bench_leg"))
	fx, fy = -1.15, -HY - 0.33
	add(drop_faces(geo.box((0.95, 0.6, 0.82), (fx, fy, 0.41), mat=freezer_m, bevel=0.03, name="freezer"), lambda nn: nn.z < -0.9))
	add(geo.box((0.82, 0.48, 0.006), (fx, fy, 0.823), mat=glass, name="freezer_lid"))
	add(front.quad(fx - 0.4, fx + 0.4, 0.45, 0.62, 0.635, blue, "freezer_band"))
	stack = [(-0.35, -3.77, 0.0, 0, 0), (-0.35, -3.77, 0.27, 1, 4), (-0.35, -3.77, 0.54, 2, -3), (0.16, -3.76, 0.0, 3, 2), (0.16, -3.76, 0.27, 0, -5)]
	for (x, y, z, c, rz) in stack:
		add(crate((x, y, z), rz, crate_mats[c], dark))
	add(crate((0.8, -4.0, 0.165), 30, crate_mats[1], dark, tilt=90))

	# ---- the hardware shop: glass front cracked, a faded poster, buckets and a broom outside ------------------------
	add(lowres(front.quad(0.05, X1 - 0.15, 0.15, SHOP, 0.0, glass), 0.5))
	add(front.box((3.0, 0.05, 0.28), 1.58, 0.03, 0.29, alu, "kick"))
	add(front.box((3.0, 0.06, 0.06), 1.58, 0.03, 2.3, alu, "transom"))
	for u in (0.08, 1.15, 2.15, X1 - 0.18):
		add(drop_faces(front.box((0.06, 0.06, SHOP - 0.15), u, 0.03, (SHOP + 0.15) * 0.5, alu, "mullion"), lambda nn: nn.y > 0.9 or abs(nn.z) > 0.9))
	add(front.box((0.03, 0.04, 0.45), 2.05, 0.08, 1.15, alu, "handle"))
	for z in (0.9, 1.3, 1.7):
		add(front.quad(0.2, 1.05, z, z + 0.05, 0.004, wood, "shelf"))
	add(front.quad(2.4, 2.95, 1.25, 2.0, 0.005, paper, "poster"))
	add(front.quad(2.46, 2.89, 1.55, 1.92, 0.006, poster_red, "poster_print"))
	add(front.cracks(0.65, 1.6, 0.006, cream_l, rnd, 7, 0.55))
	add(front.cracks(1.7, 0.9, 0.006, cream_l, rnd, 5, 0.35))
	add(panel_sign(front, 0.0, X1 + 0.02, 2.88, 3.54, 0.04, 0.14, teal, "철물점", 0.48, cream_l))
	for (z, r) in ((0.15, 0.17), (0.34, 0.17)):
		add(geo.cylinder(r, 0.3, (2.7, -HY - 0.25, z), mat=crate_mats[3], segments=10, radius_top=r + 0.03, caps=False, name="bucket"))
	add(geo.tube([(1.45, -HY - 0.12, 1.25), (1.6, -HY - 0.3, 0.12)], 0.016, mat=wood, segments=4, name="broom"))
	add(geo.box((0.28, 0.06, 0.16), (1.61, -HY - 0.31, 0.08), (0, -8, 0), mat=mat.get("canvas"), name="broom_head"))

	# The blue "담배" box sign at the corner.
	add(geo.box((0.12, 0.7, 0.46), (X0 + 0.12, -HY - 0.36, 4.1), mat=blue, name="tobacco"))
	for (fc, u) in ((Face(90, X0 + 0.18), -HY - 0.36), (Face(270, -(X0 + 0.06)), HY + 0.36)):
		add(fc.text("담배", 0.26, u, 0.004, 4.1, cream_l))
	add(geo.box((0.06, 0.12, 0.06), (X0 + 0.12, -HY - 0.03, 4.25), mat=steel, name="sign_bracket"))

	# ---- upstairs: grilled windows, dead pots on a rack, the AC unit, a laundry pole --------------------------------
	add(window(front, -2.6, F2 + 0.6, 1.6, 1.2, alu, curtain, sill_m=band, grille_m=steel, rnd=rnd))
	add(window(front, 1.7, F2 + 0.6, 1.4, 1.2, alu, glass, kind="cracked", sill_m=band, grille_m=steel, rnd=rnd))
	add(drop_faces(front.box((1.7, 0.32, 0.03), -2.6, 0.18, F2 + 0.3, steel, "pot_rack"), lambda nn: nn.y > 0.9))
	for (u, h) in ((-3.15, 0.22), (-2.7, 0.26), (-2.25, 0.2), (-1.95, 0.24)):
		add(geo.cylinder(0.12, h, front.at(u, 0.18, F2 + 0.315 + h / 2), mat=pot_m, segments=8, radius_top=0.15, name="pot"))
		for j in range(2):
			add(geo.tube([front.at(u, 0.18, F2 + 0.31 + h), front.at(u + rnd.uniform(-0.12, 0.12), 0.18 + rnd.uniform(-0.08, 0.08), F2 + 0.31 + h + rnd.uniform(0.2, 0.4))],
						 0.008, mat=dry, segments=3, name="stalk"))
	add(ac_unit(front.at(-0.35, 0.0, F2 + 0.3), 0, ac_m, fan_m, steel))
	for u in (0.75, 2.65):
		add(geo.tube([front.at(u, 0.0, F2 + 1.95), front.at(u, 0.42, F2 + 2.0)], 0.015, mat=steel, segments=4, name="hook"))
	add(geo.tube([front.at(0.55, 0.42, F2 + 2.02), front.at(2.9, 0.42, F2 + 1.98)], 0.022, mat=mat.make("shop_bamboo", (0.58, 0.52, 0.36), pattern="wood", rough=0.7, bump=0.2),
				 segments=5, name="laundry_pole"))
	for k, (u, w, h) in enumerate(((1.0, 0.38, 0.5), (1.6, 0.46, 0.66), (2.25, 0.3, 0.42))):
		add(front.box((w, 0.015, h), u, 0.42, F2 + 2.0 - h / 2, cloths[k], "cloth"))
	for (fc, u, z) in ((front, -2.0, F2 + 0.45), (front, 1.3, F2 + 0.45), (left, 0.8, F2 + 0.6)):
		add(fc.quad(u, u + 0.04, z - rnd.uniform(0.6, 1.0), z, 0.003, rust, "streak"))

	# ---- right side: the concrete stair up to the home's door ---------------------------------------------------------
	add(window(right, -2.3, 1.55, 0.8, 0.7, alu, glass, split=False, grille_m=steel, rnd=rnd))
	add(window(right, -1.3, F2 + 0.7, 1.2, 1.1, alu, curtain, sill_m=band, grille_m=steel, rnd=rnd))
	st = flight(-3.3, 1.75, 0.0, F2, -X1 - 0.02, SX1 - X1 - 0.03, 14, stair_m, "stair")
	turn([st], (0, 0, 0), (0, 0, 1), 90)
	add(st, True)
	add(drop_faces(geo.box((SX1 - X1 - 0.03, 1.7, 0.16), ((X1 + SX1) * 0.5, 2.6, F2 - 0.08), mat=stair_m, name="landing"), lambda nn: nn.x < -0.9), True)
	add(drop_faces(geo.box((0.2, 0.2, F2 - 0.16), (SX1 - 0.12, HY - 0.12, (F2 - 0.16) * 0.5), mat=stair_m, name="landing_post"), lambda nn: abs(nn.z) > 0.9))
	rx = SX1 - 0.05
	add(geo.tube([(rx, -3.3, 0.95), (rx, 1.75, F2 + 0.95), (rx, HY - 0.05, F2 + 0.95), (X1 + 0.02, HY - 0.05, F2 + 0.95)], 0.025, mat=steel, segments=5, name="rail"))
	for (y, z) in ((-3.3, 0.0), (-0.8, (2.5 / 5.05) * F2), (1.75, F2), (HY - 0.05, F2)):
		add(geo.box((0.04, 0.04, 0.95), (rx, y, z + 0.47), mat=steel, name="post"))
	add(right.frame(2.6, F2, 0.9, 2.0, 0.06, 0.08, steel))
	add(right.box((0.9, 0.04, 2.0), 2.6, 0.03, F2 + 1.0, door_m, "home_door"))
	add(drop_faces(right.box((1.3, 0.55, 0.06), 2.6, 0.275, F2 + 2.25, band, "door_canopy"), lambda nn: nn.z < -0.9))
	add(right.box((0.32, 0.12, 0.38), 1.8, 0.06, F2 + 1.35, mat.make("shop_mailbox", (0.62, 0.30, 0.24), pattern="rust", amount=0.6, rough=0.6, bump=0.2), "mailbox"))

	# ---- back: back door, LPG cylinders and the gas pipe, the boiler flue, drainpipes ---------------------------------
	add(back.frame(2.4, 0.15, 0.9, 2.0, 0.07, 0.07, steel))
	add(back.box((0.9, 0.04, 2.0), 2.4, 0.03, 1.15, door_m, "back_door"))
	add(drop_faces(back.box((1.3, 0.4, 0.15), 2.4, 0.2, 0.075, stair_m, "step"), lambda nn: nn.z < -0.9))
	add(window(back, 0.3, 1.6, 0.8, 0.7, alu, glass, split=False, grille_m=steel, rnd=rnd))
	add(window(back, 2.0, F2 + 0.75, 1.2, 1.0, alu, glass, sill_m=band, grille_m=steel, rnd=rnd))
	add(window(back, -0.9, F2 + 0.75, 1.2, 1.0, alu, curtain, sill_m=band, grille_m=steel, rnd=rnd))
	ly = HY + 0.22
	for k, x in enumerate((1.0, 1.42, 1.84)):
		add(geo.cylinder(0.18, 1.05, (x, ly, 0.525), mat=lpg, segments=9, name="lpg"))
		add(geo.cylinder(0.18, 0.12, (x, ly, 1.11), mat=lpg, segments=9, radius_top=0.07, name="lpg_top"))
		add(geo.cylinder(0.11, 0.12, (x, ly, 1.22), mat=steel, segments=6, caps=False, name="lpg_collar"))
		add(geo.tube([(x, ly, 1.2), (x, ly, 1.42), (x, HY + 0.05, 1.5)], 0.012, mat=copper, segments=4, name="lpg_hose"))
	add(geo.tube([(0.75, ly + 0.2, 0.8), (2.1, ly + 0.2, 0.8)], 0.01, mat=steel, segments=3, name="chain"))
	add(geo.tube([(0.8, HY + 0.05, 1.5), (2.0, HY + 0.05, 1.5)], 0.02, mat=copper, segments=4, name="manifold"))
	add(geo.tube([(0.8, HY + 0.06, 1.5), (0.25, HY + 0.06, 1.5), (0.25, HY + 0.06, F2 + 0.5), (0.25, HY, F2 + 0.5)], 0.028, mat=gas, segments=6, name="gas"))
	add(geo.tube([(-0.5, HY, F2 + 1.1), (-0.5, HY + 0.32, F2 + 1.1), (-0.5, HY + 0.32, F2 + 1.75)], 0.06, mat=pipe_m, segments=8, name="flue"))
	add(geo.cone(0.12, 0.1, (-0.5, HY + 0.32, F2 + 1.81), mat=rust, segments=8, name="flue_cap"))
	add(geo.tube([(X1 - 0.12, HY + 0.07, TOP - 0.05), (X1 - 0.12, HY + 0.07, 0.25), (X1 - 0.12, HY + 0.2, 0.08)], 0.05, mat=pipe_m, segments=6, name="drain"))
	add(geo.tube([(X0 - 0.07, -HY + 0.15, TOP - 0.05), (X0 - 0.07, -HY + 0.15, 0.25), (X0 - 0.2, -HY + 0.15, 0.08)], 0.05, mat=pipe_m, segments=6, name="drain"))

	# ---- left side: two small windows, the shop's name painted on the wall ---------------------------------------------
	add(window(left, 1.5, F2 + 0.9, 0.8, 0.8, alu, glass, split=False, sill_m=band, grille_m=steel, rnd=rnd))
	add(window(left, -1.6, F2 + 0.9, 0.8, 0.8, alu, curtain, split=False, sill_m=band, grille_m=steel, rnd=rnd))
	add(left.text("행운슈퍼", 0.5, 0.6, 0.004, 1.75, faded))
	add(left.quad(-1.2, 2.4, 1.3, 1.36, 0.004, faded, "paint_line"))

	# ---- roof: water tank on its stand, a sapling in the corner ----------------------------------------------------------
	tx, ty = 1.5, 1.7
	for (dx, dy) in ((-0.4, -0.4), (0.4, -0.4), (-0.4, 0.4), (0.4, 0.4)):
		add(geo.box((0.06, 0.06, 0.42), (tx + dx, ty + dy, ROOF + 0.21), mat=steel, name="tank_leg"))
	add(drop_faces(geo.box((1.0, 1.0, 0.05), (tx, ty, ROOF + 0.445), mat=steel, name="tank_deck"), lambda nn: nn.z < -0.9))
	add(geo.cylinder(0.5, 0.95, (tx, ty, ROOF + 0.945), mat=tank_m, segments=12, name="tank"))
	add(geo.cylinder(0.2, 0.06, (tx, ty, ROOF + 1.45), mat=tank_m, segments=8, name="tank_lid"))
	add(geo.tube([(tx - 0.5, ty, ROOF + 0.6), (tx - 0.8, ty, ROOF + 0.6), (tx - 0.8, ty, ROOF + 0.02)], 0.035, mat=pipe_m, segments=5, name="tank_pipe"))
	sx_, sy_ = X0 + 0.5, HY - 0.5
	add(geo.tube([(sx_, sy_, ROOF), (sx_ + 0.06, sy_ - 0.04, ROOF + 0.45), (sx_ + 0.14, sy_ - 0.06, ROOF + 0.8)], [0.06, 0.04, 0.025], mat=bark, segments=5, name="sapling"))
	add(geo.tube([(sx_ + 0.06, sy_ - 0.04, ROOF + 0.45), (sx_ - 0.2, sy_ + 0.05, ROOF + 0.7)], [0.025, 0.015], mat=bark, segments=4, name="sapling"))
	for (dx, dy, dz, r, mm) in ((0.18, -0.08, 1.02, 0.42, leaf), (-0.22, 0.06, 0.82, 0.34, leaf_dark), (0.42, -0.22, 0.8, 0.3, leaf_dark), (0.08, 0.15, 1.2, 0.28, leaf)):
		b = geo.blob((r, r, r * 0.8), (sx_ + dx, sy_ + dy, ROOF + dz), mat=mm, subdivisions=1, lumpy=0.3, seed=int(dz * 10 + r * 100), name="sapling_leaf")
		geo.tag(b, "foliage")
		add(b)
	add(moss_pad((0.5, 0.45, 0.07), (sx_ + 0.1, sy_ - 0.05, ROOF - 0.02), (0, 0, 20), moss, 41))

	# A clothesline with a forgotten towel, and a rubber basin gone to weeds.
	for x in (-1.6, 0.4):
		add(geo.tube([(x, -2.5, ROOF), (x, -2.5, ROOF + 1.25)], 0.025, mat=steel, segments=5, name="line_pole"))
	add(geo.tube([(-1.6, -2.5, ROOF + 1.2), (-0.6, -2.5, ROOF + 1.12), (0.4, -2.5, ROOF + 1.2)], 0.008, mat=steel, segments=3, name="line"))
	add(geo.box((0.4, 0.015, 0.55), (-0.85, -2.5, ROOF + 0.86), (0, 0, 4), mat=cloths[1], name="towel"))
	add(geo.cylinder(0.38, 0.3, (-2.5, 1.9, ROOF + 0.15), mat=mat.make("shop_basin", (0.48, 0.26, 0.21), pattern="rust", amount=0.5, rough=0.7, bump=0.2),
					 segments=12, radius_top=0.44, name="basin"))
	add(geo.cylinder(0.4, 0.02, (-2.5, 1.9, ROOF + 0.27), mat=mat.get("soil"), segments=12, name="basin_soil"))
	add(weeds([(-2.5, 1.9, ROOF + 0.27)], (leaf, leaf_dark, moss), rnd, 6, (0.3, 0.6), sway=False))

	# ---- moss and weeds ----------------------------------------------------------------------------------------------
	tops = [(front, X0 - 0.04, -1.6, moss), (front, 1.2, X1 + 0.04, moss_dark), (right, -HY, -1.0, moss), (back, -2.0, 1.5, moss_dark), (left, -2.5, 1.0, moss)]
	for k, (fc, u0, u1, m) in enumerate(tops):
		s = moss_strip(fc.at(u0, -0.1, TOP), fc.at(u1, -0.1, TOP), 0.14, 0.1, m, 30 + k, droop=0.12)
		if k == 0:
			geo.tag(s, "pulse")
		add(s)
	ledges = [(front, -3.9, -1.1, 3.54, 0.11, moss), (front, 0.3, 2.4, 3.54, 0.11, moss_dark), (front, -3.5, -0.3, F2, 0.04, moss_dark),
			  (right, -3.0, 0.5, F2, 0.04, moss), (back, -2.8, 1.0, F2, 0.04, moss), (left, -3.0, 2.0, F2, 0.04, moss_dark)]
	for k, (fc, u0, u1, z, n, m) in enumerate(ledges):
		# On a sign top the moss must not hang over the lettering.
		add(moss_strip(fc.at(u0, n, z), fc.at(u1, n, z), 0.07, 0.07, m, 50 + k, droop=0.03 if z > SHOP else 0.12))
	for k, (x, y, r) in enumerate(((-1.4, 0.4, (0.8, 0.6, 0.08)), (2.2, -2.4, (0.5, 0.6, 0.07)))):
		pad = moss_pad(r, (x, y, ROOF - 0.02), (0, 0, k * 35), moss if k == 0 else moss_dark, 60 + k)
		if k == 0:
			geo.tag(pad, "pulse")
		add(pad)
	for k, (x, y, r) in enumerate(((X0 - 0.06, 0.8, (0.2, 1.0, 0.14)), (-1.0, HY + 0.08, (0.8, 0.2, 0.13)), (SX1 - 0.2, -3.2, (0.3, 0.4, 0.12)),
								   (X0 + 0.4, -HY - 0.08, (0.5, 0.18, 0.12)), (2.0, HY + 0.1, (0.5, 0.2, 0.1)))):
		add(moss_pad(r, (x, y, 0.0), (0, 0, 0), moss_dark if k % 2 else moss, 70 + k, segments=10))
	low_weeds = [(X0 - 0.05, -HY - 0.1, 0.0), (-3.4, -HY - 0.15, 0.0), (0.5, -HY - 0.12, 0.0), (X1 - 0.1, -HY - 0.12, 0.0), (SX1 + 0.05, -3.3, 0.0),
				 (SX1 + 0.05, -0.5, 0.0), (SX1 + 0.05, 2.0, 0.0), (SX1 - 0.3, HY + 0.12, 0.0), (-1.5, HY + 0.15, 0.0), (X0 - 0.1, HY + 0.05, 0.0),
				 (X0 - 0.12, -1.2, 0.0), (X0 - 0.12, 1.8, 0.0), (3.6, -1.9, 1.0)]
	add(weeds(low_weeds, (leaf, leaf_dark, moss), rnd, 4))
	add(weeds([(-2.6, -HY - 0.1, F2 + 0.54), (X0 + 0.3, HY - 0.3, ROOF)], (leaf, leaf_dark), rnd, 2, (0.15, 0.3), sway=False))

	for (p, r) in (((-2.9, -HY - 0.7, 0.75), 0.045), ((sx_ + 0.4, sy_ - 0.3, ROOF + 0.6), 0.045)):
		g = geo.blob(r, p, mat=glow, subdivisions=1, name="spore")
		geo.tag(g, "pulse")
		add(g)

	for i, p in enumerate(shell):
		geo.moss_mask(p, seed=i + 9)

	res = finish("shop_2f", parts, size=1024)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
