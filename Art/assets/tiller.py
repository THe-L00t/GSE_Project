# Gyeongungi: a rusted two-wheel power tiller still hitched to its little cargo trailer, one tyre flat,
# vines over the long handlebars and weeds up through the wheels.
import math

from gse import geo, mat, pipeline


def wheel(parts, x, y, z, r, w, side, rubber, rim_mat, lugs=12, segs=16):
	made = []
	made.append(geo.cylinder(r * 0.86, w, (x, y, z), (0, 90, 0), mat=rubber, segments=segs, name="tyre"))
	made.append(geo.cylinder(r * 0.50, w + 0.02, (x, y, z), (0, 90, 0), mat=rim_mat, segments=12, name="rim"))
	made.append(geo.cylinder(r * 0.16, w + 0.10, (x + side * 0.03, y, z), (0, 90, 0), mat=rim_mat, segments=8, name="hub"))
	# Chevron lugs: two bars per lug, meeting at the centre line.
	for k in range(lugs):
		a = k / float(lugs) * math.tau
		cy = y + math.cos(a) * r * 0.90
		cz = z + math.sin(a) * r * 0.90
		for s in (-1.0, 1.0):
			lug = geo.box((w * 0.52, 0.05, 0.075), (x + s * w * 0.25, cy, cz), mat=rubber, name="lug")
			lug.rotation_mode = 'ZXY'
			lug.rotation_euler = (a - math.pi * 0.5, 0.0, math.radians(s * 24.0))
			made.append(lug)
	parts += made
	return made


def squash(objs, z_axle, factor=0.74, bulge=1.22, cx=0.0):
	# A flat tyre: everything under the axle sinks and spreads.
	for o in objs:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		for v in o.data.vertices:
			if v.co.z < z_axle:
				d = z_axle - v.co.z
				v.co.z = z_axle - d * factor
				v.co.x = cx + (v.co.x - cx) * (1.0 + (bulge - 1.0) * min(1.0, d / 0.3) ** 2)
				v.co.z = max(v.co.z, 0.0)
		o.data.update()


