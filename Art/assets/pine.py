# A Korean red pine: a trunk in a gentle S, grey plated bark below turning red-brown above,
# needles gathered in flat layered cloud pads on long arms, a flat umbrella crown on top.
import math

from mathutils import Vector

from gse import geo, mat, pipeline


def spline(points, radii, steps=4):
	# Catmull-Rom through the points so the trunk bends smoothly.
	pts = [Vector(p) for p in points]
	ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
	out, rad = [], []
	for i in range(len(pts) - 1):
		p0, p1, p2, p3 = ext[i], ext[i + 1], ext[i + 2], ext[i + 3]
		for k in range(steps):
			t = k / float(steps)
			t2, t3 = t * t, t * t * t
			out.append(0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (3 * p1 - p0 - 3 * p2 + p3) * t3))
			rad.append(radii[i] + (radii[i + 1] - radii[i]) * t)
	out.append(pts[-1])
	rad.append(radii[-1])
	return out, rad


def pad(center, radii, angle, m, top, seed, lumps=3):
	# A cloud pad: flat underneath, domed and lumpy on top, stretched along its arm.
	rnd = geo.rng(seed)
	rx, ry, rz = radii
	base = geo.blob(radii, center, (0.0, 0.0, math.degrees(angle)), mat=m, lumpy=0.22, seed=seed, name="pad")
	for v in base.data.vertices:
		if v.co.z < 0.0:
			v.co.z *= 0.4
	out = [base]
	c = Vector(center)
	ca, sa = math.cos(angle), math.sin(angle)
	for k in range(lumps):
		b = k / float(lumps) * math.tau + rnd.uniform(-0.5, 0.5)
		d = rnd.uniform(0.25, 0.5)
		s = rnd.uniform(0.42, 0.55)
		lx, ly = math.cos(b) * rx * d, math.sin(b) * ry * d
		p = c + Vector((lx * ca - ly * sa, lx * sa + ly * ca, rz * 0.45))
		out.append(geo.blob((rx * s, ry * s, rz * 0.8), p, (0.0, 0.0, math.degrees(angle)), mat=top, lumpy=0.25, seed=seed * 10 + k, name="pad_lump"))
	return out


def build():
	bark_low = mat.make("pine_bark_grey", (0.31, 0.25, 0.21), pattern="bark", rough=0.95, bump=0.75)
	bark_red = mat.make("pine_bark_red", (0.47, 0.27, 0.18), pattern="bark", rough=0.9, bump=0.6)
	needles = mat.get("pine")
	needles_top = mat.make("pine_light", (0.18, 0.30, 0.21), pattern="leaf", rough=0.85, bump=0.5)
	glow = mat.get("spore_glow")

	spine = [(0.0, 0.0, -0.05), (0.05, 0.0, 0.5), (0.2, 0.02, 1.3), (0.32, 0.05, 2.0), (0.22, 0.05, 2.7), (0.0, 0.02, 3.4), (-0.12, 0.0, 4.0), (-0.08, -0.02, 4.7)]
	widths = [0.2, 0.165, 0.145, 0.13, 0.115, 0.1, 0.085, 0.07]
	pts, rad = spline(spine, widths)
	cut = len(pts) // 2
	wood = [geo.tube(pts[:cut + 1], rad[:cut + 1], mat=bark_low, segments=12, name="trunk_low"),
			geo.tube(pts[cut - 1:], rad[cut - 1:], mat=bark_red, segments=10, name="trunk_red")]
	wood.append(geo.lathe([(0.32, -0.05), (0.24, 0.1), (0.19, 0.3), (0.165, 0.5)], mat=bark_low, segments=12, name="flare"))
	rnd = geo.rng(9)
	for k in range(4):
		a = k / 4.0 * math.tau + rnd.uniform(-0.4, 0.4)
		r = rnd.uniform(0.42, 0.55)
		wood.append(geo.tube([(0.0, 0.0, 0.3), (math.cos(a) * 0.22, math.sin(a) * 0.22, 0.08), (math.cos(a) * r, math.sin(a) * r, -0.04)],
							 [0.09, 0.06, 0.03], mat=bark_low, segments=7, name="root"))

	def trunk_at(z):
		for i in range(len(pts) - 1):
			if pts[i].z <= z <= pts[i + 1].z:
				t = (z - pts[i].z) / max(pts[i + 1].z - pts[i].z, 1e-4)
				return pts[i].lerp(pts[i + 1], t)
		return pts[-1]

	leaves = []
	arms = [
		(2.2, -20, 0.76, (0.66, 0.52, 0.2), 21),
		(2.35, 160, 0.7, (0.6, 0.5, 0.19), 22),
		(2.8, 70, 0.72, (0.64, 0.52, 0.2), 23),
		(2.9, 235, 0.74, (0.6, 0.5, 0.2), 24),
		(3.35, 10, 0.68, (0.6, 0.5, 0.19), 25),
		(3.45, 130, 0.64, (0.58, 0.48, 0.19), 26),
		(3.9, 285, 0.58, (0.55, 0.46, 0.18), 27),
		(4.0, 195, 0.52, (0.52, 0.44, 0.18), 28),
	]
	for (z, deg, d, r, seed) in arms:
		a = math.radians(deg)
		c = Vector((math.cos(a) * d * 1.06, math.sin(a) * d * 1.06, z))
		s = trunk_at(z - 0.3)
		e = c + Vector((0.0, 0.0, -0.08))
		mid = s.lerp(e, 0.55) + Vector((0.0, 0.0, -0.05))
		wood.append(geo.tube([s, mid, e], [0.075, 0.055, 0.045], mat=bark_red, segments=8, name="arm"))
		leaves += pad(c, (r[0] * 1.06, r[1] * 1.06, r[2]), a, needles, needles_top, seed)
	leaves += pad((-0.08, 0.0, 4.8), (0.9, 0.8, 0.25), 0.3, needles, needles_top, 31, lumps=4)
	leaves += pad((0.3, 0.3, 4.5), (0.55, 0.45, 0.2), 0.8, needles, needles_top, 32)
	for p in leaves:
		geo.tag(p, "sway", "foliage")

	g = geo.blob(0.055, (0.5, -0.8, 1.92), mat=glow, subdivisions=2, name="mote")
	geo.tag(g, "pulse", "sway")

	return pipeline.mixed("pine", [g], wood + leaves, voxel=0.025, tris=3600, size=512)
