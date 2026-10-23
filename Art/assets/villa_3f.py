# A red-brick 다세대 villa over open 필로티 parking: square concrete columns, a scooter under a dusty cover and an
# old bicycle in the shade, the glass entrance with its name, mailboxes and the gas meter; two floors of brick
# with grilled windows, small balconies (a laundry rack on one), AC units on brackets, a couple of windows broken;
# on the green-painted roof a water tank, a TV aerial, a satellite dish and a crate garden gone wild.
# Ivy covers the front third of the right wall and creeps round the corner; moss on the parapet and sills.
import math

import bpy
from mathutils import Matrix, Vector

from assets.office_4f import (Face, ac_unit, bricks, drop_faces, finish, flat_grid, grille, leaf_card, lowres, moss_pad,
							  moss_strip, ring, turn, vine_curtain, weeds, window)
from gse import geo, mat

HX, HY = 4.5, 4.0
PIL = 2.6
F2 = 2.9
F3 = 5.7
ROOF = 8.5
TOP = 9.5


def place(objs, loc, rz, lean=0.0):
	# Moves parts modelled about the origin (front to -Y) to loc, leaning about their length, turned to rz.
	bpy.context.view_layer.update()
	m = Matrix.Translation(Vector(loc)) @ Matrix.Rotation(math.radians(rz), 4, 'Z') @ Matrix.Rotation(math.radians(lean), 4, 'Y')
	for o in objs:
		o.matrix_world = m @ o.matrix_world
	return objs


def patch(face, outline, n, m, name="ivy"):
	# A flat leafy patch on a wall, outline given as (u, z) points.
	area = sum(outline[i][0] * outline[(i + 1) % len(outline)][1] - outline[(i + 1) % len(outline)][0] * outline[i][1] for i in range(len(outline)))
	pts = outline if area > 0 else list(reversed(outline))
	return face.mesh([(u, z, n) for (u, z) in pts], [tuple(range(len(pts)))], m, name)


def leaves_along(face, outline, n, mats, rnd, step=0.32, size=(0.13, 0.22)):
	# Leaf cards scattered along a patch's edge, breaking its outline.
	out = []
	for i in range(len(outline)):
		a, b = Vector(outline[i]), Vector(outline[(i + 1) % len(outline)])
		count = max(1, int((b - a).length / step))
		for k in range(count):
			p = a + (b - a) * ((k + rnd.uniform(0.1, 0.9)) / count)
			lf = leaf_card(face.at(p.x, n + 0.02, p.y), face.n, rnd.uniform(*size), mats[(i + k) % len(mats)], rnd, droop=rnd.uniform(0.0, 0.5))
			geo.tag(lf, "foliage")
			out.append(lf)
	return out


def laundry_rack(cloths, metal_m):
	# A folding A-frame drying rack, a few things left on it; modelled at the origin.
	parts = []
	for x in (-0.55, 0.55):
		parts.append(geo.tube([(x, -0.26, 0.0), (x, 0.0, 1.0), (x, 0.26, 0.0)], 0.014, mat=metal_m, segments=4, name="rack_leg"))
	for (y, z) in ((-0.13, 0.5), (0.0, 1.0), (0.13, 0.5)):
		parts.append(geo.tube([(-0.6, y, z), (0.6, y, z)], 0.011, mat=metal_m, segments=4, name="rack_rod"))
	for (x, w, h, m, tilt) in ((-0.3, 0.36, 0.5, cloths[0], 0), (0.1, 0.4, 0.62, cloths[1], 3), (0.43, 0.2, 0.34, cloths[2], -4)):
		parts.append(geo.box((w, 0.015, h), (x, 0.0, 1.0 - h / 2), (tilt, 0, 0), mat=m, name="cloth"))
	parts.append(geo.box((0.32, 0.27, 0.012), (-0.25, -0.0, 0.5), (0, 0, 0), mat=cloths[2], name="cloth"))
	return parts


def covered_scooter(cover_m, rubber_m):
	# A scooter under a dusty cover, nose to -Y: a draped box with the handlebar hump, wheels showing below.
	cov = geo.box((0.62, 1.75, 1.0), (0, 0, 0.0), mat=cover_m, subdiv=2, name="cover")
	for v in cov.data.vertices:
		x, y, z = v.co
		h = z + 0.5
		v.co.x = x * (1.0 - 0.35 * h)
		if y < -0.3:
			v.co.z = z + 0.28 * h * min(1.0, (-0.3 - y) / 0.5)
		elif y > 0.2:
			v.co.z = z - 0.14 * h
	cov.data.update()
	for p in cov.data.polygons:
		p.use_smooth = True
	cov.location = (0, 0, 0.52)
	geo.jitter(cov, 0.03, 4.0, seed=5)
	parts = [cov]
	for y in (-0.62, 0.62):
		parts.append(geo.cylinder(0.21, 0.09, (0, y, 0.2), (0, 90, 0), mat=rubber_m, segments=10, name="tyre"))
	parts.append(geo.tube([(0.24, -0.62, 1.1), (0.36, -0.66, 1.2)], 0.012, mat=rubber_m, segments=4, name="mirror"))
	return parts


