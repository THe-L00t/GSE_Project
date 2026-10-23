# A 4 m run of W-beam guardrail on three steel posts: galvanised grey streaked with rust, a dent knocked
# into the middle span with the paint scraped off, a yellow reflector on the middle post, road dirt splashed
# up the lower beam, moss along the top and grass round the posts. The rail faces the road on -Y; both ends
# are cut flat so runs chain end to end.
import math

import bmesh
import bpy

from gse import core, geo, mat, pipeline

L = 4.0
RAIL_Z = 0.665
RAIL_Y = -0.142
T = 0.012
# W-beam front face, top to bottom: (height from the rail centre, reach toward the road).
PROFILE = [(0.155, 0.0), (0.125, 0.048), (0.085, 0.083), (0.045, 0.066), (0.0, 0.02), (-0.045, 0.066), (-0.085, 0.083),
		   (-0.125, 0.048), (-0.155, 0.0)]
POSTS = (-L / 3.0, 0.0, L / 3.0)
POST_Y = 0.15
DENT_X, DENT_R = 0.70, 0.58
XS = [-2.0, -1.0, 0.0, 0.12, 0.30, 0.50, 0.70, 0.90, 1.10, 1.28, 2.0]


def dent(x, h):
	# Struck high: the upper ridge folds back further than the lower one, and the beam sags a little.
	t = (x - DENT_X) / DENT_R
	if abs(t) >= 1.0:
		return 0.0, 0.0
	k = (1.0 - t * t) ** 2
	return k * (0.07 + 0.065 * h / 0.155), -k * 0.05


def weather(m, mud, top, streak="rust", scar=None):
	# Rust runs down the steel, road dirt splashed up from below, and bare rusted metal where it was struck.
	g = mat.Graph(m)
	nt = m.node_tree
	bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
	c = bsdf.inputs["Base Color"].links[0].from_socket
	co = g.node("ShaderNodeTexCoord").outputs["Object"]
	sep = g.node("ShaderNodeSeparateXYZ")
	g.link(co, sep.inputs[0])
	runs = mat._noise(g, mat._stretch(g, co, 16.0, 16.0, 0.6), 2.5, 3.0, 0.6)
	c = g.mix(g.ramp(runs, 0.55, 0.65, 0.0, 0.8), c, mat.col(streak))
	if scar is not None:
		sx, sz, r = scar
		off = g.node("ShaderNodeVectorMath", operation='SUBTRACT')
		g.link(co, off.inputs[0])
		off.inputs[1].default_value = (sx, 0.0, sz)
		flat = g.node("ShaderNodeVectorMath", operation='MULTIPLY')
		g.link(off.outputs[0], flat.inputs[0])
		flat.inputs[1].default_value = (1.0, 0.0, 2.4)
		dist = g.node("ShaderNodeVectorMath", operation='LENGTH')
		g.link(flat.outputs[0], dist.inputs[0])
		ragged = g.math('ADD', dist.outputs["Value"], g.math('MULTIPLY', mat._noise(g, co, 9.0, 3.0, 0.6), 0.22))
		c = g.mix(g.ramp(ragged, r, r + 0.06, 0.9, 0.0), c, mat.col((0.44, 0.27, 0.17)))
	speck = mat._noise(g, co, 26.0, 2.0, 0.7)
	h = g.math('ADD', sep.outputs["Z"], g.math('MULTIPLY', g.math('SUBTRACT', speck, 0.5), 0.2))
	c = g.mix(g.ramp(h, top - 0.03, top + 0.03, 0.75, 0.0), c, mat.col(mud))
	nt.links.new(c, bsdf.inputs["Base Color"])
	return m


def rail(steel):
	# The beam swept along X, closed with its thickness, ends capped flat.
	bm = bmesh.new()
	rings = []
	for x in XS:
		pts = []
		for (h, d) in PROFILE:
			dy, dz = dent(x, h)
			pts.append((x, RAIL_Y - d + dy, RAIL_Z + h + dz))
		for (h, d) in reversed(PROFILE):
			dy, dz = dent(x, h)
			pts.append((x, RAIL_Y - d + T + dy, RAIL_Z + h + dz))
		rings.append([bm.verts.new(p) for p in pts])
	n = len(rings[0])
	m = len(PROFILE)
	for a, b in zip(rings, rings[1:]):
		for k in range(n):
			k2 = (k + 1) % n
			bm.faces.new((a[k], a[k2], b[k2], b[k]))
	for ring in (rings[0], rings[-1]):
		for k in range(m - 1):
			bm.faces.new((ring[k], ring[k + 1], ring[n - 2 - k], ring[n - 1 - k]))
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	me = bpy.data.meshes.new("rail")
	bm.to_mesh(me)
	bm.free()
	obj = core.link(bpy.data.objects.new("rail", me))
	me.materials.append(steel)
	for p in me.polygons:
		p.use_smooth = True
	me.set_sharp_from_angle(angle=math.radians(50.0))
	return obj


