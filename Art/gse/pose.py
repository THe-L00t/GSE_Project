# Poses keyed by where hands and feet should be: forward kinematics over a bone table, aiming a bone along a
# direction, keeping one level, and a two-bone reach with a pole. The same maths grandma.py carries inline,
# for any rig given as (name, head, tail, parent) rows with rotations in degrees about the armature's axes.
import math

from mathutils import Euler, Vector


def _q(r):
	return Euler([math.radians(a) for a in r], 'XYZ').to_quaternion()


def _deg(q):
	e = q.to_euler('XYZ')
	return (math.degrees(e.x), math.degrees(e.y), math.degrees(e.z))


class Poser:
	def __init__(self, bones):
		self.bones = bones
		self.rest = {b[0]: b for b in bones}

	def fk(self, pose, root=(0, 0, 0)):
		out = {}
		for name, head, tail, parent in self.bones:
			q = _q(pose.get(name, (0, 0, 0)))
			if parent is None:
				out[name] = (Vector(head) + Vector(root), q)
			else:
				ph, pq = out[parent]
				out[name] = (ph + pq @ (Vector(head) - Vector(self.rest[parent][1])), pq @ q)
		return out

	def carried(self, pose, bone, p, root=(0, 0, 0)):
		h, q = self.fk(pose, root)[bone]
		return h + q @ (Vector(p) - Vector(self.rest[bone][1]))

	def length(self, bone):
		return (Vector(self.rest[bone][2]) - Vector(self.rest[bone][1])).length

	def aim(self, pose, bone, want, root=(0, 0, 0)):
		rest = (Vector(self.rest[bone][2]) - Vector(self.rest[bone][1])).normalized()
		pq = self.fk(pose, root)[self.rest[bone][3]][1]
		pose[bone] = _deg(rest.rotation_difference(pq.inverted() @ Vector(want).normalized()))
		return pose

	def level(self, pose, bone, root=(0, 0, 0)):
		# Undo the chain's rotation so this bone keeps its rest orientation (a foot flat on the ground).
		pose[bone] = _deg(self.fk(pose, root)[self.rest[bone][3]][1].inverted())
		return pose

	def reach(self, pose, upper, lower, target, pole, root=(0, 0, 0)):
		s = self.fk(pose, root)[upper][0]
		a, b = self.length(upper), self.length(lower)
		t = Vector(target)
		d = max(abs(a - b) + 1e-3, min(a + b - 1e-3, (t - s).length))
		n = (t - s).normalized()
		p = Vector(pole) - n * Vector(pole).dot(n)
		p.normalize()
		ca = (a * a + d * d - b * b) / (2.0 * a * d)
		e = s + n * (a * ca) + p * (a * math.sqrt(max(0.0, 1.0 - ca * ca)))
		self.aim(pose, upper, e - s, root)
		self.aim(pose, lower, s + n * d - e, root)
		return pose


def sided(bones):
	# Left-side bones (+X) and their mirrored right-side copies; parents named chest or hips stay unsided.
	out = []
	for side, sx in (("L", 1.0), ("R", -1.0)):
		for name, head, tail, parent in bones:
			p = parent if parent in ("chest", "hips") else parent + "_" + side
			out.append((name + "_" + side, (head[0] * sx, head[1], head[2]), (tail[0] * sx, tail[1], tail[2]), p))
	return out


def report_weights(name, obj, deform):
	names = {g.index: g.name for g in obj.vertex_groups}
	bare = 0
	for v in obj.data.vertices:
		if not any(names.get(ge.group) in deform and ge.weight > 0.0 for ge in v.groups):
			bare += 1
	print("GSE_WEIGHTS %s unweighted=%d of %d" % (name, bare, len(obj.data.vertices)))
