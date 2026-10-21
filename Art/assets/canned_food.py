# A can of peaches (pickup): tin body with rolled rims and a ring pull, a faded peach-orange label with
# "peach" lettered round it and a peach drawn on, one dent, a little rust. Spins and hovers on the ground.
import math

import bmesh
from mathutils import Vector

from gse import geo, mat, pipeline

R_LABEL = 0.1318


def flat_text(body, size, loc, rot, m, angle=18.0):
	# Lettering: geo.text with only the front faces kept and the curves thinned.
	obj = geo.text(body, size, loc, rot, m, depth=0.002)
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bm.normal_update()
	bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < 0.5], context='FACES')
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(angle), use_dissolve_boundaries=False, verts=bm.verts[:], edges=bm.edges[:])
	bmesh.ops.triangulate(bm, faces=bm.faces[:])
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def wrap(obj, angle0):
	# Bends a flat part made facing -Y round the can: its x becomes an angle round the axis.
	geo.apply_modifiers(obj)
	geo.apply_transform(obj)
	for v in obj.data.vertices:
		r = -v.co.y
		a = angle0 + v.co.x / R_LABEL
		v.co.x = r * math.sin(a)
		v.co.y = -r * math.cos(a)
	obj.data.update()
	return obj


def dent(obj, centre, radius, depth):
	geo.apply_modifiers(obj)
	geo.apply_transform(obj)
	c = Vector(centre)
	for v in obj.data.vertices:
		d = (v.co - c).length
		if d < radius:
			f = 0.5 + 0.5 * math.cos(math.pi * d / radius)
			h = Vector((v.co.x, v.co.y, 0.0))
			if h.length > 1e-4:
				v.co -= h.normalized() * depth * f
	obj.data.update()
	return obj


def build():
	tin = mat.make("can_tin", (0.56, 0.57, 0.56), pattern="rust", rough=0.5, bump=0.3, amount=1.0)
	label = mat.make("can_label", (0.76, 0.50, 0.32), pattern="paper", rough=0.85, bump=0.15, edge=0.1)
	band = mat.make("can_band", (0.84, 0.80, 0.68), pattern="paper", rough=0.85, bump=0.1, edge=0.05)
	ink = mat.make("can_ink", (0.40, 0.15, 0.12), rough=0.8, bump=0.0, edge=0.0)
	peach = mat.make("can_peach", (0.88, 0.62, 0.46), rough=0.6, bump=0.05, grime=0.2)
	blush = mat.make("can_blush", (0.80, 0.42, 0.34), rough=0.6, bump=0.05, grime=0.2)
	leaf = mat.get("leaf")
	metal = mat.get("metal")

	parts = []

	# Tin body: rolled rims top and bottom, the lid sunk a little inside the top rim.
	rings = [0.25, 0.29, 0.33, 0.37, 0.41, 0.45]
	body = geo.lathe([(0.118, 0.19), (0.131, 0.192), (0.134, 0.204), (0.129, 0.215)] + [(0.129, z) for z in rings] +
					 [(0.129, 0.485), (0.134, 0.496), (0.133, 0.51), (0.122, 0.51), (0.120, 0.502)], mat=tin, segments=18, name="can")
	parts.append(body)
	parts.append(geo.lathe([(R_LABEL - 0.0015, z) for z in [0.225] + rings + [0.475]], mat=label, segments=18,
						   close_top=False, close_bottom=False, name="label"))
	for z in (0.236, 0.464):
		parts.append(geo.lathe([(R_LABEL, z - 0.009), (R_LABEL, z + 0.009)], mat=band, segments=18, close_top=False, close_bottom=False, name="label_band"))

	# Ring pull on the lid.
	parts.append(geo.box((0.03, 0.05, 0.004), (0.0, -0.03, 0.505), mat=metal, name="tab"))
	ring = [(0.024 * math.cos(a / 8.0 * math.tau), -0.075 + 0.02 * math.sin(a / 8.0 * math.tau), 0.506) for a in range(9)]
	parts.append(geo.tube(ring, 0.004, mat=metal, segments=4, caps=False, name="ring"))

	# The name on both sides, a peach drawn under the front one.
	for a0 in (0.0, math.pi):
		parts.append(wrap(flat_text("복숭아", 0.068, (0, -R_LABEL - 0.0025, 0.405), (90, 0, 0), ink), a0))
	fruit = geo.cylinder(0.042, 0.003, (0.0, -R_LABEL - 0.001, 0.31), (90, 0, 0), mat=peach, segments=12, name="peach")
	fruit.scale = (1.05, 1.0, 1.0)
	parts.append(wrap(fruit, 0.0))
	parts.append(wrap(geo.cylinder(0.024, 0.003, (0.015, -R_LABEL - 0.002, 0.30), (90, 0, 0), mat=blush, segments=10, name="blush"), 0.0))
	parts.append(wrap(geo.box((0.03, 0.003, 0.014), (0.03, -R_LABEL - 0.002, 0.352), (0, 30, 0), mat=leaf, name="peach_leaf"), 0.0))
	parts.append(wrap(geo.box((0.004, 0.003, 0.05), (-0.004, -R_LABEL - 0.0025, 0.31), (0, -8, 0), mat=blush, name="peach_cleft"), 0.0))

	# A dent in the side between the two labels, and the rim knocked in above it.
	for p in parts:
		if p.name.startswith(("can", "label")):
			dent(p, (0.128, 0.02, 0.37), 0.095, 0.04)
			dent(p, (0.125, -0.03, 0.50), 0.05, 0.016)

	for p in parts:
		geo.tag(p, "spin", "hover")
	geo.moss_mask(body, seed=1)

	return pipeline.static("canned_food", parts, size=256)
