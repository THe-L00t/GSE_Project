import bpy
import math
import os

import numpy as np
from mathutils import Euler, Vector

from . import core


def textured_material(name, files):
	# The baked textures on a plain material, as the game will see them. Albedo is read as sRGB so the
	# raw display values the game uses come out the same on screen.
	m = bpy.data.materials.new(name + "_preview")
	m.use_nodes = True
	nt = m.node_tree
	bsdf = nt.nodes.get("Principled BSDF")
	alb = nt.nodes.new("ShaderNodeTexImage")
	alb.image = bpy.data.images.load(files["albedo"], check_existing=False)
	alb.image.colorspace_settings.name = 'sRGB'
	nrm = nt.nodes.new("ShaderNodeTexImage")
	nrm.image = bpy.data.images.load(files["normal"], check_existing=False)
	nrm.image.colorspace_settings.name = 'Non-Color'
	msk = nt.nodes.new("ShaderNodeTexImage")
	msk.image = bpy.data.images.load(files["mask"], check_existing=False)
	msk.image.colorspace_settings.name = 'Non-Color'
	nmap = nt.nodes.new("ShaderNodeNormalMap")
	sep = nt.nodes.new("ShaderNodeSeparateColor")
	mul = nt.nodes.new("ShaderNodeMix")
	mul.data_type = 'RGBA'
	mul.blend_type = 'MULTIPLY'
	mul.inputs[0].default_value = 1.0
	nt.links.new(nrm.outputs["Color"], nmap.inputs["Color"])
	nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
	nt.links.new(msk.outputs["Color"], sep.inputs[0])
	nt.links.new(alb.outputs["Color"], mul.inputs[6])
	nt.links.new(sep.outputs[0], mul.inputs[7])
	nt.links.new(mul.outputs[2], bsdf.inputs["Base Color"])
	nt.links.new(sep.outputs[1], bsdf.inputs["Roughness"])
	nt.links.new(alb.outputs["Color"], bsdf.inputs["Emission Color"])
	nt.links.new(sep.outputs[2], bsdf.inputs["Emission Strength"])
	return m


def _setup(target_height, center):
	scene = bpy.context.scene
	scene.render.engine = 'BLENDER_EEVEE'
	scene.render.resolution_x = 512
	scene.render.resolution_y = 512
	scene.render.film_transparent = False
	scene.view_settings.view_transform = 'Standard'
	scene.view_settings.look = 'None'
	world = scene.world
	world.use_nodes = True
	bg = world.node_tree.nodes.get("Background")
	bg.inputs["Color"].default_value = (0.20, 0.23, 0.22, 1.0)
	bg.inputs["Strength"].default_value = 0.8

	cam_data = bpy.data.cameras.get("gse_cam") or bpy.data.cameras.new("gse_cam")
	cam_data.type = 'ORTHO'
	cam_data.ortho_scale = target_height
	cam = bpy.data.objects.get("gse_cam") or core.link(bpy.data.objects.new("gse_cam", cam_data))
	scene.camera = cam

	sun_data = bpy.data.lights.get("gse_sun") or bpy.data.lights.new("gse_sun", 'SUN')
	sun_data.energy = 3.2
	sun_data.color = (1.0, 0.95, 0.86)
	sun_data.angle = math.radians(8.0)
	sun = bpy.data.objects.get("gse_sun") or core.link(bpy.data.objects.new("gse_sun", sun_data))
	sun.rotation_euler = Euler((math.radians(50.0), 0.0, math.radians(35.0)), 'XYZ')

	ground = bpy.data.objects.get("gse_ground")
	if ground is None:
		mesh = bpy.data.meshes.new("gse_ground")
		s = 30.0
		mesh.from_pydata([(-s, -s, 0), (s, -s, 0), (s, s, 0), (-s, s, 0)], [], [(0, 1, 2, 3)])
		ground = core.link(bpy.data.objects.new("gse_ground", mesh))
		gm = bpy.data.materials.new("gse_ground")
		gm.use_nodes = True
		gm.node_tree.nodes.get("Principled BSDF").inputs["Base Color"].default_value = (0.05, 0.07, 0.05, 1.0)
		gm.node_tree.nodes.get("Principled BSDF").inputs["Roughness"].default_value = 1.0
		mesh.materials.append(gm)
	return scene, cam


def _aim(cam, center, yaw_deg, elevation_deg=30.0, distance=60.0):
	# The game camera: yaw 45, pitch 30 in game space, where the model faces +Z. In Blender the model
	# faces -Y, so the same view comes from the -Y side.
	yaw = math.radians(yaw_deg)
	el = math.radians(elevation_deg)
	game_dir = Vector((math.cos(el) * math.sin(yaw), math.sin(el), math.cos(el) * math.cos(yaw)))
	d = Vector((game_dir.x, -game_dir.z, game_dir.y))
	cam.location = Vector(center) + d * distance
	cam.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
	cam.data.clip_end = distance * 3.0


def render(name, objs, views=(45.0, 225.0), frames=None, height=None, set_frame=None):
	# Renders the asset from the game's angle (and the back), tiled side by side into previews/<name>.png.
	core.ensure_dirs()
	lo = Vector((1e9, 1e9, 1e9))
	hi = Vector((-1e9, -1e9, -1e9))
	for o in objs:
		for c in o.bound_box:
			w = o.matrix_world @ Vector(c)
			lo = Vector((min(lo.x, w.x), min(lo.y, w.y), min(lo.z, w.z)))
			hi = Vector((max(hi.x, w.x), max(hi.y, w.y), max(hi.z, w.z)))
	size = hi - lo
	center = (lo + hi) * 0.5
	extent = height or max(size.x, size.y, size.z) * 1.35 + 0.3
	scene, cam = _setup(extent, center)

	shots = []
	frame_list = frames or [None]
	for f in frame_list:
		if f is not None and set_frame is not None:
			set_frame(f)
		for yaw in views:
			_aim(cam, center, yaw)
			path = os.path.join(core.PREVIEW_DIR, "_shot.png")
			scene.render.filepath = path
			scene.render.image_settings.file_format = 'PNG'
			bpy.ops.render.render(write_still=True)
			img = bpy.data.images.load(path, check_existing=False)
			shots.append(np.array(img.pixels[:], dtype=np.float32).reshape(512, 512, 4))
			bpy.data.images.remove(img)

	cols = len(views) * (1 if frames is None else min(len(frame_list), 4))
	rows = int(math.ceil(len(shots) / float(cols)))
	sheet = np.zeros((rows * 512, cols * 512, 4), dtype=np.float32)
	for i, s in enumerate(shots):
		r = rows - 1 - i // cols
		c = i % cols
		sheet[r * 512:(r + 1) * 512, c * 512:(c + 1) * 512] = s
	out = bpy.data.images.new(name + "_sheet", cols * 512, rows * 512, alpha=True)
	out.pixels.foreach_set(sheet.reshape(-1))
	path = os.path.join(core.PREVIEW_DIR, name + ".png")
	out.filepath_raw = path
	out.file_format = 'PNG'
	out.save()
	try:
		os.remove(os.path.join(core.PREVIEW_DIR, "_shot.png"))
	except OSError:
		pass
	return path
