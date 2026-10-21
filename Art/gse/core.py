import bpy
import math
import os
import sys

from mathutils import Matrix, Vector

ART_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(ART_DIR)
GAME_DIR = os.path.join(ROOT_DIR, "SimpleGame")
MODEL_OUT = os.path.join(GAME_DIR, "Assets", "Models")
TEX_OUT = os.path.join(GAME_DIR, "Assets", "Textures")
PREVIEW_DIR = os.path.join(ART_DIR, "previews")
BLEND_DIR = os.path.join(ART_DIR, "blend")

# Blender is Z-up with the front of a model facing -Y; the game is Y-up with the front facing +Z.
TO_GAME = Matrix(((1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, -1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))
TO_BLENDER = TO_GAME.inverted()


def ensure_dirs():
	for d in (MODEL_OUT, TEX_OUT, PREVIEW_DIR, BLEND_DIR):
		os.makedirs(d, exist_ok=True)


def reset():
	for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.images,
				 bpy.data.armatures, bpy.data.actions, bpy.data.cameras, bpy.data.lights,
				 bpy.data.node_groups, bpy.data.curves, bpy.data.textures):
		for item in list(coll):
			coll.remove(item)
	scene = bpy.context.scene
	scene.unit_settings.system = 'METRIC'
	scene.unit_settings.scale_length = 1.0
	scene.render.fps = 30
	if scene.world is None:
		scene.world = bpy.data.worlds.new("World")


def link(obj):
	bpy.context.scene.collection.objects.link(obj)
	return obj


def select_only(objs, active=None):
	for o in bpy.data.objects:
		if o is not None and o.select_get():
			o.select_set(False)
	for o in objs:
		o.select_set(True)
	bpy.context.view_layer.objects.active = active if active is not None else (objs[0] if objs else None)


def set_mode(obj, mode):
	if bpy.context.object is not None and bpy.context.object.mode != 'OBJECT':
		bpy.ops.object.mode_set(mode='OBJECT')
	select_only([obj])
	if mode != 'OBJECT':
		bpy.ops.object.mode_set(mode=mode)


def deg(v):
	return math.radians(v)


def args_after_dashes():
	if "--" in sys.argv:
		return sys.argv[sys.argv.index("--") + 1:]
	return []


def use_gpu():
	prefs = bpy.context.preferences.addons['cycles'].preferences
	for kind in ('OPTIX', 'CUDA'):
		try:
			prefs.compute_device_type = kind
			prefs.get_devices()
			found = False
			for d in prefs.devices:
				d.use = d.type == kind
				found = found or d.use
			if found:
				bpy.context.scene.cycles.device = 'GPU'
				return kind
		except Exception:
			continue
	bpy.context.scene.cycles.device = 'CPU'
	return 'CPU'


def enable_cycles():
	import addon_utils
	addon_utils.enable('cycles', default_set=True)
	bpy.context.scene.render.engine = 'CYCLES'
	return use_gpu()


def save_blend(name):
	ensure_dirs()
	bpy.ops.wm.save_as_mainfile(filepath=os.path.join(BLEND_DIR, name + ".blend"), compress=True)
