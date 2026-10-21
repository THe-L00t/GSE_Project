import bpy

# Display values, read as-is by the game (no gamma step): the same space as the old recipe colours.
PALETTE = {
	"moss": (0.24, 0.38, 0.24),
	"moss_dark": (0.17, 0.29, 0.19),
	"moss_light": (0.34, 0.47, 0.28),
	"leaf": (0.18, 0.30, 0.19),
	"leaf_dark": (0.12, 0.21, 0.15),
	"pine": (0.13, 0.24, 0.18),
	"ginkgo": (0.80, 0.68, 0.26),
	"bark": (0.24, 0.20, 0.17),
	"wood": (0.33, 0.27, 0.21),
	"wood_dark": (0.24, 0.20, 0.16),
	"wood_grey": (0.36, 0.34, 0.30),
	"plaster": (0.50, 0.47, 0.42),
	"plaster_grey": (0.42, 0.42, 0.40),
	"stone": (0.38, 0.38, 0.36),
	"stone_dark": (0.29, 0.29, 0.28),
	"concrete": (0.40, 0.40, 0.38),
	"slate": (0.30, 0.31, 0.31),
	"giwa": (0.22, 0.23, 0.25),
	"tin": (0.36, 0.31, 0.27),
	"rust": (0.45, 0.28, 0.18),
	"rust_dark": (0.32, 0.20, 0.14),
	"metal": (0.20, 0.20, 0.19),
	"paint_blue": (0.24, 0.36, 0.44),
	"paint_green": (0.26, 0.38, 0.32),
	"paint_red": (0.48, 0.20, 0.17),
	"paint_cream": (0.62, 0.58, 0.48),
	"glass": (0.10, 0.14, 0.16),
	"rubber": (0.11, 0.11, 0.10),
	"soil": (0.25, 0.20, 0.15),
	"onggi": (0.30, 0.18, 0.11),
	"paper": (0.84, 0.81, 0.72),
	"skin": (0.56, 0.47, 0.40),
	"hair_dark": (0.13, 0.11, 0.10),
	"hair_white": (0.80, 0.79, 0.76),
	"coat": (0.30, 0.37, 0.37),
	"scarf": (0.44, 0.54, 0.47),
	"trousers": (0.23, 0.24, 0.25),
	"boots": (0.17, 0.14, 0.12),
	"canvas": (0.37, 0.32, 0.25),
	"cardigan": (0.52, 0.43, 0.35),
	"apron": (0.63, 0.60, 0.53),
	"monpe": (0.33, 0.27, 0.33),
	"cloth_grey": (0.33, 0.33, 0.32),
	"fur_boar": (0.33, 0.26, 0.21),
	"fur_deer": (0.43, 0.36, 0.29),
	"antler": (0.62, 0.56, 0.45),
	"bone": (0.86, 0.83, 0.74),
	"mite_shell": (0.35, 0.41, 0.30),
	"spore": (0.55, 0.95, 0.80),
	"lamp": (0.98, 0.84, 0.52),
	"berry_red": (0.74, 0.18, 0.18),
	"berry_pale": (0.86, 0.85, 0.78),
}

GLOW_SPORE = PALETTE["spore"]
GLOW_LAMP = PALETTE["lamp"]


def col(name_or_rgb, scale=1.0):
	rgb = PALETTE[name_or_rgb] if isinstance(name_or_rgb, str) else name_or_rgb
	return (rgb[0] * scale, rgb[1] * scale, rgb[2] * scale, 1.0)


