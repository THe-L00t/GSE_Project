# Bakes the five ground layers the game blends by naturalisation stage: grass, moss, dirt, concrete,
# forest floor. Each is baked flat, then made to tile: a copy shifted by half a tile covers the edges,
# blended by height so the taller stone or tuft wins instead of the two ghosting over each other.
#   blender -b --factory-startup --python Art/ground.py
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy
import numpy as np

from gse import core
from gse.mat import Graph

SIZE = 512

# The painted ground's colours (Lit.fs): each layer is scaled to average out at its old colour,
# so the scene keeps its balance and the season and weather tints still read the same.
TARGETS = {
	"grass": (0.24, 0.34, 0.24),
	"moss": (0.20, 0.31, 0.20),
	"dirt": (0.30, 0.26, 0.21),
	"concrete": (0.34, 0.35, 0.33),
	"forest": (0.13, 0.22, 0.15),
}


def plane_coords(g):
	tc = g.node("ShaderNodeTexCoord")
	return tc.outputs["UV"], None


def noise(g, co, w, scale, detail=4.0, rough=0.55, distortion=0.0):
	n = g.node("ShaderNodeTexNoise", noise_dimensions='3D')
	g.link(co, n.inputs["Vector"])
	n.inputs["Scale"].default_value = scale
	n.inputs["Detail"].default_value = detail
	n.inputs["Roughness"].default_value = rough
	n.inputs["Distortion"].default_value = distortion
	return n


def voronoi(g, co, w, scale, feature='F1', randomness=1.0):
	n = g.node("ShaderNodeTexVoronoi", voronoi_dimensions='2D', feature=feature)
	g.link(co, n.inputs["Vector"])
	n.inputs["Scale"].default_value = scale
	n.inputs["Randomness"].default_value = randomness
	return n


