import bpy
import math
import os

import numpy as np

from . import core


def unwrap(obj, margin=0.006, angle=60.0):
	if not obj.data.uv_layers:
		obj.data.uv_layers.new(name="UVMap")
	core.set_mode(obj, 'EDIT')
	bpy.ops.mesh.select_all(action='SELECT')
	bpy.ops.uv.smart_project(angle_limit=math.radians(angle), island_margin=margin, area_weight=0.0,
							 correct_aspect=True, scale_to_bounds=False)
	bpy.ops.uv.pack_islands(rotate=True, margin=margin)
	bpy.ops.object.mode_set(mode='OBJECT')


def _image(name, size):
	img = bpy.data.images.get(name)
	if img is not None:
		bpy.data.images.remove(img)
	img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=True)
	img.colorspace_settings.name = 'Non-Color'
	return img


def _target_nodes(obj):
	nodes = []
	if not obj.data.materials:
		m = bpy.data.materials.new(obj.name + "_bake")
		m.use_nodes = True
		obj.data.materials.append(m)
	for m in obj.data.materials:
		tree = m.node_tree
		node = tree.nodes.get("gse_bake")
		if node is None:
			node = tree.nodes.new("ShaderNodeTexImage")
			node.name = "gse_bake"
		tree.nodes.active = node
		node.select = True
		nodes.append(node)
	return nodes


def _pixels(img):
	w, h = img.size
	a = np.empty(w * h * 4, dtype=np.float32)
	img.pixels.foreach_get(a)
	return a.reshape(h, w, 4)


def _save_jpg(arr, name, size, quality=88):
	img = bpy.data.images.new(name + "_out", size, size, alpha=False, float_buffer=False)
	img.colorspace_settings.name = 'Non-Color'
	rgba = np.ones((size, size, 4), dtype=np.float32)
	rgba[:, :, :3] = np.clip(arr, 0.0, 1.0)
	flat = rgba.reshape(-1)
	img.pixels.foreach_set(flat)
	path = os.path.join(core.TEX_OUT, name + ".jpg")
	img.filepath_raw = path
	img.file_format = 'JPEG'
	img.save(filepath=path, quality=quality)
	return path


def bake(target, sources=None, size=512, name=None, samples=24, cage=0.04, ray=0.12, ao_distance=0.35):
	# Bakes colour, roughness, glow, tangent normals (the material bumps included) and occlusion
	# into three game textures: albedo, normal, and a mask with occlusion, roughness and glow.
	core.ensure_dirs()
	name = name or target.name
	core.enable_cycles()
	scene = bpy.context.scene
	scene.cycles.samples = samples
	scene.cycles.use_denoising = False
	scene.render.bake.margin = max(4, size // 64)
	scene.render.bake.margin_type = 'EXTEND'
	scene.world.light_settings.distance = ao_distance

	nodes = _target_nodes(target)
	selected = sources is not None and len(sources) > 0
	if selected:
		core.select_only(list(sources) + [target], target)
	else:
		core.select_only([target], target)

	results = {}
	passes = (("albedo", 'DIFFUSE'), ("rough", 'ROUGHNESS'), ("emit", 'EMIT'), ("normal", 'NORMAL'), ("ao", 'AO'))
	for key, kind in passes:
		img = _image(name + "_" + key, size)
		for n in nodes:
			n.image = img
		kwargs = dict(type=kind, margin=scene.render.bake.margin, use_clear=True, target='IMAGE_TEXTURES')
		if kind == 'DIFFUSE':
			kwargs["pass_filter"] = {'COLOR'}
		if kind == 'NORMAL':
			kwargs["normal_space"] = 'TANGENT'
		if selected:
			kwargs["use_selected_to_active"] = True
			kwargs["cage_extrusion"] = cage
			kwargs["max_ray_distance"] = ray
		bpy.ops.object.bake(**kwargs)
		results[key] = _pixels(img)

	albedo = results["albedo"][:, :, :3]
	normal = results["normal"][:, :, :3]
	ao = results["ao"][:, :, 0]
	rough = results["rough"][:, :, 0]
	emit = results["emit"][:, :, :3].max(axis=2)
	# Occlusion is kept soft: the game adds its own contact shadows and SSAO on top.
	ao = 0.35 + 0.65 * ao
	mask = np.stack([ao, rough, np.clip(emit, 0.0, 1.0)], axis=2)

	files = {
		"albedo": _save_jpg(albedo, name + "_albedo", size, 90),
		"normal": _save_jpg(normal, name + "_normal", size, 92),
		"mask": _save_jpg(mask, name + "_mask", size, 88),
	}
	for n in nodes:
		n.image = None
	return files