class Graph:
	def __init__(self, material):
		self.mat = material
		self.nodes = material.node_tree.nodes
		self.links = material.node_tree.links
		self.x = -1400

	def node(self, kind, **props):
		n = self.nodes.new(kind)
		n.location = (self.x, 0)
		self.x += 160
		for k, v in props.items():
			if k.startswith("in_"):
				n.inputs[k[3:].replace("_", " ")].default_value = v
			else:
				setattr(n, k, v)
		return n

	def link(self, a, b):
		self.links.new(a, b)

	def math(self, op, a, b=None, clamp=False):
		n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
		self._feed(n.inputs[0], a)
		if b is not None:
			self._feed(n.inputs[1], b)
		return n.outputs[0]

	def mix(self, fac, a, b, blend='MIX'):
		n = self.node("ShaderNodeMix", data_type='RGBA', blend_type=blend)
		self._feed(n.inputs[0], fac)
		self._feed(n.inputs[6], a)
		self._feed(n.inputs[7], b)
		return n.outputs[2]

	def ramp(self, value, lo, hi, a=0.0, b=1.0):
		n = self.node("ShaderNodeMapRange", clamp=True)
		self._feed(n.inputs[0], value)
		n.inputs[1].default_value = lo
		n.inputs[2].default_value = hi
		n.inputs[3].default_value = a
		n.inputs[4].default_value = b
		return n.outputs[0]

	def _feed(self, socket, value):
		if hasattr(value, "is_output") or hasattr(value, "links"):
			self.link(value, socket)
		else:
			socket.default_value = value


_COORD_CACHE = {}


def _coords(g):
	tc = g.node("ShaderNodeTexCoord")
	return tc.outputs["Object"]


def _noise(g, co, scale, detail=3.0, rough=0.55, w=None, dims='3D'):
	n = g.node("ShaderNodeTexNoise", noise_dimensions=dims)
	g.link(co, n.inputs["Vector"])
	n.inputs["Scale"].default_value = scale
	n.inputs["Detail"].default_value = detail
	n.inputs["Roughness"].default_value = rough
	return n.outputs["Fac"]


def _stretch(g, co, sx, sy, sz):
	m = g.node("ShaderNodeVectorMath", operation='MULTIPLY')
	g.link(co, m.inputs[0])
	m.inputs[1].default_value = (sx, sy, sz)
	return m.outputs[0]


def _wave(g, co, scale, axis='X', distortion=2.0, kind='BANDS', profile='SIN', detail=2.0):
	n = g.node("ShaderNodeTexWave", wave_type=kind, bands_direction=axis, wave_profile=profile)
	g.link(co, n.inputs["Vector"])
	n.inputs["Scale"].default_value = scale
	n.inputs["Distortion"].default_value = distortion
	n.inputs["Detail"].default_value = detail
	return n.outputs["Fac"]


def _voronoi(g, co, scale, feature='F1', output="Distance", randomness=1.0):
	n = g.node("ShaderNodeTexVoronoi", feature=feature)
	g.link(co, n.inputs["Vector"])
	n.inputs["Scale"].default_value = scale
	n.inputs["Randomness"].default_value = randomness
	return n.outputs[output]


