# A blue steel double gate between two short tiled-cap wall stubs: paint flaking to rust, "개조심" still
# on the closed leaf, the other leaf left ajar into the yard. Moss on the caps, ivy up one post.
import math

from gse import geo, mat, pipeline


def leaf_parts(L, H, paint, sheet, flip):
	# One gate leaf in its own frame: hinge at the origin, leaf running along +X.
	ps = []
	t = 0.05
	ps.append(geo.box((t, t, H), (t * 0.5, 0, H * 0.5 + 0.06), mat=paint, bevel=0.008, bevel_segments=1, name="stile"))
	ps.append(geo.box((t, t, H), (L - t * 0.5, 0, H * 0.5 + 0.06), mat=paint, bevel=0.008, bevel_segments=1, name="stile"))
	for z in (0.08, 1.02, H + 0.04):
		ps.append(geo.box((L, t, t), (L * 0.5, 0, z), mat=paint, bevel=0.008, bevel_segments=1, name="rail"))
	# Lower sheet with two raised panels pressed into it.
	ps.append(geo.box((L - 0.04, 0.012, 0.92), (L * 0.5, 0, 0.55), mat=sheet, name="sheet"))
	for side in (-1.0, 1.0):
		for (cx, w) in ((L * 0.27, L * 0.36), (L * 0.73, L * 0.36)):
			ps.append(geo.box((w, 0.02, 0.62), (cx, side * 0.012, 0.55), mat=sheet, name="panel"))
	# Upper grille: bars and a half sun of rays, the old pattern every gate had.
	n = 10
	for k in range(n):
		x = 0.09 + (L - 0.18) * k / (n - 1)
		ps.append(geo.cylinder(0.011, H - 0.98, (x, 0, 1.02 + (H - 0.98) * 0.5), mat=paint, segments=5, name="bar"))
		ps.append(geo.cone(0.022, 0.08, (x, 0, H + 0.11), mat=paint, segments=4, name="finial"))
	cx = L * 0.5
	arc = [(cx + 0.30 * math.cos(a / 10.0 * math.pi), 0, 1.05 + 0.30 * math.sin(a / 10.0 * math.pi)) for a in range(11)]
	ps.append(geo.tube(arc, 0.016, mat=paint, segments=5, name="sun"))
	for k in range(1, 6):
		a = k / 6.0 * math.pi
		ps.append(geo.tube([(cx + 0.10 * math.cos(a), 0, 1.05 + 0.10 * math.sin(a)), (cx + 0.29 * math.cos(a), 0, 1.05 + 0.29 * math.sin(a))], 0.010, mat=paint, segments=4, name="ray"))
	ps.append(geo.cylinder(0.10, 0.03, (cx, 0, 1.05), (90, 0, 0), mat=paint, segments=10, name="sun_core"))
	return ps


def place(objs, loc, rot_z):
	for o in objs:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.location = loc
		o.rotation_euler = (0.0, 0.0, math.radians(rot_z))
		geo.apply_transform(o)


