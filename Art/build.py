# Builds one game asset from its script in Art/assets.
#   blender -b --factory-startup --python Art/build.py -- <asset> [--no-preview] [--no-blend]
import importlib
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import bpy

from gse import core, export, preview


def run(name, want_preview=True, want_blend=True):
	started = time.time()
	core.reset()
	core.ensure_dirs()
	module = importlib.import_module("assets." + name)
	result = module.build()

	mesh = result["mesh"]
	arm = result.get("armature")
	files = result["files"]
	path, vcount, tcount = export.write_mesh(name, mesh, arm, files, result.get("specular", 0.25), result.get("emissive", 1.0),
											 result.get("unit"))
	clips = result.get("clips") or []
	if arm is not None and clips:
		export.write_anims(name, arm, clips)

	if want_preview:
		mat = preview.textured_material(name, files)
		mesh.data.materials.clear()
		mesh.data.materials.append(mat)
		for s in result.get("sources") or []:
			s.hide_render = True
		objs = [mesh]
		frames = None
		set_frame = None
		if arm is not None and clips:
			picks = result.get("preview_frames")
			if picks:
				frames = picks
				by_name = {c["name"]: c for c in clips}
				def set_frame(pick):
					arm.data.pose_position = 'POSE'
					c = by_name[pick[0]]
					ad = arm.animation_data
					ad.action = c["action"]
					if hasattr(ad, "action_slot") and len(c["action"].slots) > 0:
						ad.action_slot = c["action"].slots[0]
					bpy.context.scene.frame_set(c["start"] + pick[1])
		# The sheet is framed on the rest pose, not on whatever pose the last exported clip ended in.
		if arm is not None:
			arm.data.pose_position = 'REST'
			bpy.context.view_layer.update()
		preview.render(name, objs, views=result.get("preview_views", (45.0, 225.0)), frames=frames,
					   height=result.get("preview_height"), set_frame=set_frame)

	if want_blend:
		core.save_blend(name)
	print("GSE_BUILT %s verts=%d tris=%d clips=%d %.1fs" % (name, vcount, tcount, len(clips), time.time() - started))


if __name__ == "__main__":
	args = core.args_after_dashes()
	names = [a for a in args if not a.startswith("--")]
	for n in names:
		run(n, "--no-preview" not in args, "--no-blend" not in args)
