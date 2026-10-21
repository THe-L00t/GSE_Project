# An abandoned 1-ton pickup (Porter style), faded sky blue, sunk on flat tyres, the windscreen cracked,
# the bed full of junk under a thick blanket of moss. The cab is at +Y: this truck's nose faces +Y.
from mathutils import Vector

from gse import geo, mat, pipeline

# The windscreen plane: centre, across, up the glass, out of the glass.
SCREEN_C = Vector((0.0, 1.9918, 1.5424))
SCREEN_U = Vector((1.0, 0.0, 0.0))
SCREEN_V = Vector((0.0, -0.2017, 0.9794))
SCREEN_N = Vector((0.0, 0.9794, 0.2017))


def on_screen(u, v, out=0.026):
	return tuple(SCREEN_C + SCREEN_U * u + SCREEN_V * v + SCREEN_N * out)


def wheel(parts, x, y, squash, rubber, metal):
	r = 0.31
	z = r * squash
	tyre = geo.cylinder(r, 0.22, (x, y, z), (0, 90, 0), mat=rubber, segments=16, name="tyre")
	tyre.scale = (squash, 1.0, 1.0)
	parts.append(tyre)
	side = 1.0 if x > 0.0 else -1.0
	parts.append(geo.cylinder(0.17, 0.232, (x, y, z + 0.02), (0, 90, 0), mat=metal, segments=10, name="rim"))
	parts.append(geo.cylinder(0.06, 0.04, (x + side * 0.11, y, z + 0.02), (0, 90, 0), mat=metal, segments=8, name="hub"))


