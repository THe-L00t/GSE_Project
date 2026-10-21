# A pickup: a small bundle of medicinal herb (angelica-like) tied with twine, its stems splayed below
# the knot, lobed leaves fanning above, and one pale umbel glowing with spores on the tallest stem.
import math

from mathutils import Vector

from assets.vegetable import blade
from gse import geo, mat, pipeline


def build():
	stem_m = mat.make("herb_stem", (0.40, 0.47, 0.29), pattern="plain", rough=0.7, bump=0.1)
	leaf_m = mat.make("herb_leaf", (0.31, 0.47, 0.28), pattern="leaf", rough=0.75, bump=0.3)
	twine = mat.make("twine", (0.62, 0.55, 0.40), pattern="cloth", rough=0.95, bump=0.4)
	glow = mat.get("spore_glow")

	parts = []
	tie = 0.27
	rnd = geo.rng(4)
	tops = []
	for k in range(6):
		a = k / 6.0 * math.tau + rnd.uniform(-0.3, 0.3)
		d = Vector((math.cos(a), math.sin(a), 0.0))
		base = d * 0.03 + Vector((0, 0, 0.09))
		mid = d * 0.014 + Vector((0, 0, tie))
		top = d * rnd.uniform(0.07, 0.1) + Vector((0, 0, rnd.uniform(0.44, 0.52)))
		parts.append(geo.tube([base, mid, mid.lerp(top, 0.5) + d * 0.01, top], [0.0075, 0.007, 0.0065, 0.005], mat=stem_m, segments=5, name="stem"))
		tops.append((d, top))
	# Leaves fan out and droop from the upper stems.
	for (d, top) in tops:
		for (f, lift, turn) in ((0.55, 0.0, -0.5), (1.0, 0.02, 0.45)):
			p = Vector((0, 0, tie)).lerp(top, f) + d * 0.004
			ang = math.atan2(d.y, d.x) + turn
			o = Vector((math.cos(ang), math.sin(ang), 0.0))
			ln = rnd.uniform(0.12, 0.15)
			ctrl = [p, p + o * ln * 0.45 + Vector((0, 0, ln * 0.4 + lift)), p + o * ln + Vector((0, 0, ln * 0.2 + lift))]
			bw = rnd.uniform(0.03, 0.035)
			parts.append(blade(ctrl, lambda u, bw=bw: bw * math.sin(min(1.0, u * 1.15) * math.pi) ** 0.7 + 0.002,
							   (leaf_m,), thick=0.003, fold=0.18, lobes=2, lobe_depth=0.55, facing=o, steps=3, name="leaf"))

	# The tallest stem carries the umbel.
	crown = Vector((0.005, -0.004, 0.6))
	parts.append(geo.tube([Vector((0, 0, 0.09)), Vector((0.0, 0.0, tie)), crown], [0.008, 0.0075, 0.006], mat=stem_m, segments=5, name="stalk"))
	flower = [geo.blob((0.03, 0.03, 0.022), crown + Vector((0, 0, 0.012)), mat=glow, subdivisions=2, name="umbel")]
	for k in range(7):
		a = k / 7.0 * math.tau
		r = Vector((math.cos(a) * 0.034, math.sin(a) * 0.034, 0.0))
		flower.append(geo.blob(0.016, crown + r * 0.9 + Vector((0, 0, 0.006)), mat=glow, subdivisions=1, name="floret"))
	for f in flower:
		geo.tag(f, "pulse")
	parts += flower

	# Twine wound round the bundle, a knot and two loose ends.
	ring = [(0.024 * math.cos(i / 14.0 * math.tau), 0.024 * math.sin(i / 14.0 * math.tau), tie + 0.004 * math.sin(i / 14.0 * math.tau * 2.0)) for i in range(15)]
	parts.append(geo.tube(ring, 0.0055, mat=twine, segments=4, name="twine"))
	parts.append(geo.tube([(p[0], p[1], p[2] - 0.014) for p in ring], 0.005, mat=twine, segments=4, name="twine2"))
	parts.append(geo.blob(0.011, (0.0, -0.028, tie - 0.006), mat=twine, subdivisions=1, name="knot"))
	parts.append(geo.tube([(0.0, -0.03, tie - 0.01), (0.012, -0.04, tie - 0.04), (0.02, -0.045, tie - 0.07)], 0.004, mat=twine, segments=4, name="end"))
	parts.append(geo.tube([(0.0, -0.03, tie - 0.01), (-0.014, -0.038, tie - 0.035), (-0.02, -0.042, tie - 0.058)], 0.004, mat=twine, segments=4, name="end"))

	for p in parts:
		geo.tag(p, "spin", "hover")
	return pipeline.static("herb", parts, size=256)