def make_tileable(maps, height):
	# The original is continuous in the middle, the half-shifted copy across the edges. Near the edges
	# the copy takes over; in the band between, whichever surface stands higher shows.
	s = height.shape[0]
	shifted = {k: np.roll(np.roll(v, s // 2, axis=0), s // 2, axis=1) for k, v in maps.items()}
	hs = np.roll(np.roll(height, s // 2, axis=0), s // 2, axis=1)
	y, x = np.mgrid[0:s, 0:s] / float(s - 1)
	edge = np.maximum(np.abs(x - 0.5), np.abs(y - 0.5)) * 2.0
	t = np.clip((edge - 0.72) / 0.24, 0.0, 1.0)
	t = t * t * (3.0 - 2.0 * t)
	band = 4.0 * t * (1.0 - t)
	wgt = np.clip(t + (hs - height) * 1.6 * band, 0.0, 1.0)[:, :, None]
	out = {k: maps[k] * (1.0 - wgt) + shifted[k] * wgt for k in maps}
	out["height"] = height * (1.0 - wgt[:, :, 0]) + hs * wgt[:, :, 0]
	n = out["normal"] * 2.0 - 1.0
	n /= np.maximum(np.linalg.norm(n, axis=2, keepdims=True), 1e-5)
	out["normal"] = n * 0.5 + 0.5
	return out


def rgb(c):
	return (c[0], c[1], c[2], 1.0)


def grass(g, co, w):
	# Tufts: each voronoi cell a clump of blades, lit at its crown and dark where it meets the next.
	clumps = noise(g, co, w, 2.2, 5.0, 0.6).outputs["Fac"]
	tufts = voronoi(g, co, w, 38.0, 'F1')
	crown = g.ramp(tufts.outputs["Distance"], 0.0, 0.42, 1.0, 0.0)
	sep = g.node("ShaderNodeSeparateColor")
	g.link(tufts.outputs["Color"], sep.inputs[0])
	tuft_tone = sep.outputs[1]
	fine = noise(g, co, w, 90.0, 3.0, 0.7).outputs["Fac"]
	c = g.mix(g.ramp(clumps, 0.35, 0.7), rgb((0.16, 0.26, 0.14)), rgb((0.30, 0.41, 0.24)))
	c = g.mix(g.ramp(tuft_tone, 0.2, 0.9, 0.0, 0.6), c, rgb((0.36, 0.44, 0.24)))
	c = g.mix(g.math('MULTIPLY', g.math('SUBTRACT', 1.0, crown), 0.75), rgb((0.42, 0.52, 0.30)), c)
	c = g.mix(g.ramp(crown, 0.25, 0.0, 0.0, 0.8), c, rgb((0.09, 0.14, 0.08)))
	c = g.mix(g.ramp(fine, 0.6, 0.75, 0.0, 0.35), c, rgb((0.46, 0.55, 0.33)))
	soil = g.ramp(noise(g, co, w, 5.0, 3.0, 0.5).outputs["Fac"], 0.66, 0.74)
	c = g.mix(g.math('MULTIPLY', soil, 0.7), c, rgb((0.26, 0.22, 0.17)))
	flowers = g.ramp(voronoi(g, co, w, 11.0, 'F1').outputs["Distance"], 0.035, 0.0)
	c = g.mix(g.math('MULTIPLY', flowers, 0.7), c, rgb((0.82, 0.80, 0.66)))
	h = g.math('ADD', g.math('MULTIPLY', crown, 0.6), g.math('MULTIPLY', fine, 0.2))
	h = g.math('ADD', h, g.math('MULTIPLY', clumps, 0.2))
	return c, h, 0.6


def moss(g, co, w):
	cells = voronoi(g, co, w, 16.0, 'SMOOTH_F1')
	cushion = g.ramp(cells.outputs["Distance"], 0.0, 0.45, 1.0, 0.0)
	fuzz = noise(g, co, w, 110.0, 4.0, 0.75).outputs["Fac"]
	tone = noise(g, co, w, 3.0, 4.0, 0.6).outputs["Fac"]
	c = g.mix(g.ramp(tone, 0.3, 0.7), rgb((0.15, 0.26, 0.15)), rgb((0.27, 0.40, 0.21)))
	c = g.mix(g.ramp(cushion, 0.0, 0.5), rgb((0.08, 0.14, 0.09)), c)
	c = g.mix(g.ramp(cushion, 0.7, 1.0, 0.0, 0.6), c, rgb((0.40, 0.52, 0.28)))
	c = g.mix(g.ramp(fuzz, 0.58, 0.75, 0.0, 0.45), c, rgb((0.46, 0.56, 0.32)))
	# Moss sits proud: high everywhere, so it wins in the gaps between stones.
	h = g.math('ADD', g.math('MULTIPLY', cushion, 0.45), g.math('MULTIPLY', fuzz, 0.25))
	h = g.math('ADD', h, 0.3)
	return c, h, 0.9


def dirt(g, co, w):
	base = noise(g, co, w, 3.0, 6.0, 0.6).outputs["Fac"]
	grit = noise(g, co, w, 120.0, 2.0, 0.6).outputs["Fac"]
	pebbles = voronoi(g, co, w, 14.0, 'F1', 0.9)
	pebble = g.ramp(pebbles.outputs["Distance"], 0.26, 0.17)
	sep = g.node("ShaderNodeSeparateColor")
	g.link(pebbles.outputs["Color"], sep.inputs[0])
	pebble_tone = sep.outputs[0]
	c = g.mix(g.ramp(base, 0.3, 0.7), rgb((0.24, 0.20, 0.15)), rgb((0.36, 0.31, 0.24)))
	c = g.mix(g.ramp(grit, 0.4, 0.7, 0.0, 0.4), c, rgb((0.42, 0.37, 0.30)))
	stone = g.mix(pebble_tone, rgb((0.30, 0.29, 0.27)), rgb((0.52, 0.49, 0.44)))
	rim = g.ramp(pebbles.outputs["Distance"], 0.30, 0.25, 0.0, 0.6)
	c = g.mix(rim, c, rgb((0.15, 0.12, 0.09)))
	c = g.mix(pebble, c, stone)
	h = g.math('ADD', g.math('MULTIPLY', pebble, 0.75), g.math('MULTIPLY', base, 0.2))
	h = g.math('ADD', h, g.math('MULTIPLY', grit, 0.1))
	return c, h, 0.8


def concrete(g, co, w):
	slabs = voronoi(g, co, w, 3.0, 'DISTANCE_TO_EDGE', 0.6)
	crack = g.ramp(slabs.outputs["Distance"], 0.012, 0.0)
	cracks2 = voronoi(g, co, w, 7.0, 'DISTANCE_TO_EDGE', 1.0)
	crack = g.math('MAXIMUM', crack, g.math('MULTIPLY', g.ramp(cracks2.outputs["Distance"], 0.006, 0.0), 0.7))
	grain = noise(g, co, w, 60.0, 3.0, 0.7).outputs["Fac"]
	stain = noise(g, co, w, 2.0, 5.0, 0.6).outputs["Fac"]
	c = g.mix(g.ramp(stain, 0.3, 0.75), rgb((0.28, 0.29, 0.27)), rgb((0.42, 0.43, 0.40)))
	c = g.mix(g.ramp(grain, 0.55, 0.75, 0.0, 0.5), c, rgb((0.50, 0.50, 0.47)))
	c = g.mix(crack, c, rgb((0.10, 0.11, 0.10)))
	h = g.math('SUBTRACT', g.math('ADD', 0.6, g.math('MULTIPLY', grain, 0.15)), g.math('MULTIPLY', crack, 0.6))
	return c, h, 0.9


def forest(g, co, w):
	# Leaf litter over humus: scattered leaves of mixed age, only some of the ground covered, and needles.
	leaves = voronoi(g, co, w, 20.0, 'F1', 1.0)
	sep = g.node("ShaderNodeSeparateColor")
	g.link(leaves.outputs["Color"], sep.inputs[0])
	tone = sep.outputs[0]
	present = g.ramp(sep.outputs[2], 0.35, 0.45)
	leaf = g.math('MULTIPLY', g.ramp(leaves.outputs["Distance"], 0.42, 0.30), present)
	humus = noise(g, co, w, 4.0, 5.0, 0.6).outputs["Fac"]
	c = g.mix(g.ramp(humus, 0.3, 0.7), rgb((0.07, 0.10, 0.07)), rgb((0.15, 0.20, 0.12)))
	moss_patch = g.ramp(noise(g, co, w, 6.0, 3.0, 0.6).outputs["Fac"], 0.55, 0.65, 0.0, 0.8)
	c = g.mix(moss_patch, c, rgb((0.16, 0.28, 0.15)))
	leaf_col = g.mix(g.ramp(tone, 0.1, 0.9), rgb((0.22, 0.20, 0.10)), rgb((0.36, 0.26, 0.12)))
	c = g.mix(leaf, c, leaf_col)
	needles = g.ramp(noise(g, co, w, 140.0, 2.0, 0.5, 6.0).outputs["Fac"], 0.64, 0.7, 0.0, 0.5)
	c = g.mix(needles, c, rgb((0.32, 0.24, 0.14)))
	h = g.math('ADD', g.math('MULTIPLY', leaf, 0.45), g.math('MULTIPLY', needles, 0.25))
	h = g.math('ADD', h, g.math('MULTIPLY', moss_patch, 0.3))
	return c, h, 0.6


LAYERS = [("grass", grass, 2.2), ("moss", moss, 1.6), ("dirt", dirt, 1.0), ("concrete", concrete, 0.8), ("forest", forest, 1.4)]


def bake_layer(name, recipe, bump_strength):
	m = bpy.data.materials.new("ground_" + name)
	m.use_nodes = True
	for n in list(m.node_tree.nodes):
		m.node_tree.nodes.remove(n)
	g = Graph(m)
	co, w = plane_coords(g)
	color, height, _ = recipe(g, co, w)

	emit = g.node("ShaderNodeEmission")
	out = g.node("ShaderNodeOutputMaterial")
	bsdf = g.node("ShaderNodeBsdfPrincipled")
	bump = g.node("ShaderNodeBump")
	bump.inputs["Strength"].default_value = bump_strength
	bump.inputs["Distance"].default_value = 0.004
	g.link(height, bump.inputs["Height"])
	g.link(bump.outputs["Normal"], bsdf.inputs["Normal"])
	img_node = g.node("ShaderNodeTexImage")
	m.node_tree.nodes.active = img_node

	plane = bpy.data.objects.get("ground_plane")
	plane.data.materials.clear()
	plane.data.materials.append(m)
	core.select_only([plane], plane)

	results = {}
	for key in ("albedo", "height", "normal"):
		img = bpy.data.images.new("g_" + name + "_" + key, SIZE, SIZE, alpha=False, float_buffer=True)
		img.colorspace_settings.name = 'Non-Color'
		img_node.image = img
		for l in list(out.inputs["Surface"].links):
			m.node_tree.links.remove(l)
		if key == "albedo":
			g.link(color, emit.inputs["Color"])
			g.link(emit.outputs[0], out.inputs["Surface"])
			bpy.ops.object.bake(type='EMIT', margin=0, use_clear=True)
		elif key == "height":
			for l in list(emit.inputs["Color"].links):
				m.node_tree.links.remove(l)
			g.link(height, emit.inputs["Color"])
			g.link(emit.outputs[0], out.inputs["Surface"])
			bpy.ops.object.bake(type='EMIT', margin=0, use_clear=True)
		else:
			g.link(bsdf.outputs[0], out.inputs["Surface"])
			bpy.ops.object.bake(type='NORMAL', normal_space='TANGENT', margin=0, use_clear=True)
		a = np.empty(SIZE * SIZE * 4, dtype=np.float32)
		img.pixels.foreach_get(a)
		results[key] = a.reshape(SIZE, SIZE, 4)[:, :, :3].copy()

	h = results["height"][:, :, 0]
	h = (h - h.min()) / max(h.max() - h.min(), 1e-4)
	tiled = make_tileable({"albedo": results["albedo"], "normal": results["normal"]}, h)

	albedo = tiled["albedo"]
	mean = albedo.reshape(-1, 3).mean(axis=0)
	albedo = albedo * (np.array(TARGETS[name]) / np.maximum(mean, 1e-4))
	h = tiled["height"]
	height = np.stack([h, h, h], axis=2)

	save(albedo, "ground_%s_albedo" % name)
	save(tiled["normal"], "ground_%s_normal" % name)
	save(height, "ground_%s_height" % name)
	return albedo


def save(arr, name):
	img = bpy.data.images.new(name + "_out", SIZE, SIZE, alpha=False, float_buffer=False)
	img.colorspace_settings.name = 'Non-Color'
	rgba = np.ones((SIZE, SIZE, 4), dtype=np.float32)
	rgba[:, :, :3] = np.clip(arr, 0.0, 1.0)
	img.pixels.foreach_set(rgba.reshape(-1))
	path = os.path.join(core.TEX_OUT, name + ".jpg")
	img.filepath_raw = path
	img.file_format = 'JPEG'
	img.save(filepath=path, quality=88)


def contact_sheet(albedos):
	# Each layer tiled 2x2 side by side, to check the seams and the balance at a glance.
	tiles = [np.tile(a, (2, 2, 1)) for a in albedos]
	sheet = np.concatenate(tiles, axis=1)
	h, w = sheet.shape[:2]
	img = bpy.data.images.new("ground_sheet", w, h, alpha=False)
	rgba = np.ones((h, w, 4), dtype=np.float32)
	# Shown brighter, as the display values the game uses.
	rgba[:, :, :3] = np.clip(sheet, 0.0, 1.0)
	img.pixels.foreach_set(rgba.reshape(-1))
	img.filepath_raw = os.path.join(core.PREVIEW_DIR, "ground.png")
	img.file_format = 'PNG'
	img.save()


def main():
	core.reset()
	core.ensure_dirs()
	core.enable_cycles()
	scene = bpy.context.scene
	scene.cycles.samples = 1
	mesh = bpy.data.meshes.new("ground_plane")
	mesh.from_pydata([(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)], [], [(0, 1, 2, 3)])
	uv = mesh.uv_layers.new(name="UVMap")
	for i, (u, v) in enumerate(((0, 0), (1, 0), (1, 1), (0, 1))):
		uv.data[i].uv = (u, v)
	core.link(bpy.data.objects.new("ground_plane", mesh))

	albedos = []
	for name, recipe, strength in LAYERS:
		albedos.append(bake_layer(name, recipe, strength))
		print("GSE_GROUND", name)
	contact_sheet(albedos)


main()
