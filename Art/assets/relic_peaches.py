# Relic: a sealed tin of yellow peaches from before the Bloom. An old-style label, sun-faded on one side,
# "황도" over a painted peach, rolled rims and no pull ring; the shop's price sticker still on the lid.
import math

import bmesh
import bpy

from gse import core, geo, mat, pipeline

R = 0.115
H = 0.30


def wrap(objs, radius, turn=0.0):
	# Bends parts modelled flat in front of the can (facing -Y) round its side.
	for o in objs:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		for v in o.data.vertices:
			d = -v.co.y
			a = v.co.x / radius + math.radians(turn)
			v.co.x = d * math.sin(a)
			v.co.y = -d * math.cos(a)
		o.data.update()


def _flat(name, mat_obj, build_fn):
	bm = bmesh.new()
	build_fn(bm)
	mesh = bpy.data.meshes.new(name)
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, mesh))
	mesh.materials.append(mat_obj)
	for poly in mesh.polygons:
		poly.use_smooth = True
	return obj


def strip(w, h, loc, mat_obj, cuts=8, name="strip"):
	# A flat rectangle facing -Y, cut along X so it bends round the can.
	x0, y, z = loc
	def fn(bm):
		top = [bm.verts.new((x0 - w * 0.5 + w * k / cuts, y, z + h * 0.5)) for k in range(cuts + 1)]
		bot = [bm.verts.new((x0 - w * 0.5 + w * k / cuts, y, z - h * 0.5)) for k in range(cuts + 1)]
		for k in range(cuts):
			bm.faces.new((bot[k], bot[k + 1], top[k + 1], top[k]))
	return _flat(name, mat_obj, fn)


def disc(r, loc, mat_obj, segs=12, sx=1.0, name="disc"):
	# A flat disc facing -Y with an inner ring, so it bends round the can too.
	x0, y, z = loc
	def fn(bm):
		c = bm.verts.new((x0, y, z))
		mid = [bm.verts.new((x0 + math.cos(a / segs * math.tau) * r * 0.5 * sx, y, z + math.sin(a / segs * math.tau) * r * 0.5)) for a in range(segs)]
		out = [bm.verts.new((x0 + math.cos(a / segs * math.tau) * r * sx, y, z + math.sin(a / segs * math.tau) * r)) for a in range(segs)]
		for k in range(segs):
			j = (k + 1) % segs
			bm.faces.new((c, mid[k], mid[j]))
			bm.faces.new((mid[k], out[k], out[j], mid[j]))
	return _flat(name, mat_obj, fn)


