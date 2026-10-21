# A mossy boulder cluster: a chiselled main stone with two smaller ones leaning on it, a moss cushion
# on top, moss creeping over the upward faces, and a few grass tufts at the foot.
import math

from mathutils import Vector, noise

from gse import geo, mat, pipeline


def stone(radii, loc, m, seed, cuts=7, rot=(0.0, 0.0, 0.0)):
	# A lumpy blob with flat facets knocked off it, then a flat base sunk a little into the ground.
	obj = geo.blob(radii, loc, rot, mat=m, lumpy=0.12, seed=seed, name="stone")
	rnd = geo.rng(seed)
	r = Vector(radii)
	planes = []
	for k in range(cuts):
		a = rnd.uniform(0.0, math.tau)
		e = rnd.uniform(-0.25, 1.0)
		planes.append((Vector((math.cos(a) * math.cos(e), math.sin(a) * math.cos(e), math.sin(e))), rnd.uniform(0.5, 0.72)))
	for v in obj.data.vertices:
		u = Vector((v.co.x / r.x, v.co.y / r.y, v.co.z / r.z))
		for (n, d) in planes:
			h = u.dot(n)
			if h > d:
				u -= n * (h - d)
		v.co = Vector((u.x * r.x, u.y * r.y, u.z * r.z))
	obj.data.update()
	geo.apply_transform(obj)
	for v in obj.data.vertices:
		if v.co.z < -0.04:
			v.co.z = -0.04
	obj.data.update()
	return obj


def moss_shell(src, m, seed, thick=0.06, bias=0.3):
	# A copy of the stone pushed out along its normals where moss settles (up-facing, noisy patches)
	# and tucked inside elsewhere, so the remesh grows moss that drapes over the stone's own shape.
	shell = geo.duplicate(src, "moss_shell")
	shell.vertex_groups.clear()
	shell.data.materials.clear()
	shell.data.materials.append(m)
	off = Vector((seed * 4.1, seed * 2.9, seed * 1.7))
	me = shell.data
	normals = [v.normal.copy() for v in me.vertices]
	for v, n in zip(me.vertices, normals):
		w = n.z * 0.6 + noise.noise(v.co * 2.6 + off) * 0.9 - bias
		if w > 0.0:
			v.co += n * (thick * min(1.0, w * 2.5) + 0.012 * noise.noise(v.co * 14.0 + off))
		else:
			v.co -= n * 0.03
	me.update()
	return shell


def build():
	grey = mat.make("boulder", (0.40, 0.40, 0.38), pattern="plaster", rough=0.92, bump=0.6, var=0.18, var_scale=2.5, edge=0.25)
	dark = mat.make("boulder_dark", (0.32, 0.32, 0.31), pattern="plaster", rough=0.92, bump=0.6, var=0.18, var_scale=2.5, edge=0.25)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	grass = mat.make("tuft_grass", (0.27, 0.38, 0.22), pattern="leaf", rough=0.85, bump=0.3)

	stones = [
		stone((0.62, 0.52, 0.5), (0.0, 0.02, 0.33), grey, 3, cuts=10, rot=(6.0, -5.0, 12.0)),
		stone((0.32, 0.28, 0.26), (0.55, -0.33, 0.15), dark, 7, cuts=7, rot=(0.0, 12.0, -20.0)),
		stone((0.24, 0.2, 0.18), (-0.56, 0.34, 0.09), grey, 11, cuts=6, rot=(8.0, 0.0, 40.0)),
	]
	for k, s in enumerate(stones):
		geo.moss_mask(s, up=1.1, low=0.5, grain=0.55, seed=k + 2)

	cushions = [moss_shell(stones[0], moss, 1, 0.06, 0.42), moss_shell(stones[1], moss_dark, 2, 0.045, 0.36),
				moss_shell(stones[2], moss_dark, 3, 0.04, 0.45)]
	cushions += [
		geo.blob((0.3, 0.12, 0.16), (-0.2, 0.44, 0.13), (0.0, 0.0, 10.0), mat=moss_dark, lumpy=0.4, seed=23, name="moss_side"),
		geo.blob((0.2, 0.14, 0.05), (-0.42, -0.36, 0.0), mat=moss_dark, lumpy=0.45, seed=24, name="moss_foot"),
	]
	for c in cushions:
		geo.tag(c, "pulse")

	tufts = []
	rnd = geo.rng(31)
	for (cx, cy) in ((0.78, -0.02), (-0.2, -0.55), (-0.72, 0.0), (0.25, 0.55)):
		for k in range(6):
			h = rnd.uniform(0.18, 0.32)
			px, py = cx + rnd.uniform(-0.08, 0.08), cy + rnd.uniform(-0.08, 0.08)
			tilt = (rnd.uniform(-22, 22), rnd.uniform(-22, 22), rnd.uniform(0, 360))
			blade = geo.cone(rnd.uniform(0.025, 0.04), h, (px, py, h * 0.5 - 0.02), tilt, mat=grass, segments=4, name="blade")
			blade.scale = (1.0, 0.45, 1.0)
			geo.tag(blade, "sway", "foliage")
			tufts.append(blade)

	return pipeline.mixed("rock", tufts, stones + cushions, voxel=0.016, tris=2600, size=512)