def make(name, color, rough=0.85, pattern="plain", var=0.12, var_scale=3.0, grime=0.35, edge=0.18,
		 bump=0.25, emit=0.0, accent=None, metallic=0.0, axis='Z', amount=1.0):
	# One stylised painted-model material: base colour with soft variation, grime in the crevices,
	# a dry-brushed highlight on edges, a bump from the pattern, and optional glow.
	existing = bpy.data.materials.get(name)
	if existing is not None:
		return existing
	m = bpy.data.materials.new(name)
	m.use_nodes = True
	nodes = m.node_tree.nodes
	for n in list(nodes):
		nodes.remove(n)
	g = Graph(m)
	co = _coords(g)

	base = col(color)
	dark = (base[0] * (1.0 - var), base[1] * (1.0 - var), base[2] * (1.0 - var), 1.0)
	light = (base[0] * (1.0 + var), base[1] * (1.0 + var), base[2] * (1.0 + var), 1.0)
	n_var = _noise(g, co, var_scale, 4.0, 0.6)
	c = g.mix(g.ramp(n_var, 0.3, 0.7), dark, light)
	height = g.math('MULTIPLY', _noise(g, co, var_scale * 6.0, 5.0, 0.6), 0.25)
	rough_out = rough
	acc = col(accent) if accent is not None else None

	if pattern == "wood":
		grain = _wave(g, _stretch(g, co, 1.0, 1.0, 0.12) if axis == 'Z' else _stretch(g, co, 0.12, 1.0, 1.0), 18.0, 'Y', 6.0, 'BANDS', 'SAW', 3.0)
		c = g.mix(g.math('MULTIPLY', grain, 0.35), c, (base[0] * 0.62, base[1] * 0.60, base[2] * 0.58, 1.0))
		height = g.math('ADD', height, g.math('MULTIPLY', grain, 0.6))
	elif pattern == "planks":
		axis_scale = _stretch(g, co, 0.0, 0.0, 1.0) if axis == 'Z' else _stretch(g, co, 1.0, 0.0, 0.0)
		seams = _wave(g, axis_scale, 5.0, 'X', 0.0, 'BANDS', 'SAW', 0.0)
		gap = g.ramp(seams, 0.0, 0.06, 1.0, 0.0)
		grain = _wave(g, _stretch(g, co, 1.0, 1.0, 0.15), 22.0, 'Y', 7.0, 'BANDS', 'SAW', 3.0)
		c = g.mix(g.math('MULTIPLY', grain, 0.3), c, (base[0] * 0.65, base[1] * 0.62, base[2] * 0.6, 1.0))
		c = g.mix(gap, c, (base[0] * 0.35, base[1] * 0.33, base[2] * 0.3, 1.0))
		height = g.math('ADD', g.math('MULTIPLY', grain, 0.4), g.math('MULTIPLY', gap, -1.0))
	elif pattern == "rust":
		lo = 0.62 - 0.14 * amount
		spots = g.ramp(_noise(g, co, 2.5, 8.0, 0.65), lo, lo + 0.12)
		rc = acc or col("rust")
		c = g.mix(spots, c, rc)
		height = g.math('ADD', height, g.math('MULTIPLY', spots, 0.5))
		rough_out = g.math('ADD', rough, g.math('MULTIPLY', spots, 0.15))
	elif pattern == "plaster":
		streak = _noise(g, _stretch(g, co, 3.0, 3.0, 0.4), 2.0, 3.0, 0.5)
		c = g.mix(g.ramp(streak, 0.45, 0.75, 0.0, 0.45), c, (base[0] * 0.72, base[1] * 0.72, base[2] * 0.66, 1.0))
		height = g.math('ADD', height, g.math('MULTIPLY', _noise(g, co, 40.0, 3.0, 0.7), 0.4))
	elif pattern == "slate":
		ridges = _wave(g, co, 14.0, 'X', 0.0, 'BANDS', 'SIN', 0.0)
		height = g.math('ADD', g.math('MULTIPLY', ridges, 1.0), height)
		c = g.mix(g.math('MULTIPLY', ridges, 0.25), (base[0] * 0.85, base[1] * 0.85, base[2] * 0.85, 1.0), c)
	elif pattern == "giwa":
		ridges = _wave(g, co, 9.0, 'X', 0.0, 'BANDS', 'SIN', 0.0)
		rows = _wave(g, co, 7.0, 'Y', 0.0, 'BANDS', 'SAW', 0.0)
		height = g.math('ADD', g.math('MULTIPLY', ridges, 1.2), g.math('MULTIPLY', rows, 0.5))
		c = g.mix(g.math('MULTIPLY', ridges, 0.3), (base[0] * 0.8, base[1] * 0.8, base[2] * 0.8, 1.0), c)
	elif pattern == "stone":
		cells = _voronoi(g, co, 3.0, 'DISTANCE_TO_EDGE', "Distance")
		crack = g.ramp(cells, 0.0, 0.06, 1.0, 0.0)
		c = g.mix(g.math('MULTIPLY', crack, 0.6), c, (base[0] * 0.55, base[1] * 0.55, base[2] * 0.55, 1.0))
		height = g.math('ADD', g.math('MULTIPLY', crack, -0.8), g.math('MULTIPLY', _noise(g, co, 12.0, 6.0, 0.7), 0.6))
	elif pattern == "concrete":
		pores = g.ramp(_voronoi(g, co, 30.0), 0.0, 0.12, 1.0, 0.0)
		stain = g.ramp(_noise(g, _stretch(g, co, 2.0, 2.0, 0.5), 1.5, 4.0, 0.6), 0.5, 0.8, 0.0, 0.5)
		c = g.mix(stain, c, (base[0] * 0.7, base[1] * 0.72, base[2] * 0.66, 1.0))
		height = g.math('ADD', height, g.math('MULTIPLY', pores, -0.4))
	elif pattern == "cloth":
		w1 = _wave(g, co, 90.0, 'X', 0.0, 'BANDS', 'SIN', 0.0)
		w2 = _wave(g, co, 90.0, 'Z', 0.0, 'BANDS', 'SIN', 0.0)
		weave = g.math('MULTIPLY', g.math('ADD', w1, w2), 0.5)
		folds = _noise(g, _stretch(g, co, 1.0, 1.0, 0.35), 4.0, 2.0, 0.5)
		c = g.mix(g.ramp(folds, 0.35, 0.7, 0.0, 0.5), (base[0] * 0.8, base[1] * 0.8, base[2] * 0.8, 1.0), c)
		height = g.math('ADD', g.math('MULTIPLY', weave, 0.25), g.math('MULTIPLY', folds, 0.9))
	elif pattern == "knit":
		rows = _wave(g, co, 60.0, 'Z', 1.0, 'BANDS', 'SIN', 1.0)
		height = g.math('ADD', g.math('MULTIPLY', rows, 0.6), height)
		c = g.mix(g.math('MULTIPLY', rows, 0.25), (base[0] * 0.82, base[1] * 0.82, base[2] * 0.82, 1.0), c)
	elif pattern == "hair":
		strands = _wave(g, _stretch(g, co, 1.0, 1.0, 0.2), 60.0, 'X', 4.0, 'BANDS', 'SIN', 2.0)
		c = g.mix(g.math('MULTIPLY', strands, 0.3), c, (base[0] * 1.3 + 0.03, base[1] * 1.3 + 0.03, base[2] * 1.3 + 0.03, 1.0))
		height = g.math('MULTIPLY', strands, 0.6)
	elif pattern == "bark":
		ridges = _voronoi(g, _stretch(g, co, 1.0, 1.0, 0.18), 7.0, 'DISTANCE_TO_EDGE', "Distance")
		furrow = g.ramp(ridges, 0.0, 0.08, 1.0, 0.0)
		c = g.mix(g.math('MULTIPLY', furrow, 0.7), c, (base[0] * 0.5, base[1] * 0.5, base[2] * 0.48, 1.0))
		height = g.math('ADD', g.math('MULTIPLY', furrow, -1.0), g.math('MULTIPLY', _noise(g, co, 15.0, 4.0, 0.6), 0.4))
	elif pattern == "leaf":
		cells = _voronoi(g, co, 9.0, 'F1', "Distance")
		cell_col = _voronoi(g, co, 9.0, 'F1', "Color")
		tone = g.node("ShaderNodeSeparateColor")
		g.link(cell_col, tone.inputs[0])
		hi = acc or (base[0] * 1.35 + 0.02, base[1] * 1.3 + 0.03, base[2] * 1.15, 1.0)
		c = g.mix(g.ramp(tone.outputs[0], 0.2, 0.9, 0.0, 0.8), (base[0] * 0.8, base[1] * 0.8, base[2] * 0.8, 1.0), hi)
		height = g.math('ADD', g.math('MULTIPLY', g.ramp(cells, 0.0, 0.5, 1.0, 0.0), 0.9), height)
	elif pattern == "moss":
		fuzz = _noise(g, co, 45.0, 6.0, 0.75)
		clump = g.ramp(_noise(g, co, 5.0, 3.0, 0.6), 0.35, 0.7)
		hi = acc or col("moss_light")
		c = g.mix(clump, (base[0] * 0.75, base[1] * 0.78, base[2] * 0.75, 1.0), hi)
		height = g.math('ADD', g.math('MULTIPLY', fuzz, 0.7), g.math('MULTIPLY', clump, 0.6))
	elif pattern == "fur":
		strands = _noise(g, _stretch(g, co, 6.0, 6.0, 0.5), 30.0, 4.0, 0.7)
		c = g.mix(g.ramp(strands, 0.3, 0.7, 0.0, 0.5), (base[0] * 0.75, base[1] * 0.75, base[2] * 0.75, 1.0), c)
		height = g.math('MULTIPLY', strands, 0.8)
	elif pattern == "onggi":
		drip = _noise(g, _stretch(g, co, 4.0, 4.0, 0.3), 3.0, 5.0, 0.6)
		c = g.mix(g.ramp(drip, 0.4, 0.75, 0.0, 0.6), c, (base[0] * 0.6, base[1] * 0.55, base[2] * 0.5, 1.0))
		height = g.math('MULTIPLY', drip, 0.2)
	elif pattern == "paper":
		fiber = _noise(g, _stretch(g, co, 1.0, 8.0, 1.0), 60.0, 4.0, 0.6)
		height = g.math('MULTIPLY', fiber, 0.3)
	elif pattern == "skin":
		height = g.math('MULTIPLY', _noise(g, co, 30.0, 2.0, 0.5), 0.15)
	elif pattern == "shell":
		plates = _voronoi(g, co, 6.0, 'DISTANCE_TO_EDGE', "Distance")
		seam = g.ramp(plates, 0.0, 0.05, 1.0, 0.0)
		c = g.mix(g.math('MULTIPLY', seam, 0.6), c, (base[0] * 0.55, base[1] * 0.55, base[2] * 0.5, 1.0))
		height = g.math('ADD', g.math('MULTIPLY', seam, -0.8), height)

	# Crevices hold grime, edges catch a dry-brushed highlight: the painted-model look.
	ao = g.node("ShaderNodeAmbientOcclusion", samples=8, only_local=True)
	ao.inputs["Distance"].default_value = 0.25
	c = g.mix(g.ramp(ao.outputs["AO"], 0.2, 1.0, grime, 0.0), c, (base[0] * 0.45, base[1] * 0.45, base[2] * 0.42, 1.0))

	if edge > 0.0:
		bev = g.node("ShaderNodeBevel", samples=8)
		bev.inputs["Radius"].default_value = 0.03
		geo_n = g.node("ShaderNodeNewGeometry")
		dot = g.node("ShaderNodeVectorMath", operation='DOT_PRODUCT')
		g.link(bev.outputs["Normal"], dot.inputs[0])
		g.link(geo_n.outputs["Normal"], dot.inputs[1])
		edge_mask = g.ramp(dot.outputs["Value"], 0.98, 0.85, 0.0, edge)
		c = g.mix(edge_mask, c, (min(1.0, base[0] * 1.6 + 0.06), min(1.0, base[1] * 1.6 + 0.06), min(1.0, base[2] * 1.6 + 0.06), 1.0))

	bsdf = g.node("ShaderNodeBsdfPrincipled")
	g.link(c, bsdf.inputs["Base Color"])
	g._feed(bsdf.inputs["Roughness"], rough_out)
	bsdf.inputs["Metallic"].default_value = metallic
	if bump > 0.0:
		bn = g.node("ShaderNodeBump")
		bn.inputs["Strength"].default_value = bump
		bn.inputs["Distance"].default_value = 0.02
		g.link(height, bn.inputs["Height"])
		g.link(bn.outputs["Normal"], bsdf.inputs["Normal"])
	if emit > 0.0:
		bsdf.inputs["Emission Color"].default_value = (1.0, 1.0, 1.0, 1.0)
		bsdf.inputs["Emission Strength"].default_value = emit
	out = g.node("ShaderNodeOutputMaterial")
	g.link(bsdf.outputs["BSDF"], out.inputs["Surface"])
	m["gse_emit"] = emit
	return m