def build():
	paint = mat.make("tiller_paint", (0.46, 0.22, 0.17), pattern="rust", rough=0.75, bump=0.4, amount=0.95)
	rust = mat.get("rust")
	metal = mat.get("metal")
	rubber = mat.get("rubber")
	plate = mat.make("tiller_plate", (0.34, 0.33, 0.30), pattern="rust", rough=0.8, bump=0.45, amount=1.1)
	boards = mat.get("wood_grey")
	cream = mat.get("paint_cream")
	glass = mat.get("glass")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	vine = mat.make("tiller_vine", (0.20, 0.27, 0.17), pattern="bark", rough=0.9, bump=0.4)

	parts = []
	man_made = []
	rnd = geo.rng(17)

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			man_made.append(o)
		return o

	# --- the tiller ---
	AY, AZ, R = -0.72, 0.36, 0.37
	# Gearcase from the axle up to the engine bed.
	add(geo.box((0.34, 0.36, 0.42), (0, AY, AZ + 0.06), (-8, 0, 0), mat=paint, bevel=0.04, name="gearcase"))
	add(geo.cylinder(0.08, 0.66, (0, AY, AZ), (0, 90, 0), mat=metal, segments=10, name="axle"))
	# Engine block, cylinder head with cooling fins, fuel tank and radiator hopper on top.
	add(geo.box((0.44, 0.50, 0.30), (0.0, -0.98, 0.70), mat=paint, bevel=0.035, name="engine"))
	for k in range(5):
		add(geo.box((0.40, 0.022, 0.20), (0.0, -1.18 - k * 0.035, 0.70), mat=metal, name="fin"), False)
	add(geo.box((0.36, 0.30, 0.16), (0.0, -0.92, 0.94), mat=paint, bevel=0.05, bevel_segments=2, name="tank"))
	add(geo.cylinder(0.035, 0.05, (0.08, -0.86, 1.03), mat=metal, segments=8, name="cap"), False)
	add(geo.cylinder(0.13, 0.20, (0.0, -1.22, 0.92), mat=paint, segments=12, bevel=0.02, name="hopper"))
	add(geo.cylinder(0.05, 0.05, (0.0, -1.22, 1.04), mat=metal, segments=8, name="hopper_cap"), False)
	# Front lamp and bumper bar.
	add(geo.cylinder(0.075, 0.07, (0.0, -1.38, 0.78), (90, 0, 0), mat=cream, segments=12, name="lamp"))
	add(geo.cylinder(0.055, 0.02, (0.0, -1.42, 0.78), (90, 0, 0), mat=glass, segments=12, name="lamp_glass"), False)
	add(geo.tube([(-0.26, -1.30, 0.50), (-0.26, -1.42, 0.55), (0.26, -1.42, 0.55), (0.26, -1.30, 0.50)], 0.022, mat=metal, segments=6, name="bumper"))
	# Muffler standing up on the left.
	add(geo.cylinder(0.05, 0.30, (-0.20, -1.06, 1.00), mat=rust, segments=8, name="muffler"))
	add(geo.cylinder(0.065, 0.04, (-0.20, -1.06, 1.16), mat=metal, segments=8, name="muffler_cap"), False)
	# Flywheel on the right side, where the starting handle goes.
	FX = 0.27
	fw = add(geo.cylinder(0.24, 0.05, (FX, -0.98, 0.68), (0, 90, 0), mat=metal, segments=16, name="flywheel"))
	add(geo.cylinder(0.17, 0.06, (FX, -0.98, 0.68), (0, 90, 0), mat=paint, segments=14, name="flywheel_web"), False)
	for k in range(5):
		a = k / 5.0 * math.tau + 0.3
		add(geo.cylinder(0.04, 0.07, (FX + 0.005, -0.98 + math.cos(a) * 0.11, 0.68 + math.sin(a) * 0.11), (0, 90, 0), mat=metal, segments=6, name="flywheel_hole"), False)
	add(geo.cylinder(0.05, 0.10, (FX + 0.04, -0.98, 0.68), (0, 90, 0), mat=metal, segments=8, name="flywheel_hub"), False)
	# Belt cover between engine and gearcase.
	add(geo.box((0.08, 0.50, 0.34), (-0.26, -0.84, 0.54), (-25, 0, 0), mat=paint, bevel=0.03, name="belt_cover"))

	# Wheels: the left one is flat.
	wheel(parts, 0.40, AY, AZ, R, 0.20, 1.0, rubber, metal)
	flat = wheel(parts, -0.40, AY, AZ - 0.09, R, 0.20, -1.0, rubber, metal)
	squash(flat, AZ - 0.09, cx=-0.40)

	# Long double handlebar running back over the trailer's front.
	for s in (-1.0, 1.0):
		add(geo.tube([(s * 0.12, -0.62, 0.62), (s * 0.20, -0.20, 0.80), (s * 0.30, 0.18, 0.98), (s * 0.36, 0.40, 1.05)], 0.026, mat=paint, segments=6, name="handle"))
		add(geo.cylinder(0.034, 0.16, (s * 0.37, 0.48, 1.07), (-72, 0, 0), mat=rubber, segments=8, name="grip"), False)
		add(geo.tube([(s * 0.32, 0.30, 1.02), (s * 0.26, 0.40, 0.98)], 0.010, mat=metal, segments=4, name="lever"), False)
	add(geo.tube([(-0.25, 0.06, 0.92), (0.25, 0.06, 0.92)], 0.02, mat=paint, segments=6, name="handle_brace"))
	add(geo.tube([(0.0, -0.55, 0.42), (0.0, -0.20, 0.36), (0.0, 0.02, 0.36)], 0.035, mat=metal, segments=6, name="drawbar"))

	# --- the trailer ---
	TY0, TY1, DZ = 0.02, 1.10, 0.42
	TL = TY1 - TY0
	TC = (TY0 + TY1) * 0.5
	add(geo.box((0.98, TL, 0.05), (0, TC, DZ), mat=boards, bevel=0.01, name="deck"))
	add(geo.box((0.08, TL, 0.06), (0, TC, DZ - 0.06), mat=plate, name="chassis"))
	for s in (-1.0, 1.0):
		side = add(geo.box((0.035, TL, 0.30), (s * 0.475, TC, DZ + 0.17), (0, 0, 0), mat=plate, bevel=0.01, name="side"))
		for yy in (TY0 + 0.25, TC, TY1 - 0.25):
			add(geo.box((0.05, 0.04, 0.32), (s * 0.495, yy, DZ + 0.17), mat=rust, name="stake"), False)
	add(geo.box((0.98, 0.035, 0.30), (0, TY0, DZ + 0.17), mat=plate, bevel=0.01, name="front_board"))
	# The tailboard hangs open, dropped on its hinges.
	add(geo.box((0.94, 0.035, 0.28), (0, TY1 + 0.04, DZ - 0.11), (18, 0, 0), mat=plate, bevel=0.01, name="tailboard"))
	add(geo.box((0.50, 0.30, 0.08), (0, TY0 + 0.20, DZ + 0.24), mat=mat.get("canvas"), bevel=0.03, name="seat"), False)
	for s in (-1.0, 1.0):
		tw = wheel(parts, s * 0.45, 0.78, 0.25, 0.25, 0.13, s, rubber, metal, lugs=0, segs=14)
		add(geo.box((0.15, 0.42, 0.04), (s * 0.45, 0.78, 0.53), mat=plate, bevel=0.01, name="fender"))
	# A sack slumped in the corner, gone to moss.
	add(geo.blob((0.20, 0.16, 0.12), (-0.24, 0.88, DZ + 0.12), (0, 0, 20), mat=mat.get("canvas"), lumpy=0.2, seed=3, subdivisions=2, name="sack"))

	# --- reclaimed: moss, vines, weeds ---
	for (loc, rad, m, seed) in (((0.02, -0.94, 1.03), (0.16, 0.14, 0.04), moss, 1), ((0.0, 0.56, DZ + 0.04), (0.38, 0.34, 0.06), moss, 2),
								((-0.24, 0.91, DZ + 0.22), (0.18, 0.14, 0.06), moss_dark, 3), ((0.32, 1.00, DZ + 0.04), (0.12, 0.10, 0.05), moss_dark, 4),
								((-0.05, -1.22, 1.03), (0.10, 0.10, 0.03), moss_dark, 5), ((0.0, AY, AZ + 0.29), (0.15, 0.17, 0.04), moss, 6)):
		b = geo.blob(rad, loc, mat=m, lumpy=0.3, seed=seed, subdivisions=2, name="moss")
		geo.tag(b, "pulse")
		parts.append(b)

	# A vine up from the ground, round the right handlebar and down over the engine.
	vpath = [(0.55, -0.20, 0.0), (0.48, -0.30, 0.30), (0.40, -0.10, 0.55), (0.30, 0.10, 0.85), (0.32, 0.28, 1.06), (0.24, 0.22, 1.00),
			 (0.18, -0.10, 0.85), (0.20, -0.45, 0.80), (0.18, -0.75, 0.88), (0.10, -1.00, 1.02), (-0.05, -1.20, 0.90)]
	parts.append(geo.tube(vpath, [0.022, 0.02, 0.018, 0.016, 0.015, 0.014, 0.013, 0.012, 0.011, 0.010, 0.008], mat=vine, segments=4, name="vine"))
	vpath2 = [(-0.54, 0.30, 0.0), (-0.50, 0.30, 0.35), (-0.48, 0.45, 0.62), (-0.40, 0.60, 0.74), (-0.20, 0.70, 0.62)]
	parts.append(geo.tube(vpath2, [0.02, 0.018, 0.015, 0.012, 0.008], mat=vine, segments=4, name="vine"))
	for path in (vpath, vpath2):
		for k in range(1, len(path)):
			for j in range(2):
				p0, p1 = path[k - 1], path[k]
				t = (j + 0.5) / 2.0
				c = (p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t, p0[2] + (p1[2] - p0[2]) * t)
				lf = geo.blob((0.06, 0.04, 0.008), (c[0] + rnd.uniform(-0.04, 0.04), c[1] + rnd.uniform(-0.04, 0.04), c[2] + 0.02),
							  (rnd.uniform(-40, 40), rnd.uniform(-40, 40), rnd.uniform(0, 180)), mat=leaf if (k + j) % 2 else leaf_dark, subdivisions=1, name="vine_leaf")
				geo.tag(lf, "sway", "foliage")
				parts.append(lf)

	# Weeds up through and around the wheels.
	for (cx, cy, n, hmax) in ((0.40, AY, 5, 0.75), (-0.40, AY, 6, 0.70), (0.45, 0.78, 3, 0.5), (-0.45, 0.78, 4, 0.55), (0.0, -1.30, 4, 0.45),
							  (0.0, 0.40, 4, 0.55), (-0.30, 1.20, 3, 0.4), (0.42, -1.15, 3, 0.4)):
		for k in range(n):
			hgt = rnd.uniform(0.25, hmax)
			w = geo.cone(rnd.uniform(0.035, 0.06), hgt, (cx + rnd.uniform(-0.10, 0.10), cy + rnd.uniform(-0.16, 0.16), hgt * 0.5),
						 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf if k % 2 else leaf_dark, segments=4, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 4)

	res = pipeline.static("tiller", parts, size=512)
	return res