def bicycle(frame_m, rubber_m, seat_m):
	# An old upright bicycle along Y, wheels as thin hoops.
	parts = []
	R = 0.32
	for cy in (-0.52, 0.52):
		pts = [(0.0, cy + R * math.cos(a / 10.0 * math.tau), R + R * math.sin(a / 10.0 * math.tau)) for a in range(11)]
		parts.append(geo.tube(pts, 0.022, mat=rubber_m, segments=3, caps=False, name="bike_wheel"))
	hub_r, hub_f, bb, seat, head = (0, 0.52, R), (0, -0.52, R), (0, 0.05, 0.3), (0, 0.24, 0.84), (0, -0.36, 0.86)
	for path in ((hub_r, bb, head, hub_f), (hub_r, seat, bb), (seat, head)):
		parts.append(geo.tube(list(path), 0.018, mat=frame_m, segments=4, name="bike_frame"))
	parts.append(geo.tube([head, (0, -0.4, 1.0)], 0.016, mat=frame_m, segments=4, name="bike_stem"))
	parts.append(geo.tube([(-0.26, -0.36, 1.02), (0.0, -0.42, 1.0), (0.26, -0.36, 1.02)], 0.014, mat=frame_m, segments=4, name="bike_bar"))
	parts.append(geo.box((0.12, 0.24, 0.06), (0, 0.24, 0.89), mat=seat_m, name="bike_seat"))
	parts.append(geo.box((0.32, 0.26, 0.2), (0, -0.6, 0.86), mat=frame_m, name="bike_basket"))
	return parts


