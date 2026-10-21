# Relic: a Ginkgo Town Middle School badge. A shield of tarnished brass and green enamel, "은행" under a gold
# ginkgo leaf that has kept its shine while the rest went dull; a safety pin on the back.
import math

from gse import geo, mat, pipeline

SW, SH = 0.26, 0.30


def shield(scale=1.0):
	# Shield outline (x, z) centred on the origin: flat top with rounded shoulders, sides curving to a point.
	pts = []
	w, h = SW * 0.5 * scale, SH * 0.5 * scale
	pts.append((0.0, -h))
	for k in range(1, 7):
		t = k / 6.0
		pts.append((w * math.sin(t * math.pi * 0.5) ** 0.8, -h + (h * 1.25) * (1.0 - math.cos(t * math.pi * 0.5))))
	pts.append((w, h * 0.80))
	pts.append((w * 0.92, h * 0.97))
	pts.append((w * 0.55, h))
	pts.append((w * 0.30, h * 0.94))
	pts.append((0.0, h * 1.0))
	right = pts[1:]
	left = [(-x, z) for (x, z) in reversed(right[:-1])]
	return [pts[0]] + right + left


def leaf_outline(cx, cz, r):
	# A ginkgo leaf: narrow at the stem, curving sides opening to a wavy edge with its notch.
	pts = [(cx + 0.03 * r, cz), (cx + 0.22 * r, cz + 0.12 * r), (cx + 0.50 * r, cz + 0.30 * r), (cx + 0.72 * r, cz + 0.44 * r)]
	for (deg, f) in ((34, 1.0), (46, 1.02), (58, 0.98), (70, 1.0), (80, 0.97), (86, 0.95), (90, 0.66), (94, 0.95), (100, 0.97),
					 (110, 1.0), (122, 0.98), (134, 1.02), (146, 1.0)):
		a = math.radians(deg)
		pts.append((cx + math.cos(a) * r * f, cz + math.sin(a) * r * f * 0.92))
	pts += [(cx - 0.72 * r, cz + 0.44 * r), (cx - 0.50 * r, cz + 0.30 * r), (cx - 0.22 * r, cz + 0.12 * r), (cx - 0.03 * r, cz)]
	return pts


def slab(outline, thick, y_front, m, name, bevel=0.0):
	return geo.polygon_slab(outline, thick, (0.0, y_front + thick, 0.0), (90, 0, 0), mat=m, name=name, bevel=bevel)


def settle(parts, rot, loc):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	brass = mat.make("badge_brass", (0.62, 0.52, 0.30), pattern="rust", rough=0.45, bump=0.2, amount=0.45, accent=(0.36, 0.45, 0.38))
	enamel = mat.make("badge_enamel", (0.20, 0.36, 0.34), pattern="plain", rough=0.3, var=0.08, grime=0.3, edge=0.1, bump=0.03)
	gold = mat.make("badge_gold", (0.86, 0.70, 0.28), pattern="plain", rough=0.3, var=0.06, grime=0.2, edge=0.4, bump=0.03)
	cream = mat.make("badge_cream", (0.86, 0.80, 0.62), pattern="plain", rough=0.4, grime=0.1, edge=0.1, bump=0.0)
	steel = mat.make("badge_pin", (0.70, 0.70, 0.66), pattern="plain", rough=0.3, grime=0.2, edge=0.3, bump=0.0)

	parts = []
	T = 0.022
	parts.append(slab(shield(1.0), T, -T * 0.5, brass, "rim", bevel=0.005))
	parts.append(slab(shield(0.84), 0.004, -T * 0.5 - 0.002, enamel, "field"))
	# A cream band across the lower part carrying the lettering.
	parts.append(slab([(-0.105, -0.035), (0.105, -0.035), (0.10, -0.085), (-0.10, -0.085)], 0.003, -T * 0.5 - 0.005, cream, "band"))
	parts.append(slab([(-0.112, -0.031), (0.112, -0.031), (0.112, -0.035), (-0.112, -0.035)], 0.004, -T * 0.5 - 0.006, gold, "band_line"))
	parts.append(slab([(-0.104, -0.085), (0.104, -0.085), (0.098, -0.089), (-0.098, -0.089)], 0.004, -T * 0.5 - 0.006, gold, "band_line"))
	t = geo.text("은행", 0.052, (0.0, -T * 0.5 - 0.009, -0.061), (90, 0, 0), mat=enamel, depth=0.002, name="eunhaeng")
	parts.append(geo.decimate(t, 260))
	# The ginkgo leaf, raised and bright, its veins pressed in fan lines.
	lx, lz = 0.0, -0.012
	parts.append(slab(leaf_outline(lx, lz, 0.105), 0.006, -T * 0.5 - 0.010, gold, "leaf", bevel=0.002))
	vein = mat.make("badge_vein", (0.76, 0.61, 0.25), pattern="plain", rough=0.35, grime=0.1, edge=0.0, bump=0.0)
	for k in range(5):
		a = math.radians(48.0 + 84.0 * k / 4.0)
		parts.append(geo.tube([(lx, -T * 0.5 - 0.0105, lz + 0.01), (lx + math.cos(a) * 0.040, -T * 0.5 - 0.0105, lz + math.sin(a) * 0.036),
							   (lx + math.cos(a) * 0.084, -T * 0.5 - 0.0105, lz + math.sin(a) * 0.076)], 0.0011, mat=vein, segments=3, name="vein"))
	parts.append(geo.tube([(lx, -T * 0.5 - 0.009, lz + 0.004), (lx + 0.004, -T * 0.5 - 0.009, lz - 0.016)], 0.004, mat=gold, segments=4, name="leaf_stem"))
	# Two small stars at the shoulders.
	for s in (-1.0, 1.0):
		star = []
		for k in range(10):
			a = math.radians(90.0 + k * 36.0)
			rr = 0.014 if k % 2 == 0 else 0.006
			star.append((s * 0.085 + math.cos(a) * rr, 0.105 + math.sin(a) * rr))
		parts.append(slab(star, 0.004, -T * 0.5 - 0.006, gold, "star"))

	# Back: a hinge plate, the safety pin and its catch.
	by = T * 0.5
	parts.append(geo.box((0.03, 0.006, 0.026), (-0.075, by + 0.003, 0.04), mat=steel, bevel=0.002, bevel_segments=1, name="pin_hinge"))
	parts.append(geo.box((0.03, 0.006, 0.026), (0.075, by + 0.003, 0.04), mat=steel, bevel=0.002, bevel_segments=1, name="pin_catch"))
	parts.append(geo.tube([(0.088, by + 0.012, 0.042), (0.096, by + 0.010, 0.036), (0.090, by + 0.008, 0.032)], 0.003, mat=steel, segments=4, name="catch_hook"))
	coil = [(-0.090 + 0.010 * math.cos(a / 8.0 * math.tau), by + 0.010 + 0.003 * k / 8.0, 0.04 + 0.010 * math.sin(a / 8.0 * math.tau)) for k, a in enumerate(range(9))]
	parts.append(geo.tube(coil, 0.0028, mat=steel, segments=4, name="pin_coil"))
	parts.append(geo.tube([(-0.080, by + 0.013, 0.04), (0.092, by + 0.013, 0.038)], 0.003, mat=steel, segments=4, name="pin"))

	settle(parts, (-14, 0, 15), (0.0, 0.0, 0.35))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("rim", "field")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=17)

	return pipeline.static("relic_badge", parts, size=256)