def build():
	steel = weather(mat.make("guardrail_steel", (0.47, 0.48, 0.47), pattern="rust", rough=0.6, bump=0.25, amount=0.35, accent="rust"),
					(0.30, 0.26, 0.20), 0.635, scar=(DENT_X, RAIL_Z + 0.04, 0.24))
	post_m = weather(mat.make("guardrail_post", (0.42, 0.43, 0.42), pattern="rust", rough=0.62, bump=0.25, amount=0.45, accent="rust"),
					 (0.28, 0.24, 0.19), 0.32, streak="rust_dark")
	reflector = mat.make("guardrail_reflector", (0.74, 0.60, 0.20), pattern="plain", rough=0.35, bump=0.05, grime=0.25, edge=0.1)
	metal = mat.get("metal")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	grass = mat.make("guardrail_grass", (0.30, 0.40, 0.24), pattern="leaf", rough=0.85, bump=0.4)
	dry = mat.make("guardrail_dry_grass", (0.46, 0.43, 0.30), pattern="leaf", rough=0.9, bump=0.3)
	glow = mat.get("spore_glow")
	rnd = geo.rng(17)

	parts = []
	man_made = []

	beam = rail(steel)
	parts.append(beam)
	man_made.append(beam)

	# Posts behind the beam with a spacer block each and a bolt through the valley.
	for x in POSTS:
		p = geo.cylinder(0.075, 0.81, (x, POST_Y, 0.405), mat=post_m, segments=8, name="post")
		sp = geo.box((0.07, 0.23, 0.20), (x, -0.035, RAIL_Z), mat=post_m, name="spacer")
		man_made += [p, sp]
		parts += [p, sp]
		parts.append(geo.cylinder(0.024, 0.02, (x, RAIL_Y - 0.02 - 0.008, RAIL_Z), (90, 0, 0), mat=metal, segments=6, name="bolt"))

	# Yellow reflector on a short stalk above the middle post, facing the road.
	parts.append(geo.box((0.03, 0.03, 0.12), (0.0, POST_Y - 0.01, 0.87), mat=metal, name="stalk"))
	parts.append(geo.box((0.13, 0.014, 0.15), (0.0, POST_Y - 0.03, 0.96), mat=reflector, bevel=0.006, bevel_segments=1, name="reflector"))

	# Moss along the top of the beam and on two post tops.
	for (x, w, m, seed) in ((-1.55, 0.30, moss, 1), (-0.30, 0.18, moss_dark, 2), (1.62, 0.26, moss, 3)):
		b = geo.blob((w, 0.055, 0.035), (x, RAIL_Y - 0.03, RAIL_Z + 0.16), (0, 0, rnd.uniform(-4, 4)), mat=m, subdivisions=2, lumpy=0.35, seed=seed, name="moss_top")
		if seed == 1:
			geo.tag(b, "pulse")
		parts.append(b)
	for x in (POSTS[0], POSTS[2]):
		parts.append(geo.blob((0.075, 0.075, 0.03), (x, POST_Y, 0.815), mat=moss_dark, subdivisions=1, lumpy=0.3, seed=int(x * 10), name="moss_cap"))

	# Moss heaped round the post feet, grass tufts pushing up both sides of the beam.
	for i, x in enumerate(POSTS):
		parts.append(geo.blob((0.20, 0.10, 0.06), (x + rnd.uniform(-0.04, 0.04), POST_Y - 0.06, 0.0), mat=moss if i % 2 else moss_dark, subdivisions=2,
							  lumpy=0.35, seed=10 + i, name="moss_foot"))
	tufts = [(POSTS[0] + 0.05, 0.13, 8), (POSTS[0] - 0.10, -0.13, 6), (POSTS[1] + 0.10, 0.13, 7), (POSTS[1] - 0.18, -0.13, 5),
			 (POSTS[2] - 0.05, 0.13, 8), (POSTS[2] + 0.12, -0.13, 6), (-1.88, -0.12, 4), (0.62, 0.12, 5), (-0.72, 0.13, 4), (1.84, 0.12, 4)]
	for (cx, cy, n) in tufts:
		for k in range(n):
			hgt = rnd.uniform(0.16, 0.40)
			a = math.tau * k / n + rnd.uniform(-0.4, 0.4)
			lean = rnd.uniform(8, 20)
			w = geo.cone(rnd.uniform(0.024, 0.036), hgt, (cx + math.cos(a) * 0.03, cy + math.sin(a) * 0.02, hgt * 0.45),
						 (-math.sin(a) * lean, math.cos(a) * lean, 0), mat=(grass, leaf, grass, dry)[k % 4], segments=3, name="grass")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	g = geo.blob(0.04, (-1.45, RAIL_Y - 0.04, RAIL_Z + 0.24), mat=glow, subdivisions=1, name="spore")
	geo.tag(g, "pulse")
	parts.append(g)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 3)

	res = pipeline.static("guardrail", parts, size=512)
	res["preview_views"] = (45.0, 135.0, 225.0, 315.0)
	return res