def build():
	brick = bricks("villa_brick", (0.47, 0.26, 0.20), (0.62, 0.59, 0.54), 0.30, 0.10, joint=0.02, vary=0.18, pattern="plaster", rough=0.9, bump=0.3)
	band = mat.make("villa_band", (0.58, 0.56, 0.50), pattern="concrete", rough=0.9, bump=0.3)
	column_m = mat.make("villa_column", (0.55, 0.54, 0.50), pattern="concrete", rough=0.92, bump=0.3)
	floor_m = mat.make("villa_parking", (0.25, 0.25, 0.24), pattern="concrete", rough=0.95, bump=0.3, grime=0.5)
	green = mat.make("villa_roof_green", (0.30, 0.42, 0.34), pattern="concrete", rough=0.85, bump=0.25)
	coping = mat.make("villa_coping", (0.34, 0.45, 0.37), pattern="rust", accent=(0.52, 0.52, 0.49), amount=1.0, rough=0.85, bump=0.25)
	alu = mat.make("villa_alu", (0.60, 0.61, 0.60), rough=0.5, bump=0.1, grime=0.45)
	glass = mat.make("villa_glass", (0.16, 0.20, 0.23), pattern="plaster", var=0.3, rough=0.2, grime=0.15, edge=0.05, bump=0.05)
	curtain = mat.make("villa_curtain", (0.55, 0.50, 0.40), pattern="cloth", rough=0.9, bump=0.2)
	dark = mat.make("villa_dark", (0.05, 0.045, 0.04), rough=0.95, grime=0.0, edge=0.0, bump=0.0)
	steel = mat.make("villa_steel", (0.28, 0.29, 0.29), pattern="rust", accent="rust", amount=1.0, rough=0.75, bump=0.4)
	rust = mat.get("rust")
	door_m = mat.make("villa_door", (0.40, 0.33, 0.27), pattern="rust", amount=0.8, rough=0.7, bump=0.35)
	ac_m = mat.make("villa_ac", (0.66, 0.66, 0.62), pattern="rust", amount=0.3, rough=0.6, bump=0.2, grime=0.5)
	fan_m = mat.make("villa_fan", (0.20, 0.20, 0.20), rough=0.7, bump=0.1)
	gas = mat.make("villa_gas", (0.62, 0.52, 0.22), pattern="rust", amount=0.5, rough=0.6, bump=0.2)
	meter_m = mat.make("villa_meter", (0.58, 0.58, 0.55), rough=0.6, bump=0.1, grime=0.5)
	pipe_m = mat.make("villa_pipe", (0.56, 0.56, 0.53), rough=0.6, bump=0.1, grime=0.5)
	mailbox_m = mat.make("villa_mailbox", (0.48, 0.50, 0.50), pattern="rust", amount=0.6, rough=0.6, bump=0.2)
	mailflap = mat.make("villa_mailflap", (0.62, 0.63, 0.61), rough=0.55, bump=0.1)
	name_m = mat.make("villa_name", (0.68, 0.58, 0.34), rough=0.45, bump=0.05, edge=0.1)
	paint = mat.make("villa_paint", (0.66, 0.64, 0.56), rough=0.9, bump=0.0, edge=0.0)
	paper = mat.make("villa_paper", (0.78, 0.76, 0.68), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	cover_m = mat.make("villa_cover", (0.44, 0.43, 0.39), pattern="cloth", rough=0.95, bump=0.4, grime=0.5)
	rubber = mat.get("rubber")
	bike_m = mat.make("villa_bike", (0.24, 0.33, 0.30), pattern="rust", accent="rust", amount=1.1, rough=0.7, bump=0.3)
	cloths = (mat.make("villa_cloth_blue", (0.42, 0.50, 0.58), pattern="cloth", rough=0.95, bump=0.3),
			  mat.make("villa_cloth_pink", (0.66, 0.50, 0.48), pattern="cloth", rough=0.95, bump=0.3),
			  mat.make("villa_cloth_white", (0.76, 0.75, 0.70), pattern="cloth", rough=0.95, bump=0.3))
	tank_m = mat.make("villa_tank", (0.66, 0.66, 0.60), pattern="plaster", rough=0.6, bump=0.2, grime=0.5)
	dish_m = mat.make("villa_dish", (0.70, 0.70, 0.67), pattern="rust", amount=0.35, rough=0.55, bump=0.15)
	styro = mat.make("villa_styro", (0.74, 0.73, 0.68), pattern="plaster", rough=0.92, bump=0.2, grime=0.5)
	crate_m = mat.make("villa_crate", (0.50, 0.26, 0.21), pattern="rust", accent=(0.46, 0.36, 0.30), amount=0.6, rough=0.7, bump=0.25)
	pot_m = mat.get("onggi")
	soil = mat.get("soil")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	ivy = mat.make("villa_ivy", (0.20, 0.33, 0.20), pattern="leaf", rough=0.8, bump=0.45)
	bark = mat.get("bark")
	dry = mat.make("villa_dry_stalk", (0.46, 0.40, 0.28), pattern="wood", rough=0.9, bump=0.3)
	glow = mat.get("spore_glow")
	rnd = geo.rng(17)

	parts = []
	shell = []

	def add(objs, mossy=False):
		if not isinstance(objs, (list, tuple)):
			objs = [objs]
		parts.extend(objs)
		if mossy:
			shell.extend(objs)
		return objs

	front = Face(0, HY, HX)
	right = Face(90, HX, HY)
	back = Face(180, HY, HX)
	left = Face(270, HX, HY)
	floors = (F2, F3)

	# ---- shell: brick body on the slab, bands, parapet with green paint over the coping ---------------------------
	for fc in (front, right, back, left):
		add(fc.wall(-fc.half, fc.half, F2, TOP - 0.12, brick, cols=3, rows=3, name="wall"), True)
	add(ring(-HX - 0.05, HX + 0.05, -HY - 0.05, HY + 0.05, PIL, F2, 0.05, band, "slab_band"), True)
	add(ring(-HX - 0.05, HX + 0.05, -HY - 0.05, HY + 0.05, F3 - 0.08, F3 + 0.08, 0.05, band, "mid_band"), True)
	add(ring(-HX, HX, -HY, HY, ROOF, TOP - 0.12, 0.2, green, "parapet_in", top=False, inner=ROOF, outer=False))
	add(ring(-HX - 0.04, HX + 0.04, -HY - 0.04, HY + 0.04, TOP - 0.12, TOP, 0.28, coping, "coping", inner=TOP - 0.12), True)
	add(flat_grid(-HX + 0.2, HX - 0.2, -HY + 0.2, HY - 0.2, ROOF, 4, 4, green, "roof"), True)

	# ---- piloti: columns, the stair core with the entrance, walls behind, the parking floor --------------------------
	add(lowres(flat_grid(-1.9, HX, -HY, HY - 0.2, 0.03, 3, 3, floor_m, "parking"), 0.6), True)
	for x in (-1.7, 1.25):
		add(geo.box((0.1, 3.0, 0.004), (x, -2.1, 0.034), mat=paint, name="stripe"))
	add(geo.box((0.9, 0.7, 0.004), (1.5, -1.0, 0.033), (0, 0, 20), mat=dark, name="oil_stain"))
	for (x, y) in ((1.25, -HY + 0.225), (HX - 0.225, -HY + 0.225), (HX - 0.225, -0.2), (1.25, -0.2), (HX - 0.225, HY - 0.225)):
		add(drop_faces(geo.box((0.45, 0.45, PIL), (x, y, PIL / 2), mat=column_m, name="column", bevel=0.025, bevel_segments=1), lambda nn: abs(nn.z) > 0.9), True)
	add(drop_faces(geo.box((2.6, 3.4, PIL), (-3.2, -2.3, PIL / 2), mat=brick, name="core"), lambda nn: abs(nn.z) > 0.9 or nn.y > 0.9), True)
	add(lowres(drop_faces(geo.box((HX * 2, 0.2, PIL), (0.0, HY - 0.1, PIL / 2), mat=brick, name="back_wall"), lambda nn: abs(nn.z) > 0.9), 0.6), True)
	add(drop_faces(geo.box((0.2, 4.4, PIL), (-HX + 0.1, 1.6, PIL / 2), mat=brick, name="side_wall"), lambda nn: abs(nn.z) > 0.9 or nn.x > 0.9), True)

	# Entrance: glass door under a little canopy, the villa's name on the slab band, mailboxes beside it.
	add(front.frame(-3.55, 0.0, 1.0, 2.15, 0.07, 0.08, alu))
	add(lowres(front.quad(-4.05, -3.05, 0.0, 2.15, 0.01, glass), 0.5))
	add(front.box((0.04, 0.05, 2.15), -3.55, 0.03, 1.075, alu, "door_stile"))
	add(front.box((0.03, 0.05, 0.45), -3.45, 0.07, 1.05, alu, "handle"))
	add(drop_faces(front.box((1.5, 0.55, 0.08), -3.55, 0.275, 2.38, band, "canopy"), lambda nn: nn.z < -0.9))
	add(front.text("햇살빌라", 0.24, -3.2, 0.054, 2.75, name_m))
	add(front.box((0.78, 0.16, 0.5), -2.45, 0.08, 1.45, mailbox_m, "mailbox"))
	for i in range(3):
		for j in range(2):
			u = -2.7 + i * 0.25
			z = 1.33 + j * 0.24
			add(front.quad(u - 0.1, u + 0.1, z - 0.09, z + 0.09, 0.162, mailflap, "flap"))
			add(front.quad(u - 0.06, u + 0.06, z + 0.04, z + 0.06, 0.164, dark, "slot"))
	add(front.quad(-4.45, -4.15, 1.5, 1.9, 0.004, paper, "notice"))

	# Gas meter and its yellow pipe on the core's parking side; the scooter and the bicycle in the shade.
	coreside = Face(90, -1.9)
	add(coreside.box((0.34, 0.2, 0.42), -3.4, 0.1, 1.5, meter_m, "gas_meter"))
	add(geo.tube([coreside.at(-3.4, 0.06, 0.3), coreside.at(-3.4, 0.06, 1.28)], 0.03, mat=gas, segments=6, name="gas"))
	add(geo.tube([coreside.at(-3.4, 0.06, 1.72), coreside.at(-3.4, 0.06, 2.55), coreside.at(-3.4, 0.06, 2.6)], 0.03, mat=gas, segments=6, name="gas"))
	add(place(covered_scooter(cover_m, rubber), (3.1, -2.7, 0.0), 8))
	add(place(bicycle(bike_m, rubber, mat.get("boots")), (-1.72, -1.9, 0.0), 0, -11))

	# Back of the piloti: a steel door and a vent in the wall.
	add(back.frame(-2.5, 0.0, 0.9, 2.05, 0.07, 0.07, steel))
	add(back.box((0.9, 0.04, 2.05), -2.5, 0.03, 1.03, door_m, "back_door"))
	add(grille(back, 1.0, 1.6, 0.6, 0.4, steel, 0.02, gap=0.1))
	add(back.quad(0.7, 1.3, 1.6, 2.0, 0.005, dark, "vent"))

	# ---- upper floors ----------------------------------------------------------------------------------------------
	for k, f in enumerate(floors):
		top = k == 1
		# Front: the stair window over the core, the living-room window, the balcony.
		add(window(front, -3.2, f + 1.35, 0.7, 0.8, alu, glass, grille_m=steel, split=False, sill_m=band, rnd=rnd))
		add(window(front, -0.3, f + 0.85, 2.2, 1.4, alu, curtain if not top else glass, kind="broken" if top else "glass",
				   sill_m=band, grille_m=steel, rnd=rnd, dark_m=dark))
		add(drop_faces(front.box((2.7, 0.62, 0.14), 2.6, 0.31, f - 0.07, band, "balcony"), lambda nn: nn.y > 0.9 or nn.z < -0.9), True)
		add(window(front, 2.6, f + 0.08, 2.1, 2.0, alu, glass if top else curtain, rnd=rnd))
		add(grille(front, 2.6, f + 0.12, 2.56, 0.8, steel, 0.6, gap=0.14))
		for u in (1.3, 3.9):
			add(geo.tube([front.at(u, 0.0, f + 0.95), front.at(u, 0.6, f + 0.95), front.at(u, 0.6, f)], 0.02, mat=steel, segments=4, name="rail"))
		add(geo.tube([front.at(1.3, 0.6, f + 0.95), front.at(3.9, 0.6, f + 0.95)], 0.025, mat=steel, segments=4, name="rail"))
		if not top:
			rack = laundry_rack(cloths, steel)
			add(place(rack, front.at(2.45, 0.3, f), 0))
			add(ac_unit(front.at(-2.15, 0.0, f + 0.12), 0, ac_m, fan_m, steel))
		else:
			add(ac_unit(front.at(3.3, 0.04, f), 0, ac_m, fan_m, steel, bracket=False))
			for (u, h) in ((1.6, 0.26), (1.95, 0.22), (2.3, 0.3)):
				add(geo.cylinder(0.13, h, front.at(u, 0.3, f + h / 2), mat=pot_m, segments=8, radius_top=0.16, name="pot"))
				for j in range(3):
					add(geo.tube([front.at(u, 0.3, f + h), front.at(u + rnd.uniform(-0.12, 0.12), 0.3 + rnd.uniform(-0.1, 0.1), f + h + rnd.uniform(0.25, 0.45))],
								 0.008, mat=dry, segments=3, name="stalk"))
		# Right: two grilled windows, the far one broken upstairs; an AC unit on the top floor.
		add(window(right, -1.0, f + 0.9, 1.4, 1.2, alu, glass, sill_m=band, grille_m=steel, rnd=rnd))
		add(window(right, 2.3, f + 1.0, 1.0, 1.0, alu, glass, kind="broken" if top else "glass", sill_m=band, grille_m=steel, rnd=rnd, dark_m=dark))
		# Back: two rooms and a bathroom.
		add(window(back, -2.8, f + 1.0, 1.2, 1.0, alu, curtain if top else glass, sill_m=band, grille_m=steel, rnd=rnd))
		add(window(back, 1.2, f + 1.0, 1.2, 1.0, alu, glass, sill_m=band, grille_m=steel, rnd=rnd))
		add(window(back, 3.4, f + 1.5, 0.6, 0.6, alu, glass, split=False, sill_m=band, rnd=rnd))
		# Left: the stair window and a small one.
		add(window(left, 2.3, f + 1.4, 0.6, 0.9, alu, glass, split=False, sill_m=band, grille_m=steel, rnd=rnd))
		add(window(left, -1.6, f + 1.1, 0.8, 0.8, alu, glass, split=False, sill_m=band, grille_m=steel, rnd=rnd))
	add(ac_unit(right.at(0.75, 0.0, F3 + 0.15), 90, ac_m, fan_m, steel))
	add(ac_unit(back.at(-1.2, 0.0, F2 + 0.1), 180, ac_m, fan_m, steel))
	add(ac_unit(back.at(2.4, 0.0, F3 + 0.1), 180, ac_m, fan_m, steel))
	for (fc, u, z) in ((front, -0.9, F2 + 0.75), (right, -0.6, F2 + 0.8), (back, -2.4, F3 + 0.9), (front, 0.6, F3 + 0.75)):
		add(fc.quad(u, u + 0.04, z - rnd.uniform(0.5, 0.9), z, 0.003, rust, "streak"))

	# Gas pipes up the back to each flat, the meters at the bottom; drainpipes at two corners.
	gy = HY + 0.06
	add(geo.tube([(-0.3, gy, 0.3), (-0.3, gy, 1.12)], 0.03, mat=gas, segments=6, name="gas"))
	add(back.box((0.34, 0.2, 0.42), 0.3, 0.1, 1.33, meter_m, "gas_meter"))
	add(geo.tube([(-0.3, gy, 1.55), (-0.3, gy, F3 + 0.6), (0.2, gy, F3 + 0.6), (0.2, HY, F3 + 0.6)], 0.03, mat=gas, segments=6, name="gas"))
	add(geo.tube([(-0.3, gy, F2 + 0.6), (0.2, gy, F2 + 0.6), (0.2, HY, F2 + 0.6)], 0.03, mat=gas, segments=6, name="gas"))
	add(geo.tube([(-HX + 0.15, HY + 0.07, TOP - 0.1), (-HX + 0.15, HY + 0.07, 0.25), (-HX + 0.15, HY + 0.2, 0.08)], 0.05, mat=pipe_m, segments=6, name="drain"))
	add(geo.tube([(-HX - 0.07, -HY + 0.15, TOP - 0.1), (-HX - 0.07, -HY + 0.15, 0.25), (-HX - 0.2, -HY + 0.15, 0.08)], 0.05, mat=pipe_m, segments=6, name="drain"))

	# ---- roof: tank, aerial, dish, the roof hatch and a crate garden gone wild ---------------------------------------
	add(drop_faces(geo.box((1.4, 1.4, 0.3), (2.8, 2.5, ROOF + 0.15), mat=column_m, name="tank_base"), lambda nn: nn.z < -0.9), True)
	add(geo.cylinder(0.64, 1.25, (2.8, 2.5, ROOF + 0.925), mat=tank_m, segments=14, name="tank"))
	for z in (ROOF + 0.6, ROOF + 1.25):
		add(geo.cylinder(0.655, 0.06, (2.8, 2.5, z), mat=tank_m, segments=14, caps=False, name="tank_rib"))
	add(geo.cylinder(0.24, 0.08, (2.8, 2.5, ROOF + 1.59), mat=tank_m, segments=10, name="tank_lid"))
	add(geo.tube([(2.2, 2.5, ROOF + 0.45), (1.8, 2.5, ROOF + 0.45), (1.8, 2.5, ROOF + 0.02)], 0.04, mat=pipe_m, segments=6, name="tank_pipe"))

	ax, ay = -3.5, HY - 0.32
	add(geo.tube([(ax, ay, ROOF), (ax, ay, TOP + 0.95)], 0.025, mat=steel, segments=5, name="mast"))
	for z in (TOP - 0.3, ROOF + 0.3):
		add(geo.box((0.08, 0.14, 0.06), (ax, ay + 0.1, z), mat=steel, name="mast_clamp"))
	boom = [geo.tube([(ax - 0.65, ay, TOP + 0.85), (ax + 0.55, ay, TOP + 0.85)], 0.015, mat=steel, segments=4, name="boom")]
	for k in range(6):
		x = ax - 0.6 + k * 0.22
		h = 0.32 - k * 0.03
		boom.append(geo.tube([(x, ay - h, TOP + 0.85), (x, ay + h, TOP + 0.85)], 0.008, mat=steel, segments=3, name="element"))
	turn(boom, (ax, ay, TOP + 0.85), (0, 0, 1), 18)
	turn(boom, (ax, ay, TOP + 0.85), (0, 1, 0), 9)
	add(boom)

	dx, dy = 3.55, -2.85
	add(geo.tube([(dx, dy, ROOF), (dx, dy, ROOF + 0.55)], 0.03, mat=steel, segments=5, name="dish_post"))
	add(drop_faces(geo.box((0.4, 0.4, 0.05), (dx, dy, ROOF + 0.025), mat=steel, name="dish_foot"), lambda nn: nn.z < -0.9))
	dish = [geo.lathe([(0.0, 0.0), (0.16, 0.015), (0.3, 0.06), (0.36, 0.1)], (0, 0, 0), mat=dish_m, segments=12, name="dish", close_top=False),
			geo.tube([(0.0, 0.0, 0.02), (0.0, -0.05, 0.42)], 0.012, mat=steel, segments=3, name="lnb_arm"),
			geo.box((0.06, 0.06, 0.09), (0.0, -0.05, 0.44), mat=dish_m, name="lnb")]
	turn(dish, (0, 0, 0), (1, 0, 0), 62)
	add(place(dish, (dx, dy - 0.05, ROOF + 0.62), 12))

	add(drop_faces(geo.box((1.0, 1.0, 0.5), (-3.25, -1.25, ROOF + 0.25), mat=column_m, name="hatch"), lambda nn: nn.z < -0.9), True)
	add(geo.box((1.1, 1.1, 0.07), (-3.25, -1.25, ROOF + 0.535), (0, 3, 0), mat=steel, name="hatch_lid"))

	# The crate garden: styrofoam boxes, a crate and a rubber basin, all run wild; a squash vine spills over the parapet.
	beds = [((-3.3, -3.2), (0.62, 0.42, 0.3), styro), ((-2.6, -3.25), (0.62, 0.42, 0.3), styro), ((-1.9, -3.25), (0.55, 0.38, 0.28), crate_m),
			((-3.0, -2.6), (0.62, 0.42, 0.3), styro), ((-2.3, -2.62), (0.62, 0.42, 0.3), styro), ((-1.2, -3.2), (0.62, 0.42, 0.3), styro),
			((-0.5, -3.22), (0.55, 0.38, 0.28), crate_m)]
	for k, ((x, y), (w, d, h), m) in enumerate(beds):
		add(drop_faces(geo.box((w, d, h), (x, y, ROOF + h / 2), (0, 0, rnd.uniform(-6, 6)), mat=m, name="planter"), lambda nn: nn.z < -0.9), True)
		add(geo.box((w - 0.06, d - 0.06, 0.03), (x, y, ROOF + h - 0.01), mat=soil, name="planter_soil"))
	add(geo.cylinder(0.36, 0.3, (-1.6, -2.55, ROOF + 0.15), mat=crate_m, segments=12, radius_top=0.42, name="basin"))
	lush = mat.make("villa_leaf_light", (0.27, 0.41, 0.23), pattern="leaf", rough=0.8, bump=0.45)
	for (c, r, m, s) in (((-3.25, -3.2, ROOF + 0.6), (0.48, 0.36, 0.42), lush, 2), ((-2.5, -3.2, ROOF + 0.72), (0.44, 0.34, 0.5), leaf, 2),
						 ((-2.95, -2.55, ROOF + 0.58), (0.42, 0.34, 0.36), leaf_dark, 1), ((-2.2, -2.6, ROOF + 0.55), (0.36, 0.3, 0.32), lush, 1),
						 ((-1.15, -3.15, ROOF + 0.6), (0.42, 0.32, 0.4), lush, 2), ((-1.6, -2.55, ROOF + 0.48), (0.36, 0.34, 0.3), leaf_dark, 1),
						 ((-0.5, -3.2, ROOF + 0.5), (0.32, 0.28, 0.3), leaf, 1)):
		b = geo.blob(r, c, mat=m, subdivisions=s, lumpy=0.35, seed=int(-c[0] * 10), name="garden")
		geo.tag(b, "foliage")
		add(b)
	spill = vine_curtain(front, -2.95, -1.55, TOP + 0.05, [0.5, 1.1, 1.7, 1.3, 0.8, 1.4, 0.6], 0.08, ivy, 4, rows=3, name="spill")
	lip = moss_strip(front.at(-3.05, -0.1, TOP), front.at(-1.45, -0.1, TOP), 0.17, 0.14, ivy, 9, droop=0.3)
	for o in (spill, lip):
		geo.tag(o, "foliage")
		add(o)
	vine = [(-2.2, -3.25, ROOF + 0.4), (-2.2, -3.75, TOP + 0.12), (-2.25, -HY - 0.12, TOP - 0.1)]
	add(geo.tube(vine, [0.03, 0.025, 0.02], mat=leaf_dark, segments=4, name="squash_vine"))
	for k in range(16):
		p = front.at(rnd.uniform(-2.95, -1.55), 0.12, TOP - rnd.uniform(0.0, 1.4))
		lf = leaf_card(p, front.n, rnd.uniform(0.2, 0.3), (leaf, lush, leaf_dark)[k % 3], rnd, droop=0.5)
		geo.tag(lf, "foliage")
		add(lf)
	for j in range(5):
		add(geo.tube([(-1.9 + rnd.uniform(-0.15, 0.15), -3.25, ROOF + 0.25), (-1.9 + rnd.uniform(-0.3, 0.3), -3.25 + rnd.uniform(-0.2, 0.2), ROOF + rnd.uniform(1.1, 1.6))],
					 0.014, mat=dry, segments=3, name="stalk"))
	add(weeds([(-3.3, -3.2, ROOF + 0.3), (-2.6, -3.25, ROOF + 0.3), (-1.2, -3.2, ROOF + 0.3), (-2.3, -2.62, ROOF + 0.3)], (leaf, lush), rnd, 3, (0.4, 0.75), sway=False))

	# The villa's name in metal letters across the front parapet.
	add(front.text("햇살빌라", 0.55, 0.0, 0.012, TOP - 0.55, name_m))

	# ---- ivy over the front third of the right wall, round the corner and up the corner column ---------------------
	ivy_right = [(-4.06, 2.55), (-3.1, 2.62), (-2.6, 3.5), (-1.9, 4.1), (-1.45, 5.3), (-1.7, 6.5), (-2.3, 7.3), (-2.9, 8.0), (-3.5, 8.65), (-4.06, 8.45)]
	ivy_front = [(4.56, 2.55), (4.56, 7.1), (4.35, 6.6), (4.1, 5.9), (3.98, 5.0), (4.06, 4.0), (3.96, 3.2), (4.05, 2.55)]
	col_front = Face(0, HY)
	col_right = Face(90, HX)
	ivy_col_f = [(4.05, 0.0), (4.5, 0.0), (4.5, 2.6), (4.2, 2.6), (4.1, 1.6), (4.15, 0.7)]
	ivy_col_r = [(-4.0, 0.0), (-3.6, 0.0), (-3.6, 0.9), (-3.72, 1.9), (-4.0, 2.6)]
	for (fc, outline, n) in ((right, ivy_right, 0.07), (front, ivy_front, 0.07), (col_front, ivy_col_f, 0.02), (col_right, ivy_col_r, 0.02)):
		sheet = patch(fc, outline, n, ivy)
		geo.tag(sheet, "foliage")
		add(sheet)
		add(leaves_along(fc, outline, n, (leaf, leaf_dark, ivy), rnd))
	for k in range(34):
		u = rnd.uniform(-3.9, -2.3)
		z = rnd.uniform(2.9, 7.6 - max(0.0, u + 3.2) * 1.2)
		lf = leaf_card(right.at(u, 0.1, z), right.n, rnd.uniform(0.16, 0.24), leaf if k % 2 else leaf_dark, rnd, droop=0.3)
		geo.tag(lf, "foliage")
		add(lf)

	# ---- moss on the coping, sills and balcony edges; weeds at the foot ---------------------------------------------
	tops = [(front, -4.5, -3.1, moss), (front, 1.6, 4.5, moss_dark), (right, -4.0, -0.4, moss), (back, -1.5, 3.0, moss_dark), (left, -3.8, -0.5, moss)]
	for k, (fc, u0, u1, m) in enumerate(tops):
		s = moss_strip(fc.at(u0, -0.1, TOP), fc.at(u1, -0.1, TOP), 0.14, 0.1, m, 30 + k, droop=0.12)
		if k == 0:
			geo.tag(s, "pulse")
		add(s)
	ledges = [(front, -1.4, 0.8, F3 + 0.08, 0.05, moss), (front, 1.35, 3.85, F2, 0.6, moss_dark), (right, 0.2, 3.5, F3 + 0.08, 0.05, moss_dark),
			  (back, -3.4, -0.6, F2, 0.05, moss), (left, -3.6, -0.4, F3 + 0.08, 0.05, moss)]
	for k, (fc, u0, u1, z, n, m) in enumerate(ledges):
		add(moss_strip(fc.at(u0, n - 0.03, z), fc.at(u1, n - 0.03, z), 0.07, 0.07, m, 50 + k, droop=0.12))
	sills = [(right, -1.6, -0.6, F2 + 0.84, moss), (back, 0.75, 1.6, F3 + 0.94, moss_dark), (front, -1.3, -0.3, F3 + 0.79, moss), (left, -1.95, -1.3, F2 + 1.04, moss_dark)]
	for k, (fc, u0, u1, z, m) in enumerate(sills):
		add(moss_strip(fc.at(u0, 0.06, z), fc.at(u1, 0.06, z), 0.05, 0.05, m, 80 + k, droop=0.08))
	for k, (x, y, r) in enumerate(((0.6, 1.4, (0.9, 0.6, 0.08)), (-1.8, 2.8, (0.7, 0.5, 0.07)), (3.6, -0.6, (0.5, 0.8, 0.07)))):
		pad = moss_pad(r, (x, y, ROOF - 0.02), (0, 0, k * 35), moss if k != 1 else moss_dark, 60 + k)
		if k == 0:
			geo.tag(pad, "pulse")
		add(pad)
	for k, (x, y, r) in enumerate(((-HX - 0.05, 1.5, (0.2, 1.0, 0.14)), (-0.6, HY + 0.08, (0.9, 0.2, 0.13)), (HX - 0.2, -HY + 0.1, (0.4, 0.3, 0.12)),
								   (-4.4, -HY - 0.05, (0.5, 0.18, 0.12)), (2.9, -0.2, (0.5, 0.35, 0.08)))):
		add(moss_pad(r, (x, y, 0.0), (0, 0, 0), moss_dark if k % 2 else moss, 70 + k, segments=8))
	low_weeds = [(-4.6, -4.15, 0.0), (-1.7, -4.12, 0.0), (1.25, -4.15, 0.0), (4.6, -4.1, 0.0), (4.62, -1.8, 0.0), (4.6, 1.8, 0.0), (4.62, 3.9, 0.0),
				 (2.0, 4.15, 0.0), (-2.0, 4.15, 0.0), (-4.65, 3.6, 0.0), (-4.65, 0.4, 0.0), (0.4, -3.4, 0.0), (3.6, -0.4, 0.0)]
	add(weeds(low_weeds, (leaf, leaf_dark, moss), rnd, 4))
	add(weeds([(-0.3, -HY - 0.06, F3 + 0.79), (HX + 0.06, 2.3, F3 + 0.94)], (leaf, leaf_dark), rnd, 2, (0.12, 0.25), sway=False))

	for (p, r) in (((3.3, -1.6, 0.6), 0.045), ((-2.4, -2.6, ROOF + 1.05), 0.045)):
		g = geo.blob(r, p, mat=glow, subdivisions=1, name="spore")
		geo.tag(g, "pulse")
		add(g)

	for i, p in enumerate(shell):
		geo.moss_mask(p, seed=i + 5)

	res = finish("villa_3f", parts, size=1024)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
