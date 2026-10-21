# Vinyl greenhouse skeleton: galvanised pipe hoops in a tunnel, torn milky plastic hanging in strips,
# the old beds gone to tall grass inside and vines up the hoops. One purlin has snapped and hangs down.
import math

import bmesh
import bpy
from mathutils import Vector, noise

from gse import core, geo, mat, pipeline

HW, HH, LEN = 1.5, 2.3, 5.0


def arch(t):
	# t 0..1 across the hoop, left foot to right foot; slightly squared round arch.
	a = math.pi * (1.0 - t)
	c, s = math.cos(a), math.sin(a)
	x = HW * math.copysign(abs(c) ** 0.85, c)
	z = HH * abs(s) ** 0.75
	return x, z


def ribbon(path, widths, mat_obj, name, seed=0, wrinkle=0.04):
	# A strip of plastic along a path, its width running along Y, wrinkled, with a little thickness.
	bm = bmesh.new()
	rows = []
	off = Vector((seed * 5.1, seed * 2.3, seed * 1.7))
	for i, (p, w) in enumerate(zip(path, widths)):
		row = []
		rag = (noise.noise(Vector((i * 0.9, seed * 1.3, 0.5))) * 0.35, noise.noise(Vector((i * 0.9, seed * 1.3, 4.5))) * 0.35)
		for k in range(3):
			f = k / 2.0 - 0.5
			if k == 0:
				f -= rag[0] * 0.5
			elif k == 2:
				f += rag[1] * 0.5
			co = Vector((p[0], p[1] + f * w, p[2]))
			n = noise.noise(co * 3.0 + off)
			co.x += n * wrinkle
			co.y += noise.noise(co * 2.0 + off + Vector((7, 0, 0))) * wrinkle * 0.5
			row.append(bm.verts.new(co))
		rows.append(row)
	for i in range(len(rows) - 1):
		for k in range(2):
			bm.faces.new((rows[i][k], rows[i][k + 1], rows[i + 1][k + 1], rows[i + 1][k]))
	mesh = bpy.data.meshes.new(name)
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, mesh))
	mesh.materials.append(mat_obj)
	for poly in mesh.polygons:
		poly.use_smooth = True
	mod = obj.modifiers.new("solid", 'SOLIDIFY')
	mod.thickness = 0.012
	mod.offset = 0.0
	return obj