def settle(parts, rot, loc):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	tin = mat.make("peach_tin", (0.62, 0.60, 0.55), pattern="rust", rough=0.35, bump=0.15, amount=0.2, accent="rust_dark")
	label = mat.make("peach_label", (0.82, 0.69, 0.42), pattern="paper", rough=0.9, var=0.14, grime=0.2, edge=0.05, bump=0.1)
	faded = mat.make("peach_label_faded", (0.84, 0.77, 0.58), pattern="paper", rough=0.9, var=0.1, grime=0.15, edge=0.05, bump=0.1)
	band = mat.make("peach_band", (0.62, 0.30, 0.18), pattern="paper", rough=0.85, var=0.12, grime=0.2, edge=0.05, bump=0.1)
	ink = mat.make("peach_ink", (0.45, 0.15, 0.10), pattern="plain", rough=0.8, grime=0.0, edge=0.0, bump=0.0)
	peach = mat.make("peach_fruit", (0.88, 0.58, 0.26), pattern="plain", rough=0.7, var=0.12, grime=0.2, edge=0.15, bump=0.05)
	blush = mat.make("peach_blush", (0.80, 0.40, 0.28), pattern="plain", rough=0.7, var=0.1, grime=0.0, edge=0.0, bump=0.0)
	stone = mat.make("peach_stone", (0.55, 0.30, 0.16), pattern="plain", rough=0.8, grime=0.0, edge=0.0, bump=0.0)
	green = mat.make("peach_leaf", (0.34, 0.48, 0.28), pattern="plain", rough=0.8, grime=0.0, edge=0.0, bump=0.0)
	sticker = mat.make("peach_sticker", (0.86, 0.84, 0.76), pattern="paper", rough=0.9, edge=0.0, bump=0.05)

	parts = []
	z0 = -H * 0.5
	can = geo.cylinder(R, H, (0, 0, 0), mat=tin, segments=20, name="can")
	parts.append(can)
	for z in (z0 + 0.004, -z0 - 0.004):
		ring = [(R * 1.02 * math.cos(a / 20.0 * math.tau), R * 1.02 * math.sin(a / 20.0 * math.tau), z) for a in range(21)]
		parts.append(geo.tube(ring, 0.009, mat=tin, segments=4, name="rim"))
	for r in (0.08, 0.05):
		parts.append(geo.cylinder(r, 0.006, (0, 0, -z0 + 0.001), mat=tin, segments=20, name="lid_bead"))
	# Ribbed bands just inside the rims.
	for z in (z0 + 0.03, -z0 - 0.03):
		parts.append(geo.cylinder(R + 0.004, 0.012, (0, 0, z), mat=tin, segments=20, name="bead"))

	# The label: warm yellow, orange bands top and bottom, paler where the sun found it.
	lab = geo.cylinder(R + 0.003, 0.20, (0, 0, -0.002), mat=label, segments=20, caps=False, name="label")
	parts.append(lab)
	for z in (0.083, -0.087):
		parts.append(geo.cylinder(R + 0.0045, 0.026, (0, 0, z), mat=band, segments=20, caps=False, name="label_band"))
	fade = strip(0.26, 0.13, (0, -(R + 0.0065), -0.002), faded, cuts=12, name="fade")
	wrap([fade], R, turn=110.0)
	parts.append(fade)

	# Front art: a peach half with its stone, a whole peach and a leaf; "황도" above.
	art = []
	art.append(disc(0.042, (-0.022, -(R + 0.0045), -0.030), peach, name="peach"))
	art.append(disc(0.017, (-0.022, -(R + 0.0055), -0.030), stone, segs=8, name="peach_stone"))
	art.append(disc(0.036, (0.034, -(R + 0.0050), -0.040), peach, name="peach"))
	art.append(disc(0.020, (0.044, -(R + 0.0060), -0.032), blush, segs=8, name="peach_blush"))
	leaf = disc(0.024, (0.0, -(R + 0.0058), 0.0), green, segs=8, sx=0.45, name="peach_leaf")
	leaf.rotation_euler = (0.0, math.radians(-50.0), 0.0)
	leaf.location = (0.010, 0.0, 0.004)
	art.append(leaf)
	text = geo.text("황도", 0.062, (0.0, -(R + 0.007), 0.036), (90, 0, 0), mat=ink, depth=0.002, name="hwangdo")
	text = geo.decimate(text, 260)
	art.append(text)
	sub = geo.text("통조림", 0.026, (0.0, -(R + 0.007), -0.088), (90, 0, 0), mat=sticker, depth=0.002, name="tongjorim")
	sub = geo.decimate(sub, 140)
	art.append(sub)
	wrap(art, R, turn=0.0)
	parts += art
	# Small print round the back.
	back = []
	for k, w in enumerate((0.12, 0.13, 0.09, 0.13, 0.11, 0.07)):
		back.append(strip(w, 0.0055, (-0.065 + w * 0.5, -(R + 0.0045), 0.050 - k * 0.019), ink, cuts=5, name="print"))
	wrap(back, R, turn=180.0)
	parts += back
	# Price sticker on the lid.
	parts.append(geo.box((0.06, 0.04, 0.003), (0.02, -0.02, -z0 + 0.006), (0, 0, 15), mat=sticker, name="sticker"))
	parts.append(geo.box((0.06, 0.008, 0.0035), (0.0221, -0.0326, -z0 + 0.0065), (0, 0, 15), mat=band, name="sticker_stripe"))

	settle(parts, (6, -8, 18), (0.0, 0.0, 0.35))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("can", "rim", "lid_bead")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=5)

	return pipeline.static("relic_peaches", parts, size=256)
