# Reads an exported .gsm/.gsa exactly as SimpleGame/Asset.cpp does, skins the mesh the way Lit.vs does,
# and compares it with Blender's own evaluated pose of the saved .blend.
#   blender -b Art/blend/<name>.blend --python verify_rig.py -- <name> <clip> <frame>
import struct
import sys

import bpy
import numpy as np
from mathutils import Matrix
from mathutils.kdtree import KDTree

args = sys.argv[sys.argv.index("--") + 1:]
name, clip_name, frame = args[0], args[1], int(args[2])
root = bpy.path.abspath("//../../SimpleGame/Assets/Models/")

data = open(root + name + ".gsm", "rb").read()
at = 0


def take(fmt):
	global at
	size = struct.calcsize(fmt)
	v = struct.unpack_from("<" + fmt, data, at)
	at += size
	return v


assert data[:4] == b"GSM1"
at = 4
vc, ic, bc, flags = take("4I")
take("6f")
for _ in range(3):
	take("48s")
take("4f")
names, parents, inv = [], [], []
for b in range(bc):
	names.append(take("32s")[0].split(b"\0")[0].decode())
	parents.append(take("i")[0])
	inv.append(np.array(take("16f")).reshape(4, 4).T)
verts = []
for i in range(vc):
	v = take("3f4h4h2f4B4B4BI")
	verts.append(v)
pos = np.array([v[0:3] for v in verts])
joints = np.array([v[17:21] for v in verts])
weights = np.array([v[21:25] for v in verts], dtype=np.float64) / 255.0

anim = open(root + name + ".gsa", "rb").read()
at = 0
data = anim
assert data[:4] == b"GSA1"
at = 4
abc, cc = take("2I")
assert abc == bc
clips = {}
for c in range(cc):
	cname = take("32s")[0].split(b"\0")[0].decode()
	fps, fcount, loop = take("fII")
	frames = []
	for f in range(fcount):
		row = []
		for b in range(bc):
			t = take("3f")
			q = np.array(take("4h"), dtype=np.float64) / 32767.0
			row.append((np.array(t), q / np.linalg.norm(q)))
		frames.append(row)
	clips[cname] = frames


def pose_matrix(t, q):
	x, y, z, w = q
	m = np.identity(4)
	m[0, 0] = 1 - 2 * (y * y + z * z)
	m[1, 0] = 2 * (x * y + w * z)
	m[2, 0] = 2 * (x * z - w * y)
	m[0, 1] = 2 * (x * y - w * z)
	m[1, 1] = 1 - 2 * (x * x + z * z)
	m[2, 1] = 2 * (y * z + w * x)
	m[0, 2] = 2 * (x * z + w * y)
	m[1, 2] = 2 * (y * z - w * x)
	m[2, 2] = 1 - 2 * (x * x + y * y)
	m[0:3, 3] = t
	return m


row = clips[clip_name][frame]
glob = [None] * bc
skin = [None] * bc
for i in range(bc):
	local = pose_matrix(*row[i])
	glob[i] = glob[parents[i]] @ local if parents[i] >= 0 else local
	skin[i] = glob[i] @ inv[i]
skinned = np.zeros_like(pos)
for k in range(4):
	mats = np.array([skin[j] for j in joints[:, k]])
	hp = np.concatenate([pos, np.ones((len(pos), 1))], axis=1)
	skinned += weights[:, k:k + 1] * np.einsum("nij,nj->ni", mats, hp)[:, :3]
skinned /= np.maximum(weights.sum(axis=1, keepdims=True), 1e-8)

# Blender's own pose of the same frame.
C = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0)))
obj = bpy.data.objects[name]
arm = bpy.data.objects[name + "_rig"]
act = bpy.data.actions[clip_name]
arm.animation_data.action = act
if hasattr(arm.animation_data, "action_slot") and len(act.slots) > 0:
	arm.animation_data.action_slot = act.slots[0]
bpy.context.scene.frame_set(frame)
deps = bpy.context.evaluated_depsgraph_get()
me = obj.evaluated_get(deps).to_mesh()
rest = [C @ v.co for v in obj.data.vertices]
posed = np.array([tuple(C @ v.co) for v in me.vertices])
tree = KDTree(len(rest))
for i, r in enumerate(rest):
	tree.insert(r, i)
tree.balance()
match = np.array([tree.find(p)[1] for p in pos])
err = np.linalg.norm(skinned - posed[match], axis=1)
moved = np.linalg.norm(posed[match] - pos, axis=1)
print("GSE_VERIFY %s %s f%d bones=%d verts=%d moved_max=%.3f err_mean=%.4f err_max=%.4f" % (
	name, clip_name, frame, bc, vc, moved.max(), err.mean(), err.max()))
