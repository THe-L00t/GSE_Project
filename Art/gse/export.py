import bpy
import os
import struct

from mathutils import Matrix, Vector

from . import core

# Matches AnimFlag in SimpleGame/Mesh.h.
ANIM_BITS = {"breathe": 1, "sway": 2, "pulse": 4, "hover": 8, "spin": 16, "bob": 32, "foliage": 64}

FLAG_SKINNED = 1
FLAG_INDEX32 = 2


def _name(s, size):
	b = s.encode("ascii")[:size - 1]
	return b + b"\0" * (size - len(b))


def _snorm16(x):
	return max(-32767, min(32767, int(round(x * 32767.0))))


def _cols(m):
	return [m[r][c] for c in range(4) for r in range(4)]


def bone_order(arm_obj):
	order = []
	def visit(b):
		order.append(b)
		for c in b.children:
			visit(c)
	for b in arm_obj.data.bones:
		if b.parent is None:
			visit(b)
	return order


def write_mesh(name, obj, arm_obj=None, textures=None, specular=0.25, emissive=1.0, unit=None):
	# GSM1: header, bones, 52-byte vertices, indices. Positions in game space (Y up, front +Z).
	# unit (game x, y, z sizes) squeezes a model built at real size into the unit box the game
	# stretches back out per placement, as it did for the old recipes of houses, sheds and rails.
	core.ensure_dirs()
	sx, sy, sz = unit if unit is not None else (1.0, 1.0, 1.0)
	mesh = obj.data
	uv_name = mesh.uv_layers.active.name
	mesh.calc_tangents(uvmap=uv_name)
	uvs = mesh.uv_layers[uv_name].data
	normals = mesh.corner_normals

	group_names = {g.index: g.name for g in obj.vertex_groups}
	bones = bone_order(arm_obj) if arm_obj is not None else []
	bone_index = {b.name: i for i, b in enumerate(bones)}

	color_attr = mesh.color_attributes.get("tint")
	C = core.TO_GAME.to_3x3()

	vert_info = []
	for v in mesh.vertices:
		anim = 0
		moss = 0.0
		joints = []
		for ge in v.groups:
			gname = group_names.get(ge.group)
			if gname is None:
				continue
			if gname in ANIM_BITS and ge.weight > 0.5:
				anim |= ANIM_BITS[gname]
			elif gname == "moss":
				moss = ge.weight
			elif gname in bone_index and ge.weight > 0.001:
				joints.append((ge.weight, bone_index[gname]))
		joints.sort(reverse=True)
		joints = joints[:4]
		total = sum(w for w, _ in joints)
		if total > 0.0:
			raw = [w / total * 255.0 for w, _ in joints]
			ws = [int(round(x)) for x in raw]
			ws[0] += 255 - sum(ws)
			js = [j for _, j in joints]
		else:
			ws, js = [], []
		while len(ws) < 4:
			ws.append(0)
			js.append(0)
		vert_info.append((anim, moss, js, ws))

	verts = []
	index_of = {}
	indices = []
	lo = Vector((1e9, 1e9, 1e9))
	hi = Vector((-1e9, -1e9, -1e9))
	for poly in mesh.polygons:
		if len(poly.loop_indices) != 3:
			raise RuntimeError(name + ": mesh is not triangulated")
		for li in poly.loop_indices:
			loop = mesh.loops[li]
			vi = loop.vertex_index
			p = C @ mesh.vertices[vi].co
			n = (C @ normals[li].vector)
			t = (C @ loop.tangent)
			p = Vector((p.x / sx, p.y / sy, p.z / sz))
			n = Vector((n.x * sx, n.y * sy, n.z * sz)).normalized()
			t = Vector((t.x / sx, t.y / sy, t.z / sz)).normalized()
			sign = loop.bitangent_sign
			uv = uvs[li].uv
			if color_attr is not None:
				cv = color_attr.data[li].color if color_attr.domain == 'CORNER' else color_attr.data[vi].color
				tint = (cv[0], cv[1], cv[2])
			else:
				tint = (1.0, 1.0, 1.0)
			anim, moss, js, ws = vert_info[vi]
			key = (vi, round(n.x, 4), round(n.y, 4), round(n.z, 4), round(uv.x, 5), round(uv.y, 5), sign,
				   round(tint[0], 3), round(tint[1], 3), round(tint[2], 3))
			idx = index_of.get(key)
			if idx is None:
				idx = len(verts)
				index_of[key] = idx
				verts.append(struct.pack("<3f4h4h2f4B4B4BI",
					p.x, p.y, p.z,
					_snorm16(n.x), _snorm16(n.y), _snorm16(n.z), 0,
					_snorm16(t.x), _snorm16(t.y), _snorm16(t.z), 32767 if sign >= 0.0 else -32767,
					uv.x, 1.0 - uv.y,
					int(tint[0] * 255 + 0.5), int(tint[1] * 255 + 0.5), int(tint[2] * 255 + 0.5), int(moss * 255 + 0.5),
					js[0], js[1], js[2], js[3],
					ws[0], ws[1], ws[2], ws[3],
					anim))
				lo = Vector((min(lo.x, p.x), min(lo.y, p.y), min(lo.z, p.z)))
				hi = Vector((max(hi.x, p.x), max(hi.y, p.y), max(hi.z, p.z)))
			indices.append(idx)
	mesh.free_tangents()

	flags = FLAG_SKINNED if bones else 0
	big = len(verts) > 65535
	if big:
		flags |= FLAG_INDEX32
	tex = textures or {}
	out = bytearray()
	out += b"GSM1"
	out += struct.pack("<4I", len(verts), len(indices), len(bones), flags)
	out += struct.pack("<6f", lo.x, lo.y, lo.z, hi.x, hi.y, hi.z)
	for key in ("albedo", "normal", "mask"):
		out += _name(os.path.basename(tex.get(key, "")), 48)
	out += struct.pack("<4f", specular, emissive, 0.0, 0.0)
	for b in bones:
		bind = core.TO_GAME @ b.matrix_local @ core.TO_BLENDER
		inv = bind.inverted()
		parent = bone_index[b.parent.name] if b.parent is not None else -1
		out += _name(b.name, 32)
		out += struct.pack("<i", parent)
		out += struct.pack("<16f", *_cols(inv))
	for v in verts:
		out += v
	out += struct.pack("<%d%s" % (len(indices), "I" if big else "H"), *indices)

	path = os.path.join(core.MODEL_OUT, name + ".gsm")
	with open(path, "wb") as f:
		f.write(out)
	return path, len(verts), len(indices) // 3