def glow(name, color, strength=1.0, rough=0.4):
	return make(name, color, rough=rough, pattern="plain", var=0.05, grime=0.0, edge=0.0, bump=0.05, emit=strength)


def get(name):
	# Shorthand presets named after their use.
	presets = {
		"moss": dict(color="moss", pattern="moss", rough=0.95, bump=0.5),
		"moss_dark": dict(color="moss_dark", pattern="moss", rough=0.95, bump=0.5),
		"leaf": dict(color="leaf", pattern="leaf", rough=0.8, bump=0.45),
		"leaf_dark": dict(color="leaf_dark", pattern="leaf", rough=0.8, bump=0.45),
		"pine": dict(color="pine", pattern="leaf", rough=0.85, bump=0.5),
		"ginkgo": dict(color="ginkgo", pattern="leaf", rough=0.75, bump=0.4),
		"bark": dict(color="bark", pattern="bark", rough=0.95, bump=0.7),
		"wood": dict(color="wood", pattern="wood", rough=0.85, bump=0.35),
		"wood_dark": dict(color="wood_dark", pattern="wood", rough=0.85, bump=0.35),
		"wood_grey": dict(color="wood_grey", pattern="wood", rough=0.9, bump=0.4),
		"planks": dict(color="wood", pattern="planks", rough=0.88, bump=0.45),
		"plaster": dict(color="plaster", pattern="plaster", rough=0.92, bump=0.3),
		"plaster_grey": dict(color="plaster_grey", pattern="plaster", rough=0.92, bump=0.3),
		"stone": dict(color="stone", pattern="stone", rough=0.92, bump=0.6),
		"stone_dark": dict(color="stone_dark", pattern="stone", rough=0.92, bump=0.6),
		"concrete": dict(color="concrete", pattern="concrete", rough=0.93, bump=0.35),
		"slate": dict(color="slate", pattern="slate", rough=0.8, bump=0.6),
		"giwa": dict(color="giwa", pattern="giwa", rough=0.7, bump=0.8),
		"tin": dict(color="tin", pattern="rust", rough=0.75, bump=0.4),
		"rust": dict(color="rust", pattern="rust", rough=0.9, bump=0.5),
		"metal": dict(color="metal", pattern="rust", rough=0.6, bump=0.3, accent="rust_dark"),
		"paint_blue": dict(color="paint_blue", pattern="rust", rough=0.7, bump=0.35, amount=0.55),
		"paint_green": dict(color="paint_green", pattern="rust", rough=0.7, bump=0.35, amount=0.55),
		"paint_red": dict(color="paint_red", pattern="rust", rough=0.7, bump=0.35, amount=0.4),
		"paint_cream": dict(color="paint_cream", pattern="rust", rough=0.7, bump=0.35, amount=0.55),
		"glass": dict(color="glass", pattern="plain", rough=0.15, grime=0.1, edge=0.05, bump=0.05),
		"rubber": dict(color="rubber", pattern="plain", rough=0.95, bump=0.2),
		"soil": dict(color="soil", pattern="stone", rough=0.98, bump=0.6),
		"onggi": dict(color="onggi", pattern="onggi", rough=0.35, bump=0.15, edge=0.25),
		"paper": dict(color="paper", pattern="paper", rough=0.95, bump=0.1, edge=0.05),
		"skin": dict(color="skin", pattern="skin", rough=0.75, bump=0.1, grime=0.25, edge=0.08),
		"hair_dark": dict(color="hair_dark", pattern="hair", rough=0.7, bump=0.4),
		"hair_white": dict(color="hair_white", pattern="hair", rough=0.7, bump=0.4),
		"coat": dict(color="coat", pattern="cloth", rough=0.9, bump=0.45),
		"scarf": dict(color="scarf", pattern="knit", rough=0.95, bump=0.5),
		"trousers": dict(color="trousers", pattern="cloth", rough=0.9, bump=0.45),
		"boots": dict(color="boots", pattern="plain", rough=0.7, bump=0.25),
		"canvas": dict(color="canvas", pattern="cloth", rough=0.92, bump=0.5),
		"cardigan": dict(color="cardigan", pattern="knit", rough=0.95, bump=0.5),
		"apron": dict(color="apron", pattern="cloth", rough=0.9, bump=0.35),
		"monpe": dict(color="monpe", pattern="cloth", rough=0.9, bump=0.4),
		"cloth_grey": dict(color="cloth_grey", pattern="cloth", rough=0.92, bump=0.45),
		"fur_boar": dict(color="fur_boar", pattern="fur", rough=0.95, bump=0.6),
		"fur_deer": dict(color="fur_deer", pattern="fur", rough=0.95, bump=0.5),
		"antler": dict(color="antler", pattern="wood", rough=0.8, bump=0.4),
		"bone": dict(color="bone", pattern="plain", rough=0.6, bump=0.15),
		"mite_shell": dict(color="mite_shell", pattern="shell", rough=0.55, bump=0.5),
		"berry_red": dict(color="berry_red", pattern="plain", rough=0.35, bump=0.1, grime=0.15),
		"berry_pale": dict(color="berry_pale", pattern="plain", rough=0.35, bump=0.1, grime=0.15),
	}
	if name == "spore_glow":
		return glow(name, "spore", 1.0)
	if name == "lamp_glow":
		return glow(name, "lamp", 1.0)
	if name in presets:
		return make(name, **presets[name])
	raise KeyError(name)