def build():
	paint = mat.make("gate_paint", (0.26, 0.38, 0.46), pattern="rust", rough=0.72, bump=0.4, amount=0.9)
	sheet = mat.make("gate_sheet", (0.27, 0.39, 0.47), pattern="rust", rough=0.72, bump=0.35, amount=0.75)
	red = mat.make("gate_letter", (0.56, 0.20, 0.16), pattern="rust", rough=0.7, bump=0.2, amount=0.5)
	wall = mat.get("plaster_grey")
	block = mat.get("concrete")
	tile = mat.get("giwa")
	metal = mat.get("metal")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	vine = mat.make("gate_vine", (0.22, 0.25, 0.17), pattern="bark", rough=0.9, bump=0.4)
	plate = mat.get("wood_dark")

	parts = []
	man_made = []
	rnd = geo.rng(23)

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			man_made.append(o)
		return o

	# Wall stubs: block wall skimmed with plaster, a footing, and a little giwa roof on top.
	SX = 1.55
	for s in (-1.0, 1.0):
		x = s * SX
		w = add(geo.box((0.50, 0.30, 1.72), (x, 0, 0.86), mat=wall, bevel=0.02, bevel_segments=1, name="wall", subdiv=1))
		geo.jitter(w, 0.012, 2.0, seed=int(3 + s))
		add(geo.box((0.56, 0.36, 0.14), (x, 0, 0.07), mat=block, bevel=0.015, bevel_segments=1, name="footing"))
		add(geo.box((0.58, 0.38, 0.05), (x, 0, 1.745), mat=block, bevel=0.01, bevel_segments=1, name="cap_slab"))
		cap = add(geo.prism((0.64, 0.46, 0.16), (x, 0, 1.77), mat=tile, name="cap_roof"))
		add(geo.cylinder(0.045, 0.66, (x, 0, 1.93), (0, 90, 0), mat=tile, segments=8, name="cap_ridge"))
		# Plaster flaked off near the bottom shows the blocks.
		add(geo.box((0.30, 0.02, 0.22), (x + s * 0.04, -0.152, 0.42), mat=block, bevel=0.005, bevel_segments=1, name="blocks"), False)
		# Steel post bolted to the stub's inner face.
		add(geo.box((0.08, 0.08, 1.86), (x - s * 0.29, 0, 0.93), mat=paint, bevel=0.01, bevel_segments=1, name="post"))
		add(geo.sphere(0.055, (x - s * 0.29, 0, 1.90), mat=paint, segments=6, rings=4, name="post_knob"))

	# Nameplate on the left stub, the name long since weathered off.
	add(geo.box((0.12, 0.02, 0.30), (-SX + 0.02, -0.16, 1.32), mat=plate, bevel=0.005, bevel_segments=1, name="nameplate"))

	# Leaves: the right one shut, the left one standing open about thirty degrees into the yard.
	inner = SX - 0.25 - 0.08
	L = inner - 0.02
	H = 1.68
	right = leaf_parts(L, H, paint, sheet, 1.0)
	place(right, (inner, 0, 0), 180.0)
	left = leaf_parts(L, H, paint, sheet, -1.0)
	place(left, (-inner, 0, 0), 30.0)
	for o in right + left:
		add(o, o.name.startswith(("rail", "sheet", "panel")))
	# Hinges.
	for s in (-1.0, 1.0):
		for z in (0.30, 1.50):
			add(geo.cylinder(0.022, 0.12, (s * (inner + 0.01), 0, z), mat=metal, segments=6, name="hinge"), False)
	# Latch bar and handle on the shut leaf, and "beware of dog" painted on its sheet.
	add(geo.box((0.22, 0.03, 0.035), (0.10, -0.04, 1.02), mat=metal, name="latch"), False)
	add(geo.tube([(0.10, -0.03, 0.88), (0.10, -0.08, 0.90), (0.10, -0.08, 0.98), (0.10, -0.03, 1.00)], 0.012, mat=metal, segments=4, name="handle"), False)
	sign = geo.text("개조심", 0.20, (L * 0.5 + 0.02, -0.034, 0.56), (90, 0, 0), mat=red, depth=0.004, name="sign")
	sign = geo.decimate(sign, 420)
	parts.append(sign)

	# Rust run down the shut leaf from the rail and the latch.
	rust = mat.get("rust")
	for (x, z, h) in ((0.30, 0.86, 0.26), (0.58, 0.80, 0.38), (0.95, 0.88, 0.22), (0.12, 0.70, 0.30)):
		parts.append(geo.box((0.025, 0.004, h), (x, -0.036, z - h * 0.5 + 0.12), mat=rust, name="streak"))

	# Moss on the caps and footings.
	for s in (-1.0, 1.0):
		x = s * SX
		for (dx, dy, dz, r, m, seed) in ((0.10 * s, -0.10, 1.86, (0.22, 0.12, 0.06), moss, 1), (-0.12 * s, 0.10, 1.86, (0.18, 0.12, 0.05), moss_dark, 2),
										 (0.0, -0.17, 0.12, (0.26, 0.05, 0.10), moss, 3), (0.20 * s, -0.16, 1.70, (0.10, 0.05, 0.08), moss_dark, 4)):
			b = geo.blob(r, (x + dx, dy, dz), (0, 0, rnd.uniform(-10, 10)), mat=m, lumpy=0.3, seed=seed + int(s * 5), subdivisions=2, name="moss")
			geo.tag(b, "pulse")
			parts.append(b)

	# Ivy climbing the right stub's face and over onto the post.
	ivy = [(1.62, -0.17, 0.0), (1.58, -0.17, 0.40), (1.66, -0.17, 0.80), (1.56, -0.17, 1.15), (1.44, -0.16, 1.45), (1.30, -0.06, 1.60), (1.28, -0.05, 1.86)]
	ivy2 = [(1.62, -0.17, 0.80), (1.74, -0.17, 1.05), (1.76, -0.17, 1.40), (1.68, -0.20, 1.72)]
	for path, rads in ((ivy, [0.025, 0.022, 0.02, 0.018, 0.015, 0.012, 0.01]), (ivy2, [0.016, 0.014, 0.012, 0.009])):
		parts.append(geo.tube(path, rads, mat=vine, segments=4, name="ivy"))
		for k in range(1, len(path)):
			for j in range(3):
				p0, p1 = path[k - 1], path[k]
				t = (j + 0.5) / 3.0
				c = (p0[0] + (p1[0] - p0[0]) * t + rnd.uniform(-0.07, 0.07), p0[1] + (p1[1] - p0[1]) * t - 0.02, p0[2] + (p1[2] - p0[2]) * t + rnd.uniform(-0.04, 0.04))
				lf = geo.blob((0.075, 0.012, 0.065), c, (rnd.uniform(-20, 20), 0, rnd.uniform(-35, 35)), mat=leaf if (k + j) % 2 else leaf_dark, subdivisions=1, name="ivy_leaf")
				geo.tag(lf, "sway", "foliage")
				parts.append(lf)

	# Weeds along the foot of the walls and in the gap under the gate.
	for (cx, cy, n) in ((-1.55, -0.30, 4), (1.55, -0.30, 4), (-0.60, -0.10, 3), (0.50, -0.12, 3), (0.0, 0.25, 3), (-1.68, 0.24, 3), (1.68, 0.24, 3)):
		for k in range(n):
			hgt = rnd.uniform(0.18, 0.45)
			w = geo.cone(rnd.uniform(0.035, 0.06), hgt, (cx + rnd.uniform(-0.18, 0.18), cy + rnd.uniform(-0.06, 0.06), hgt * 0.5),
						 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf if k % 2 else leaf_dark, segments=4, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 7)

	return pipeline.static("gate", parts, size=512)