def write_anims(name, arm_obj, clips, fps=30):
	# GSA1: per clip, every frame holds each bone's transform relative to its parent, in game space.
	core.ensure_dirs()
	scene = bpy.context.scene
	bones = bone_order(arm_obj)
	out = bytearray()
	out += b"GSA1"
	out += struct.pack("<2I", len(bones), len(clips))
	for clip in clips:
		act = clip["action"]
		ad = arm_obj.animation_data
		ad.action = act
		if hasattr(ad, "action_slot") and len(act.slots) > 0:
			ad.action_slot = act.slots[0]
		start, end = clip["start"], clip["end"]
		count = end - start + 1
		out += _name(clip["name"], 32)
		out += struct.pack("<fII", float(fps), count, 1 if clip.get("loop", True) else 0)
		for f in range(start, end + 1):
			scene.frame_set(f)
			for b in bones:
				pb = arm_obj.pose.bones[b.name]
				m = pb.matrix
				if b.parent is not None:
					m = arm_obj.pose.bones[b.parent.name].matrix.inverted() @ m
				me = core.TO_GAME @ m @ core.TO_BLENDER
				t = me.to_translation()
				q = me.to_quaternion().normalized()
				out += struct.pack("<3f4h", t.x, t.y, t.z, _snorm16(q.x), _snorm16(q.y), _snorm16(q.z), _snorm16(q.w))
	path = os.path.join(core.MODEL_OUT, name + ".gsa")
	with open(path, "wb") as f:
		f.write(out)
	return path
