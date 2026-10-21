# A wooden utility pole, leaning a little: cross-arm with ceramic insulators, cut wires hanging,
# a rusting transformer can, climbing pegs, an old flyer, ivy wound round the lower part, moss at the foot.
import math

from mathutils import Matrix

from gse import geo, mat, pipeline


def build():
	wood = mat.make("pole_wood", (0.34, 0.29, 0.24), pattern="wood", rough=0.9, bump=0.45)
	arm_wood = mat.make("pole_arm", (0.33, 0.29, 0.24), pattern="wood", rough=0.9, bump=0.4, axis='X')
	ceramic = mat.make("pole_ceramic", (0.74, 0.74, 0.70), rough=0.35, bump=0.05, grime=0.4)
	can = mat.make("pole_can", (0.38, 0.42, 0.40), pattern="rust", rough=0.7, bump=0.4, amount=1.0)
	flyer = mat.make("pole_flyer", (0.66, 0.62, 0.52), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	ink = mat.make("pole_ink", (0.42, 0.24, 0.22), rough=0.9, bump=0.0, edge=0.0)
	metal = mat.get("metal")
	wire = mat.make("pole_wire", (0.12, 0.12, 0.12), rough=0.6, bump=0.0, edge=0.05)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	glow = mat.get("spore_glow")
	rnd = geo.rng(5)

	up = []
	ground = []

	# The pole and its cross-arm, with two flat braces.
	pole = geo.cylinder(0.13, 5.6, (0, 0, 2.8), mat=wood, segments=10, radius_top=0.105, name="pole")
	up.append(pole)
	up.append(geo.cylinder(0.11, 0.04, (0, 0, 5.62), mat=metal, segments=10, name="cap"))
	arm = geo.box((1.80, 0.10, 0.12), (0, -0.16, 5.20), mat=arm_wood, bevel=0.015, bevel_segments=1, name="crossarm")
	up.append(arm)
	for sx in (-1.0, 1.0):
		up.append(geo.tube([(sx * 0.55, -0.16, 5.14), (0.0, -0.12, 4.82)], 0.016, mat=metal, segments=4, name="brace"))
	up.append(geo.box((0.06, 0.14, 0.20), (0, -0.07, 5.20), mat=metal, name="arm_bolt"))

	# Insulators: three bells and one broken pin.
	bell = [(0.026, 0.0), (0.055, 0.02), (0.062, 0.05), (0.040, 0.07), (0.058, 0.095), (0.034, 0.135), (0.0, 0.15)]
	tops = []
	for x in (-0.72, -0.24, 0.72):
		up.append(geo.lathe(bell, (x, -0.16, 5.26), mat=ceramic, segments=10, name="insulator"))
		tops.append((x, -0.16, 5.38))
	up.append(geo.cylinder(0.012, 0.08, (0.24, -0.16, 5.30), mat=metal, segments=6, name="pin"))
	up.append(geo.lathe(bell[:4], (0.30, -0.42, 0.10), (0, 70, 20), mat=ceramic, segments=10, name="insulator_fallen"))

	# Cut wires: tied at the insulators, run off a little, then hang slack.
	for i, (x, y, z) in enumerate(tops):
		for side in ((1.0,) if i != 1 else (1.0, -1.0)):
			drop = rnd.uniform(0.7, 1.6)
			pts = [(x, y, z), (x, y + side * 0.25, z - 0.03), (x + 0.03, y + side * 0.42, z - drop * 0.4),
				   (x + 0.05, y + side * 0.46, z - drop * 0.8), (x + 0.04, y + side * 0.44, z - drop)]
			up.append(geo.tube(pts, 0.012, mat=wire, segments=4, name="wire"))

	# Transformer can on a bracket on the front, a lead running up to the arm.
	up.append(geo.cylinder(0.21, 0.55, (0, -0.36, 4.25), mat=can, segments=14, name="transformer"))
	up.append(geo.cylinder(0.225, 0.05, (0, -0.36, 4.55), mat=can, segments=14, name="transformer_lid"))
	up.append(geo.cylinder(0.215, 0.04, (0, -0.36, 4.05), mat=metal, segments=14, name="transformer_band"))
	for z in (4.05, 4.42):
		up.append(geo.box((0.10, 0.22, 0.06), (0, -0.19, z), mat=metal, name="bracket"))
		up.append(geo.cylinder(0.135, 0.05, (0, 0, z), mat=metal, segments=10, name="band"))
	for x in (-0.08, 0.08):
		up.append(geo.lathe([(0.03, 0.0), (0.035, 0.04), (0.02, 0.06), (0.03, 0.08), (0.0, 0.10)], (x, -0.40, 4.575), mat=ceramic, segments=8, name="bushing"))
	up.append(geo.tube([(0.08, -0.40, 4.66), (0.20, -0.36, 4.85), (0.40, -0.25, 5.05), (0.72, -0.16, 5.33)], 0.010, mat=wire, segments=4, name="lead"))
	up.append(geo.tube([(-0.08, -0.40, 4.66), (-0.22, -0.48, 4.50), (-0.30, -0.52, 4.20)], 0.010, mat=wire, segments=4, name="lead_cut"))

	# Climbing pegs above the ivy, an old flyer taped on, and a number plate.
	for k in range(6):
		a = math.radians(90 + (60 if k % 2 else -60))
		z = 2.5 + k * 0.32
		up.append(geo.cylinder(0.014, 0.24, (math.cos(a) * 0.17, math.sin(a) * 0.17, z), (0, 90, math.degrees(a)), mat=metal, segments=6, name="peg"))
	up.append(geo.box((0.20, 0.012, 0.28), (0.02, -0.128, 2.05), (0, 5, 0), mat=flyer, name="flyer"))
	up.append(geo.box((0.15, 0.008, 0.04), (0.02, -0.136, 2.12), (0, 5, 0), mat=ink, name="flyer_title"))
	up.append(geo.box((0.11, 0.008, 0.10), (0.02, -0.136, 1.98), (0, 5, 0), mat=ink, name="flyer_photo"))
	up.append(geo.box((0.12, 0.015, 0.16), (0.0, -0.118, 3.0), mat=mat.make("pole_plate", (0.56, 0.56, 0.52), pattern="rust", amount=0.6), name="plate"))

	# Ivy winding round the lower 2.4 m.
	for (phase, top, turns) in ((0.0, 2.40, 1.6), (2.4, 1.90, 1.2), (4.3, 1.30, 0.9)):
		pts = []
		n = 12
		for k in range(n + 1):
			t = k / float(n)
			a = phase + t * turns * math.tau
			z = 0.05 + (top - 0.05) * t
			r = 0.145 - 0.012 * t
			pts.append((math.cos(a) * r, math.sin(a) * r, z))
		vine = geo.tube(pts, [0.022] * n + [0.01], mat=leaf_dark, segments=4, name="ivy")
		geo.tag(vine, "sway", "foliage")
		up.append(vine)
		for k in range(1, n + 1):
			if rnd.random() < 0.95:
				x, y, z = pts[k]
				d = math.atan2(y, x)
				lr = rnd.uniform(0.09, 0.14)
				lf = geo.blob((lr, lr * 0.55, lr * 0.85), (x + math.cos(d) * 0.06, y + math.sin(d) * 0.06, z + rnd.uniform(-0.05, 0.05)),
							  (0, 0, math.degrees(d) + 90), mat=leaf if rnd.random() < 0.55 else leaf_dark, subdivisions=1,
							  lumpy=0.3, seed=k, name="ivy_leaf")
				geo.tag(lf, "sway", "foliage")
				up.append(lf)
	for (a, z, s) in ((0.5, 0.55, 0.22), (2.6, 0.85, 0.20), (4.4, 0.35, 0.24), (1.6, 1.45, 0.17), (5.5, 1.20, 0.16), (3.5, 1.85, 0.14), (0.9, 2.20, 0.12)):
		lf = geo.blob((s, s * 0.8, s * 0.9), (math.cos(a) * 0.20, math.sin(a) * 0.20, z), mat=leaf, subdivisions=2, lumpy=0.35, seed=int(a * 10), name="ivy_clump")
		geo.tag(lf, "sway", "foliage")
		up.append(lf)

	# Moss up the shady side and round the foot, grass tufts.
	streak = geo.blob((0.07, 0.10, 0.55), (-0.10, 0.08, 0.60), (0, -3, 30), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=3, name="moss_streak")
	up.append(streak)
	foot = geo.blob((0.48, 0.42, 0.13), (0.03, 0.02, 0.03), mat=moss, lumpy=0.3, seed=1, name="moss_foot")
	geo.tag(foot, "pulse")
	ground.append(foot)
	ground.append(geo.blob((0.30, 0.22, 0.08), (-0.38, 0.30, 0.02), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=2, name="moss_foot2"))
	for (cx, cy) in ((0.35, -0.25), (-0.30, -0.30), (0.20, 0.38), (-0.45, 0.05)):
		for k in range(4):
			h = rnd.uniform(0.18, 0.38)
			w = geo.cone(rnd.uniform(0.04, 0.07), h, (cx + rnd.uniform(-0.1, 0.1), cy + rnd.uniform(-0.1, 0.1), h * 0.5),
						 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=5, name="grass")
			geo.tag(w, "sway", "foliage")
			ground.append(w)
	g = geo.blob(0.045, (0.25, -0.15, 0.15), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	ground.append(g)

	# The whole pole leans a little; things on the ground stay put.
	lean = Matrix.Rotation(math.radians(2.5), 4, 'Y') @ Matrix.Rotation(math.radians(-1.0), 4, 'X')
	for p in up:
		p.matrix_basis = lean @ p.matrix_basis
	for i, p in enumerate((pole, arm)):
		geo.moss_mask(p, seed=i + 4)

	return pipeline.static("pole", up + ground, size=512)
