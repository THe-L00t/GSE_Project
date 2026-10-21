import bpy

from . import bake, core, geo, rig


def static(name, parts, size=512, angle=60.0):
	# Hard-surface props: the parts are the final mesh; materials bake straight off them.
	low = geo.join(parts, name)
	low = geo.triangulate(low)
	bake.unwrap(low, angle=angle)
	files = bake.bake(low, None, size, name)
	return dict(mesh=low, files=files, sources=None)


def organic(name, parts, voxel=0.02, tris=4000, size=512, smooth=2, groups=None, keep_parts=False):
	# Creatures and soft shapes: the parts fuse into one remeshed skin, then the parts' materials,
	# bumps and tags (bones, shader flags, moss) are carried onto it.
	src = geo.join(parts, name + "_parts")
	low = geo.duplicate(src, name)
	low.vertex_groups.clear()
	low.data.materials.clear()
	low = geo.remesh(low, voxel, smooth)
	low = geo.decimate(low, tris)
	low = geo.triangulate(low)
	rig.transfer_groups(low, src, groups)
	bake.unwrap(low, angle=66.0)
	files = bake.bake(low, [src], size, name)
	if not keep_parts:
		src.hide_render = True
	return dict(mesh=low, files=files, sources=[src])


def mixed(name, hard, soft, voxel=0.02, tris=3000, size=512):
	# Hard parts kept as modelled, soft parts fused; both baked together onto one texture set.
	soft_src = geo.join(soft, name + "_soft_parts")
	soft_low = geo.duplicate(soft_src, name + "_soft")
	soft_low.vertex_groups.clear()
	soft_low.data.materials.clear()
	soft_low = geo.remesh(soft_low, voxel, 2)
	soft_low = geo.decimate(soft_low, tris)
	rig.transfer_groups(soft_low, soft_src)
	hard_src = geo.join(hard, name + "_hard_parts")
	hard_low = geo.duplicate(hard_src, name + "_hard")
	low = geo.join([hard_low, soft_low], name)
	low.data.materials.clear()
	low = geo.triangulate(low)
	bake.unwrap(low)
	files = bake.bake(low, [hard_src, soft_src], size, name)
	hard_src.hide_render = True
	soft_src.hide_render = True
	return dict(mesh=low, files=files, sources=[hard_src, soft_src])
