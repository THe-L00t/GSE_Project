# A clump of reeds (galdae) standing at the water's edge: stiff stems splaying out of one root, long narrow
# leaves arching off them, and pale feathery plumes nodding at the tips. The deer slips away into these.
import math

from mathutils import Vector

from assets.vegetable import blade
from gse import geo, mat, pipeline


def build():
	stem_m = mat.make("reed_stem", (0.52, 0.55, 0.36), pattern="plain", rough=0.75, bump=0.1, grime=0.3)
	leaf_m = mat.make("reed_leaf", (0.36, 0.46, 0.28), pattern="leaf", rough=0.8, bump=0.25)
	dry_m = mat.make("reed_leaf_dry", (0.60, 0.55, 0.38), pattern="leaf", rough=0.85, bump=0.25)
	plume_m = mat.make("reed_plume", (0.74, 0.66, 0.55), pattern="fur", rough=0.95, bump=0.45, grime=0.2, edge=0.3)
	root_m = mat.make("reed_root", (0.24, 0.27, 0.18), pattern="moss", rough=0.9, bump=0.4)

	rnd = geo.rng(17)
	parts = []
	foliage = []

	# The root: a low mat of old leaves and mud the stems rise out of.
	parts.append(geo.blob((0.22, 0.2, 0.06), (0.0, 0.0, 0.02), mat=root_m, lumpy=0.35, seed=3, name="root"))

	count = 15
	for k in range(count):
		a = k / float(count) * math.tau + rnd.uniform(-0.25, 0.25)
		d = Vector((math.cos(a), math.sin(a), 0.0))
		lean = rnd.uniform(0.06, 0.24)
		height = rnd.uniform(1.05, 1.6)
		base = d * rnd.uniform(0.02, 0.12) + Vector((0.0, 0.0, 0.03))
		mid = base + d * lean * 0.45 + Vector((0.0, 0.0, height * 0.55))
		top = base + d * lean + Vector((0.0, 0.0, height))
		stem = geo.tube([base, mid, top], [0.012, 0.009, 0.005], mat=stem_m, segments=4, name="stem")
		foliage.append(stem)

		# One or two leaves leave the stem low down and arch outward, the older ones gone straw coloured.
		for leaf in range(rnd.choice((1, 2, 2))):
			f = rnd.uniform(0.15, 0.45)
			p = base.lerp(top, f)
			turn = rnd.uniform(-0.9, 0.9)
			o = Vector((math.cos(a + turn), math.sin(a + turn), 0.0))
			ln = rnd.uniform(0.42, 0.62)
			ctrl = [p, p + o * ln * 0.35 + Vector((0, 0, ln * 0.45)), p + o * ln * 0.8 + Vector((0, 0, ln * 0.35)), p + o * ln + Vector((0, 0, ln * 0.05))]
			m = dry_m if rnd.random() < 0.3 else leaf_m
			w = rnd.uniform(0.016, 0.022)
			foliage.append(blade(ctrl, lambda u, w=w: w * (1.0 - u * 0.85) + 0.002, (m,), thick=0.003, fold=0.25, facing=o, steps=3, name="leaf"))

		# Most stems carry a plume, nodding over to one side.
		if rnd.random() < 0.7:
			nod = d * 0.08 + Vector((0.0, 0.0, -0.02))
			head = top + Vector((0.0, 0.0, 0.02))
			tip = head + nod + Vector((0.0, 0.0, 0.2))
			centre = head.lerp(tip, 0.5)
			axis = (tip - head).normalized()
			q = axis.to_track_quat('Z', 'Y').to_euler()
			rot = tuple(math.degrees(x) for x in q)
			plume = geo.blob((0.04, 0.032, 0.13), centre, rot, mat=plume_m, lumpy=0.4, seed=k + 20, subdivisions=2, name="plume")
			parts.append(plume)
			geo.tag(plume, "sway")

	for o in foliage:
		geo.tag(o, "sway", "foliage")
	parts += foliage
	return pipeline.static("reeds", parts, size=256)