def build():
	paint = mat.make("truck_paint", (0.42, 0.51, 0.54), pattern="rust", rough=0.72, bump=0.35, amount=0.7)
	gate = mat.make("truck_gate", (0.38, 0.41, 0.41), pattern="rust", rough=0.75, bump=0.4, amount=0.9)
	metal = mat.get("metal")
	rust = mat.get("rust")
	glass = mat.get("glass")
	crack = mat.make("truck_crack", (0.58, 0.64, 0.64), rough=0.3, grime=0.0, edge=0.0, bump=0.0)
	rubber = mat.get("rubber")
	bone = mat.get("bone")
	plate = mat.make("truck_plate", (0.60, 0.53, 0.30), pattern="rust", rough=0.7, bump=0.2, amount=0.5)
	amber = mat.make("truck_indicator", (0.60, 0.42, 0.22), rough=0.45, bump=0.05)
	crate = mat.get("planks")
	canvas = mat.get("canvas")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	glow = mat.get("spore_glow")

	parts = []
	shell = []

	# Cab-over cab, side profile swept across the width: the notch is the front wheel arch.
	cab = geo.polygon_slab([(0.68, 0.40), (0.98, 0.40), (1.01, 0.52), (1.12, 0.62), (1.38, 0.66), (1.64, 0.62),
							(1.75, 0.52), (1.78, 0.40), (2.04, 0.40), (2.05, 1.20), (1.91, 1.88), (0.68, 1.88)],
						   1.72, loc=(-0.86, 0.0, 0.0), rot=(90, 0, 90), mat=paint, bevel=0.07, name="cab")
	shell.append(cab)
	parts.append(geo.box((1.50, 0.03, 0.56), on_screen(0.0, 0.0, 0.012), (11.63, 0, 0), mat=glass, bevel=0.01, name="windscreen"))
	for sx in (-1.0, 1.0):
		parts.append(geo.box((0.03, 0.75, 0.46), (sx * 0.866, 1.30, 1.56), mat=glass, name="side_window"))
		parts.append(geo.box((0.014, 0.014, 1.05), (sx * 0.868, 0.88, 1.30), mat=metal, name="door_seam"))
		parts.append(geo.box((0.02, 0.12, 0.035), (sx * 0.872, 0.98, 1.24), mat=metal, name="door_handle"))
		parts.append(geo.box((0.30, 0.04, 0.16), (sx * 0.58, 2.045, 0.80), mat=bone if sx > 0 else glass, bevel=0.02, bevel_segments=1, name="headlight"))
		parts.append(geo.box((0.11, 0.04, 0.10), (sx * 0.80, 2.042, 0.80), mat=amber, name="indicator"))
	parts.append(geo.box((0.90, 0.03, 0.36), (0.0, 0.668, 1.56), mat=glass, name="rear_window"))
	parts.append(geo.box((0.78, 0.03, 0.20), (0.0, 2.052, 0.98), mat=metal, name="grille"))
	for z in (0.94, 1.02):
		parts.append(geo.box((0.70, 0.02, 0.022), (0.0, 2.066, z), mat=rubber, name="grille_slot"))
	bumper = geo.box((1.82, 0.16, 0.16), (0.0, 2.10, 0.40), mat=metal, bevel=0.03, name="bumper")
	shell.append(bumper)
	parts.append(geo.box((0.34, 0.02, 0.15), (0.0, 2.186, 0.40), mat=plate, name="plate"))

	# A cracked windscreen: one impact, lines running out from it, and the wipers left mid-sweep.
	hit = (-0.30, 0.06)
	for ray in ([(0.10, 0.02), (0.22, 0.07), (0.38, 0.05)], [(-0.08, 0.06), (-0.16, 0.16), (-0.22, 0.21)],
				[(0.04, -0.09), (0.12, -0.18)], [(-0.10, -0.05), (-0.26, -0.12), (-0.34, -0.20)], [(0.06, 0.12), (0.16, 0.19)]):
		pts = [on_screen(hit[0], hit[1])] + [on_screen(hit[0] + du, hit[1] + dv) for (du, dv) in ray]
		parts.append(geo.tube(pts, 0.007, mat=crack, segments=4, name="crack"))
	for (u0, u1, v1) in ((-0.64, -0.14, -0.15), (0.08, 0.50, -0.20)):
		parts.append(geo.tube([on_screen(u0, -0.25), on_screen(u1, v1)], 0.011, mat=rubber, segments=5, name="wiper"))

	# Mirrors: the left one still up, the right one hanging off its arm.
	parts.append(geo.tube([(-0.86, 1.90, 1.40), (-0.99, 1.95, 1.44), (-0.99, 1.95, 1.54)], 0.014, mat=metal, segments=5, name="mirror_arm"))
	parts.append(geo.box((0.05, 0.13, 0.22), (-1.00, 1.95, 1.62), mat=metal, bevel=0.015, bevel_segments=1, name="mirror"))
	parts.append(geo.tube([(0.86, 1.90, 1.40), (0.98, 1.95, 1.42), (1.00, 1.96, 1.34)], 0.014, mat=metal, segments=5, name="mirror_arm"))
	parts.append(geo.box((0.05, 0.13, 0.22), (1.01, 1.97, 1.22), (0, 12, 8), mat=metal, bevel=0.015, bevel_segments=1, name="mirror"))

	# Chassis, tank and mud flaps under the bed.
	for sx in (-1.0, 1.0):
		parts.append(geo.box((0.10, 3.80, 0.14), (sx * 0.42, -0.20, 0.62), mat=metal, name="rail"))
		parts.append(geo.box((0.22, 0.02, 0.26), (sx * 0.79, -1.58, 0.44), mat=rubber, name="mudflap"))
	parts.append(geo.box((0.30, 0.62, 0.26), (0.62, -0.10, 0.54), mat=metal, bevel=0.04, bevel_segments=1, name="tank"))

	# Wheels, all four gone soft; the truck has sunk onto its rims.
	for (x, y, sq) in ((-0.79, 1.38, 0.70), (0.79, 1.38, 0.74), (-0.79, -1.20, 0.72), (0.79, -1.20, 0.82)):
		wheel(parts, x, y, sq, rubber, metal)

	# The bed: floor, drop sides, headboard, a cab guard frame, and the tailgate hanging open.
	shell.append(geo.box((1.80, 2.80, 0.08), (0.0, -0.76, 0.74), mat=gate, bevel=0.02, name="bed_floor"))
	for sx in (-1.0, 1.0):
		shell.append(geo.box((0.05, 2.80, 0.38), (sx * 0.875, -0.76, 0.97), mat=gate, bevel=0.015, name="gate_side"))
		parts.append(geo.box((0.07, 2.82, 0.04), (sx * 0.875, -0.76, 1.165), mat=gate, name="gate_rail"))
		for y in (-2.13, 0.60):
			parts.append(geo.box((0.08, 0.08, 0.44), (sx * 0.87, y, 0.98), mat=gate, name="gate_post"))
	shell.append(geo.box((1.80, 0.05, 0.40), (0.0, 0.615, 0.98), mat=gate, bevel=0.015, name="headboard"))
	shell.append(geo.box((1.80, 0.05, 0.38), (0.0, -2.20, 0.574), (-12, 0, 0), mat=gate, bevel=0.015, name="tailgate"))
	for sx in (-1.0, 1.0):
		parts.append(geo.tube([(sx * 0.80, 0.60, 1.16), (sx * 0.80, 0.60, 1.94)], 0.028, mat=metal, segments=6, name="guard_post"))
	parts.append(geo.tube([(-0.80, 0.60, 1.94), (0.80, 0.60, 1.94)], 0.028, mat=metal, segments=6, name="guard_top"))
	parts.append(geo.tube([(-0.80, 0.60, 1.55), (0.80, 0.60, 1.55)], 0.022, mat=metal, segments=6, name="guard_mid"))

	# Rust bloom low on the cab and along one side.
	for sx in (-1.0, 1.0):
		parts.append(geo.box((0.02, 0.30, 0.20), (sx * 0.866, 0.84, 0.54), mat=rust, name="rust_cab"))
	parts.append(geo.box((0.02, 0.70, 0.10), (0.902, -1.55, 0.84), mat=rust, name="rust_gate"))
	parts.append(geo.box((0.02, 0.06, 0.45), (-0.868, 1.66, 1.10), mat=rust, name="rust_streak"))

	# Junk in the bed: a crate, two sacks, a spare tyre half swallowed by moss.
	box = geo.box((0.55, 0.48, 0.42), (0.38, 0.20, 0.99), (0, 0, 10), mat=crate, bevel=0.02, name="crate")
	shell.append(box)
	parts.append(geo.box((0.57, 0.50, 0.05), (0.38, 0.20, 1.10), (0, 0, 10), mat=mat.get("wood_dark"), name="crate_band"))
	parts.append(geo.blob((0.22, 0.19, 0.27), (-0.42, 0.24, 1.02), (0, -12, 20), mat=canvas, subdivisions=2, lumpy=0.15, seed=3, name="sack"))
	parts.append(geo.blob((0.07, 0.07, 0.07), (-0.47, 0.24, 1.29), mat=canvas, subdivisions=1, lumpy=0.2, seed=4, name="sack_neck"))
	parts.append(geo.blob((0.32, 0.17, 0.14), (-0.28, -0.38, 0.92), (0, 0, 70), mat=canvas, subdivisions=2, lumpy=0.18, seed=6, name="sack_lying"))
	ring = [(-0.40 + 0.24 * c, -1.62 + 0.24 * s, 0.88) for (c, s) in
			[(1, 0), (0.866, 0.5), (0.5, 0.866), (0, 1), (-0.5, 0.866), (-0.866, 0.5), (-1, 0), (-0.866, -0.5),
			 (-0.5, -0.866), (0, -1), (0.5, -0.866), (0.866, -0.5), (1, 0)]]
	parts.append(geo.tube(ring, 0.085, mat=rubber, segments=8, caps=False, name="spare_tyre"))

	# A thick blanket of moss filling the bed, spilling over the left side and the open tailgate.
	blanket = geo.blob((0.74, 1.12, 0.26), (0.0, -1.05, 0.80), mat=moss, lumpy=0.25, seed=5, name="moss_bed")
	geo.tag(blanket, "pulse")
	parts.append(blanket)
	parts.append(geo.blob((0.52, 0.42, 0.15), (0.12, -0.12, 0.82), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=8, name="moss_bed_front"))
	parts.append(geo.blob((0.12, 0.58, 0.26), (-0.90, -1.15, 1.00), (0, 15, 0), mat=moss, subdivisions=2, lumpy=0.3, seed=9, name="moss_spill_side"))
	parts.append(geo.blob((0.50, 0.08, 0.22), (0.18, -2.21, 0.66), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=10, name="moss_spill_tail"))
	roof = geo.blob((0.62, 0.48, 0.10), (0.10, 1.26, 1.90), mat=moss, lumpy=0.3, seed=11, name="moss_roof")
	geo.tag(roof, "pulse")
	parts.append(roof)
	parts.append(geo.blob((0.34, 0.07, 0.05), (-0.42, 2.03, 1.22), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=12, name="moss_ledge"))

	# A sapling and ferns coming up out of the bed, weeds at the wheels.
	rnd = geo.rng(4)
	for (cx, cy, n, hmin, hmax) in ((0.40, -1.35, 5, 0.35, 0.65), (-0.55, -0.75, 3, 0.25, 0.40), (0.55, -0.55, 3, 0.2, 0.35)):
		for k in range(n):
			h = rnd.uniform(hmin, hmax)
			w = geo.cone(rnd.uniform(0.05, 0.08), h, (cx + rnd.uniform(-0.12, 0.12), cy + rnd.uniform(-0.12, 0.12), 0.98 + h * 0.5),
						 (rnd.uniform(-25, 25), rnd.uniform(-25, 25), 0), mat=leaf, segments=5, name="sprout")
			geo.tag(w, "sway", "foliage")
			parts.append(w)
	for (cx, cy) in ((-0.86, 1.40), (0.86, -1.15), (-0.84, -1.30), (0.60, 1.96), (-0.40, -2.02), (0.86, 1.10)):
		for k in range(4):
			h = rnd.uniform(0.22, 0.45)
			w = geo.cone(rnd.uniform(0.05, 0.08), h, (cx + rnd.uniform(-0.1, 0.1), cy + rnd.uniform(-0.1, 0.1), h * 0.5),
						 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=5, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	for (loc, r) in (((-0.22, -0.95, 1.07), 0.06), ((0.34, 1.32, 2.00), 0.05)):
		g = geo.blob(r, loc, mat=glow, subdivisions=2, name="spore")
		geo.tag(g, "pulse")
		parts.append(g)

	for i, p in enumerate(shell):
		geo.moss_mask(p, seed=i + 2)
	parts += shell

	return pipeline.static("truck", parts, size=512)
