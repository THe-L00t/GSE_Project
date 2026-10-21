import bpy
import math

from mathutils import Euler, Quaternion, Vector
from mathutils.bvhtree import BVHTree

from . import core


def armature(name, bones):
	# bones: list of (name, head, tail, parent) in Blender space; the model faces -Y.
	data = bpy.data.armatures.new(name)
	obj = core.link(bpy.data.objects.new(name, data))
	core.set_mode(obj, 'EDIT')
	made = {}
	for (bname, head, tail, parent) in bones:
		eb = data.edit_bones.new(bname)
		eb.head = Vector(head)
		eb.tail = Vector(tail)
		eb.roll = 0.0
		if parent is not None:
			eb.parent = made[parent]
			eb.use_connect = False
		made[bname] = eb
	bpy.ops.object.mode_set(mode='OBJECT')
	for b in data.bones:
		b.use_deform = True
	return obj


def bind(mesh_obj, arm_obj):
	mod = mesh_obj.modifiers.new("armature", 'ARMATURE')
	mod.object = arm_obj
	mesh_obj.parent = arm_obj
	return mod


def transfer_groups(target, source, names=None):
	# Each target vertex takes the groups of the nearest point on the source surface,
	# interpolated across that face. The source is the model in parts, each part tagged rigidly.
	src_mesh = source.data
	src_mw = source.matrix_world
	verts = [src_mw @ v.co for v in src_mesh.vertices]
	polys = [list(p.vertices) for p in src_mesh.polygons]
	tree = BVHTree.FromPolygons(verts, polys)
	src_names = {g.index: g.name for g in source.vertex_groups}
	wanted = set(names) if names else set(src_names.values())
	weights = [dict() for _ in src_mesh.vertices]
	for v in src_mesh.vertices:
		for ge in v.groups:
			n = src_names.get(ge.group)
			if n in wanted:
				weights[v.index][n] = ge.weight
	groups = {}
	for n in wanted:
		groups[n] = target.vertex_groups.get(n) or target.vertex_groups.new(name=n)
	tmw = target.matrix_world
	for v in target.data.vertices:
		co = tmw @ v.co
		hit = tree.find_nearest(co)
		if hit[0] is None:
			continue
		loc, _, fi, _ = hit
		pv = polys[fi]
		# Inverse-distance blend over the face's corners.
		acc = {}
		total = 0.0
		for vi in pv:
			d = (verts[vi] - loc).length + 1e-5
			w = 1.0 / d
			total += w
			for n, gw in weights[vi].items():
				acc[n] = acc.get(n, 0.0) + gw * w
		for n, val in acc.items():
			val /= total
			if val > 0.001:
				groups[n].add([v.index], min(1.0, val), 'REPLACE')


def smooth_groups(obj, names, iterations=4, factor=0.5, limit=4):
	# Softens the joints between rigid parts by averaging bone weights over mesh edges, then keeps
	# the strongest four per vertex and normalises them.
	import numpy as np
	me = obj.data
	count = len(me.vertices)
	edges = np.empty(len(me.edges) * 2, dtype=np.int64)
	me.edges.foreach_get("vertices", edges)
	edges = edges.reshape(-1, 2)
	groups = [obj.vertex_groups.get(n) or obj.vertex_groups.new(name=n) for n in names]
	col = {g.index: i for i, g in enumerate(groups)}
	W = np.zeros((count, len(groups)), dtype=np.float64)
	for v in me.vertices:
		for ge in v.groups:
			i = col.get(ge.group)
			if i is not None:
				W[v.index, i] = ge.weight
	for _ in range(iterations):
		acc = np.zeros_like(W)
		cnt = np.zeros(count)
		np.add.at(acc, edges[:, 0], W[edges[:, 1]])
		np.add.at(acc, edges[:, 1], W[edges[:, 0]])
		np.add.at(cnt, edges[:, 0], 1.0)
		np.add.at(cnt, edges[:, 1], 1.0)
		avg = acc / np.maximum(cnt, 1.0)[:, None]
		W = W * (1.0 - factor) + avg * factor
	order = np.argsort(-W, axis=1)
	keep = np.zeros_like(W, dtype=bool)
	rows = np.arange(count)[:, None]
	keep[rows, order[:, :limit]] = True
	W = np.where(keep, W, 0.0)
	W[W < 0.01] = 0.0
	total = W.sum(axis=1)
	W = W / np.maximum(total, 1e-8)[:, None]
	all_idx = list(range(count))
	for i, g in enumerate(groups):
		g.remove(all_idx)
		nz = np.nonzero(W[:, i])[0]
		for vi in nz:
			g.add([int(vi)], float(W[vi, i]), 'REPLACE')


def _local_rotation(bone, rx, ry, rz):
	# A rotation given about the armature's axes, turned into the bone's own rest frame.
	q_arm = Euler((math.radians(rx), math.radians(ry), math.radians(rz)), 'XYZ').to_quaternion()
	r = bone.matrix_local.to_quaternion()
	return r.inverted() @ q_arm @ r


def new_clip(arm_obj, name):
	if arm_obj.animation_data is None:
		arm_obj.animation_data_create()
	act = bpy.data.actions.new(name)
	act.use_fake_user = True
	arm_obj.animation_data.action = act
	return act


def key(arm_obj, frame, pose, root=None, root_bone="root"):
	# pose: {bone: (rx, ry, rz) degrees about armature axes}; bones left out return to rest.
	for pb in arm_obj.pose.bones:
		pb.rotation_mode = 'QUATERNION'
		rot = pose.get(pb.name)
		pb.rotation_quaternion = _local_rotation(pb.bone, *rot) if rot is not None else Quaternion()
		pb.keyframe_insert("rotation_quaternion", frame=frame)
		if pb.name == root_bone:
			loc = Vector(root) if root is not None else Vector((0, 0, 0))
			pb.location = pb.bone.matrix_local.to_3x3().inverted() @ loc
			pb.keyframe_insert("location", frame=frame)


def clip(arm_obj, name, keys, loop=True):
	# keys: list of (frame, pose, root_offset). A loop repeats its first pose at the end.
	act = new_clip(arm_obj, name)
	for (frame, pose, root) in keys:
		key(arm_obj, frame, pose, root)
	end = keys[-1][0]
	return dict(name=name, action=act, start=keys[0][0], end=end, loop=loop)


def mirror(pose):
	# Swaps _L and _R and flips rotations about Y and Z, for the other half of a walk cycle.
	out = {}
	for b, (rx, ry, rz) in pose.items():
		if b.endswith("_L"):
			nb = b[:-2] + "_R"
		elif b.endswith("_R"):
			nb = b[:-2] + "_L"
		else:
			nb = b
		out[nb] = (rx, -ry, -rz)
	return out