def build():
	pipe = mat.make("gh_pipe", (0.50, 0.51, 0.50), pattern="rust", rough=0.55, bump=0.25, amount=0.45, accent="rust")
	vinyl = mat.make("gh_vinyl", (0.64, 0.66, 0.62), pattern="plaster", rough=0.6, var=0.18, var_scale=5.0, grime=0.5, edge=0.04, bump=0.2)
	soil = mat.get("soil")
	stake = mat.get("wood_grey")
	vine = mat.make("gh_vine", (0.21, 0.27, 0.17), pattern="bark", rough=0.9, bump=0.4)
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	moss = mat.get("moss")
	grass = mat.make("gh_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	dry = mat.make("gh_dry_grass", (0.50, 0.46, 0.32), pattern="leaf", rough=0.9, bump=0.3)

	parts = []
	man_made = []
	rnd = geo.rng(31)

	def add(o, mossy=True):
		parts.append(o)
		if mossy:
			man_made.append(o)
		return o

	R = 0.026
	n_hoops = 7
	ys = [-LEN * 0.5 + LEN * i / (n_hoops - 1) for i in range(n_hoops)]
	steps = 14
	for i, y in enumerate(ys):
		lean = 0.0
		sag = 0.0
		if i == n_hoops - 1:
			lean = 0.18
		if i == 2:
			sag = 0.16
		pts = []
		for k in range(steps + 1):
			t = k / float(steps)
			x, z = arch(t)
			top = math.sin(math.pi * t)
			pts.append((x, y + lean * (z / HH) ** 2, z - sag * top ** 4))
		pts[0] = (pts[0][0], pts[0][1], -0.05)
		pts[-1] = (pts[-1][0], pts[-1][1], -0.05)
		add(geo.tube(pts, R, mat=pipe, segments=5, name="hoop"))

	# Purlins: a ridge pipe and two side runs a side; the left upper one has snapped and hangs.
	for t in (0.5, 0.18, 0.82, 0.32, 0.68):
		x, z = arch(t)
		if abs(t - 0.32) < 1e-3:
			# Broken: runs from the back to the middle hoop, then droops down to the ground.
			x2, z2 = x + 0.04, z
			add(geo.tube([(x, LEN * 0.5, z), (x, 0.0, z - 0.05), (x2 - 0.10, -0.55, z - 0.45), (x2 - 0.25, -0.95, 0.35), (x2 - 0.35, -1.10, 0.02)],
						 R, mat=pipe, segments=5, name="purlin_broken"))
			add(geo.tube([(x, -LEN * 0.5, z), (x, -1.55, z - 0.06), (x - 0.05, -1.40, z - 0.18)], R, mat=pipe, segments=5, name="purlin_stub"))
			continue
		zz = z - (0.10 if t == 0.5 else 0.0)
		add(geo.tube([(x, -LEN * 0.5, zz), (x, -LEN * 0.25, zz - 0.02), (x, 0.0, zz - 0.06), (x, LEN * 0.25, zz - 0.02), (x, LEN * 0.5, zz)], R * 0.9, mat=pipe, segments=5, name="purlin"))
	# Door frame at the front end and ground rails along the feet.
	for sx in (-0.45, 0.45):
		add(geo.tube([(sx, -LEN * 0.5, -0.02), (sx, -LEN * 0.5, 1.85)], R, mat=pipe, segments=5, name="door_post"))
	add(geo.tube([(-0.80, -LEN * 0.5, 1.85), (0.80, -LEN * 0.5, 1.85)], R, mat=pipe, segments=5, name="door_head"))
	for sx in (-HW, HW):
		add(geo.tube([(sx, -LEN * 0.5, 0.25), (sx, LEN * 0.5, 0.25)], R * 0.8, mat=pipe, segments=4, name="foot_rail"))

	# Plastic: a torn roof sheet still on the back bays, and strips hanging from a few hoops.
	def arch_path(y, t0, t1, n, drop=0.0, sag=0.0, out=0.03):
		path = []
		for k in range(n + 1):
			t = t0 + (t1 - t0) * k / float(n)
			x, z = arch(t)
			s = 1.0 + out / HW
			path.append((x * s, y, z + out - sag * math.sin(math.pi * k / float(n))))
		if drop > 0.0:
			x, z = path[-1][0], path[-1][2]
			path.append((x * 1.01, y, z - drop * 0.5))
			path.append((x * 1.02, y + 0.03, z - drop))
		return path

	sheets = [
		(LEN * 0.5 - 0.42, 0.50, 0.96, 8, 0.0, 0.08, 0.80, 0.80),
		(LEN * 0.5 - 1.25, 0.58, 0.88, 6, 0.6, 0.10, 0.75, 0.40),
		(LEN * 0.5 - 0.42, 0.20, 0.50, 6, 0.0, 0.05, 0.80, 0.60),
	]
	for i, (y, t0, t1, n, drop, sag, w0, w1) in enumerate(sheets):
		path = arch_path(y, t0, t1, n * 2, drop, sag)
		widths = [w0 + (w1 - w0) * k / float(len(path) - 1) for k in range(len(path))]
		add(ribbon(path, widths, vinyl, "vinyl", seed=i + 1, wrinkle=0.08), False)
	# Hanging tatters: draped over a hoop top and falling down one side.
	tatters = [(ys[1], 0.46, 0.30, 1.0, 0.32), (ys[3], 0.55, 0.78, 1.4, 0.26), (ys[1] + 0.1, 0.60, 0.80, 0.7, 0.22), (ys[4], 0.40, 0.22, 1.2, 0.30),
			   (ys[0], 0.15, 0.05, 0.6, 0.25)]
	for i, (y, t0, t1, drop, w) in enumerate(tatters):
		path = arch_path(y, t0, t1, 6, drop, 0.0, out=0.035)
		widths = [w] * (len(path) - 2) + [w * 0.6, w * 0.25]
		add(ribbon(path, widths, vinyl, "tatter", seed=i + 7, wrinkle=0.09), False)

	# Inside: the old beds as low soil ridges, a row of crop stakes, all gone to tall grass.
	for bx in (-0.70, 0.70):
		b = geo.blob((0.42, LEN * 0.45, 0.14), (bx, 0.0, 0.0), mat=soil, lumpy=0.15, seed=int(bx * 10), subdivisions=2, name="bed")
		parts.append(b)
		for k in range(6):
			y = -2.0 + k * 0.8 + rnd.uniform(-0.1, 0.1)
			st = geo.box((0.03, 0.03, 1.0), (bx + rnd.uniform(-0.05, 0.05), y, 0.45), (rnd.uniform(-12, 12), rnd.uniform(-12, 12), 0), mat=stake, name="stake")
			add(st, False)
	for k in range(36):
		x = rnd.uniform(-1.25, 1.25)
		y = rnd.uniform(-2.3, 2.3)
		hgt = rnd.uniform(0.6, 1.3)
		w = geo.cone(rnd.uniform(0.05, 0.09), hgt, (x, y, hgt * 0.5), (rnd.uniform(-12, 12), rnd.uniform(-12, 12), 0), mat=(grass, leaf, dry)[k % 3], segments=4, name="tall_grass")
		geo.tag(w, "sway", "foliage")
		parts.append(w)
	for k in range(7):
		x = rnd.uniform(-1.1, 1.1)
		y = rnd.uniform(-2.1, 2.1)
		r = rnd.uniform(0.25, 0.40)
		g = geo.blob((r, r * 0.9, r * 0.5), (x, y, r * 0.25), mat=(grass, leaf_dark, leaf)[k % 3], lumpy=0.5, seed=k + 40, subdivisions=2, name="grass_clump")
		geo.tag(g, "sway", "foliage")
		parts.append(g)
	# Weeds round the feet outside.
	for k in range(22):
		side = -1.0 if k % 2 else 1.0
		y = rnd.uniform(-2.5, 2.5)
		hgt = rnd.uniform(0.25, 0.6)
		w = geo.cone(rnd.uniform(0.04, 0.07), hgt, (side * rnd.uniform(1.45, 1.75), y, hgt * 0.5), (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0),
					 mat=leaf if k % 3 else leaf_dark, segments=4, name="weed")
		geo.tag(w, "sway", "foliage")
		parts.append(w)

	# Vines up two hoops on the right side, leaves along them.
	for (y, t1, seed) in ((ys[2], 0.62, 1), (ys[5], 0.70, 2), (ys[4], 0.20, 3)):
		path = []
		t_start = 1.0 if t1 > 0.5 else 0.0
		for k in range(7):
			t = t_start + (t1 - t_start) * k / 6.0
			x, z = arch(t)
			s = 1.0 + 0.03 / HW
			path.append((x * s, y + 0.03 * math.sin(k * 1.7), z + 0.03))
		rads = [0.02 - 0.0022 * k for k in range(7)]
		parts.append(geo.tube(path, rads, mat=vine, segments=4, name="vine"))
		for k in range(1, len(path)):
			for j in range(2):
				p0, p1 = path[k - 1], path[k]
				t = (j + 0.5) / 2.0
				c = (p0[0] + (p1[0] - p0[0]) * t, p0[1] + (p1[1] - p0[1]) * t + rnd.uniform(-0.06, 0.06), p0[2] + (p1[2] - p0[2]) * t)
				lf = geo.blob((0.08, 0.06, 0.012), c, (rnd.uniform(-50, 50), rnd.uniform(-50, 50), rnd.uniform(0, 180)), mat=leaf if (k + j) % 2 else leaf_dark,
							  subdivisions=1, name="vine_leaf")
				geo.tag(lf, "sway", "foliage")
				parts.append(lf)
	m = geo.blob((0.35, 0.30, 0.10), (-1.45, 1.6, 0.05), mat=moss, lumpy=0.3, seed=5, subdivisions=2, name="moss")
	geo.tag(m, "pulse")
	parts.append(m)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, up=0.6, low=0.8, seed=i + 1)

	return pipeline.static("greenhouse", parts, size=512)
