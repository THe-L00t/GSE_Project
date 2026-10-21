# An old aluminium canteen (pickup) in a faded water-blue cloth cover with snaps and a belt loop, a screw cap
# held on a bead chain, a drop of water on the shoulder. Spins and hovers on the ground.
import math

from mathutils import Vector

from gse import geo, mat, pipeline

FLAT = 0.62


def flatten(obj):
	# The canteen is oval, flatter front to back.
	obj.scale = (1.0, FLAT, 1.0)
	geo.apply_transform(obj)
	return obj


def dent(obj, centre, radius, depth):
	c = Vector(centre)
	for v in obj.data.vertices:
		d = (v.co - c).length
		if d < radius:
			f = 0.5 + 0.5 * math.cos(math.pi * d / radius)
			h = Vector((v.co.x, v.co.y / FLAT, 0.0))
			if h.length > 1e-4:
				h = h.normalized()
				v.co -= Vector((h.x, h.y * FLAT, 0.0)) * depth * f
	obj.data.update()
	return obj


def build():
	alu = mat.make("flask_alu", (0.60, 0.63, 0.64), pattern="rust", rough=0.45, bump=0.3, amount=0.3, accent=(0.42, 0.44, 0.44))
	cover = mat.make("flask_cover", (0.36, 0.47, 0.52), pattern="cloth", rough=0.92, bump=0.5)
	hem = mat.make("flask_hem", (0.26, 0.35, 0.40), pattern="cloth", rough=0.92, bump=0.4)
	cap_mat = mat.make("flask_cap", (0.22, 0.24, 0.25), rough=0.6, bump=0.15)
	water = mat.make("flask_water", (0.55, 0.72, 0.82), rough=0.1, bump=0.0, grime=0.0, edge=0.3)
	metal = mat.get("metal")

	parts = []

	# Aluminium shoulder and neck above the cover (the body inside it is not modelled).
	body = geo.lathe([(0.150, 0.44), (0.147, 0.48), (0.132, 0.51), (0.105, 0.535), (0.075, 0.552), (0.05, 0.56)],
					 mat=alu, segments=18, close_bottom=False, name="shoulder")
	flatten(body)
	dent(body, (0.10, -0.06, 0.50), 0.06, 0.012)
	parts.append(body)
	parts.append(geo.lathe([(0.042, 0.55), (0.042, 0.585), (0.047, 0.59), (0.047, 0.598), (0.042, 0.603)],
						   mat=alu, segments=12, close_bottom=False, name="neck"))
	parts.append(geo.cylinder(0.052, 0.012, (0, 0, 0.565), mat=alu, segments=12, name="neck_ring"))

	# Screw cap with ridges, a little tab for the chain.
	parts.append(geo.lathe([(0.053, 0.598), (0.055, 0.60), (0.055, 0.638), (0.05, 0.645), (0.0, 0.647)],
						   mat=cap_mat, segments=14, name="cap"))
	for k in range(7):
		a = math.tau * k / 7.0
		parts.append(geo.box((0.008, 0.008, 0.032), (math.cos(a) * 0.055, math.sin(a) * 0.055, 0.62), (0, 0, math.degrees(a)), mat=cap_mat, name="cap_ridge"))
	parts.append(geo.box((0.022, 0.012, 0.016), (0.064, 0.0, 0.612), mat=cap_mat, name="cap_tab"))

	# Bead chain from the cap tab, sagging round to the neck ring.
	chain = [(0.074, 0.0, 0.608), (0.092, -0.012, 0.588), (0.100, -0.022, 0.562), (0.092, -0.030, 0.540), (0.074, -0.034, 0.548), (0.056, -0.026, 0.562)]
	parts.append(geo.tube(chain, 0.0035, mat=metal, segments=4, name="chain"))
	for p in chain[1:-1]:
		parts.append(geo.sphere(0.0065, p, mat=metal, segments=6, rings=4, name="bead"))

	# The cloth cover: bottom to shoulder, a darker hem round the top, two snaps and a belt loop behind.
	cloth = geo.lathe([(0.0, 0.098), (0.10, 0.10), (0.142, 0.112), (0.158, 0.15), (0.161, 0.25), (0.160, 0.36), (0.157, 0.44), (0.152, 0.47)],
					  mat=cover, segments=18, close_top=False, name="cover")
	flatten(cloth)
	geo.jitter(cloth, 0.004, scale=12.0, seed=2)
	parts.append(cloth)
	hem_ring = [(0.153 * math.cos(a / 16.0 * math.tau), 0.153 * FLAT * math.sin(a / 16.0 * math.tau), 0.468) for a in range(17)]
	parts.append(geo.tube(hem_ring, 0.009, mat=hem, segments=5, caps=False, name="hem"))
	for x in (-0.075, 0.075):
		parts.append(geo.box((0.05, 0.012, 0.09), (x, -0.155 * FLAT * 0.93, 0.43), (8, 0, 0), mat=hem, name="flap"))
		parts.append(geo.cylinder(0.012, 0.008, (x, -0.155 * FLAT * 0.93 - 0.008, 0.415), (90, 0, 0), mat=metal, segments=8, name="snap"))
	parts.append(geo.box((0.10, 0.025, 0.20), (0.0, 0.161 * FLAT + 0.008, 0.33), mat=hem, bevel=0.006, bevel_segments=1, name="belt_loop"))
	parts.append(geo.box((0.16, 0.006, 0.008), (0.0, -0.161 * FLAT - 0.002, 0.20), mat=hem, name="seam"))

	# One drop of water on the shoulder.
	parts.append(geo.sphere((0.011, 0.011, 0.016), (-0.07, -0.072, 0.528), mat=water, segments=8, rings=6, name="drop"))

	for p in parts:
		geo.tag(p, "spin", "hover")
	geo.moss_mask(cloth, seed=3)

	return pipeline.static("water_flask", parts, size=256)
