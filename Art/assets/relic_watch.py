# Relic: father's wristwatch. A round steel case on a worn leather strap still buckled in a loop, the glass
# cracked from one corner; the hole he always used is stretched long.
import math

import bmesh
import bpy

from gse import core, geo, mat, pipeline


def band(points, normals, width, thick, mat_obj, name="band", closed=True):
	# A flat strap along a path: points and outward normals in the YZ plane, width along X.
	bm = bmesh.new()
	rings = []
	for p, n in zip(points, normals):
		ring = []
		for (sx, sn) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
			ring.append(bm.verts.new((p[0] + sx * width * 0.5, p[1] + n[1] * sn * thick * 0.5, p[2] + n[2] * sn * thick * 0.5)))
		rings.append(ring)
	count = len(rings) if closed else len(rings) - 1
	for i in range(count):
		a, b = rings[i], rings[(i + 1) % len(rings)]
		for k in range(4):
			j = (k + 1) % 4
			bm.faces.new((a[k], a[j], b[j], b[k]))
	if not closed:
		bm.faces.new(list(reversed(rings[0])))
		bm.faces.new(rings[-1])
	bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
	mesh = bpy.data.meshes.new(name)
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, mesh))
	mesh.materials.append(mat_obj)
	for poly in mesh.polygons:
		poly.use_smooth = True
	mesh.set_sharp_from_angle(angle=math.radians(50.0))
	return obj


def settle(parts, rot, loc, scale=1.0):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.scale = (scale, scale, scale)
		o.location = loc
		geo.apply_transform(o)


def build():
	steel = mat.make("watch_steel", (0.70, 0.68, 0.63), pattern="plain", rough=0.3, var=0.06, grime=0.35, edge=0.35, bump=0.05)
	dial = mat.make("watch_dial", (0.88, 0.82, 0.68), pattern="paper", rough=0.6, var=0.08, grime=0.3, edge=0.0, bump=0.02)
	ink = mat.make("watch_ink", (0.13, 0.12, 0.11), pattern="plain", rough=0.5, grime=0.0, edge=0.1, bump=0.0)
	red = mat.make("watch_second", (0.70, 0.24, 0.16), pattern="plain", rough=0.5, grime=0.0, edge=0.0, bump=0.0)
	crack = mat.make("watch_crack", (0.84, 0.86, 0.84), pattern="plain", rough=0.2, grime=0.0, edge=0.0, bump=0.0)
	leather = mat.make("watch_leather", (0.40, 0.26, 0.16), pattern="cloth", rough=0.8, var=0.15, grime=0.4, edge=0.25, bump=0.3)
	worn = mat.make("watch_leather_worn", (0.52, 0.38, 0.26), pattern="cloth", rough=0.85, var=0.1, grime=0.2, edge=0.1, bump=0.2)
	hole = mat.make("watch_hole", (0.10, 0.07, 0.05), pattern="plain", rough=0.9, grime=0.0, edge=0.0, bump=0.0)

	parts = []
	# Strap: buckled into a loop standing behind the case, in the YZ plane.
	cy, ry, rz = 0.075, 0.085, 0.125
	n = 28
	pts, nrm = [], []
	for k in range(n):
		a = k / float(n) * math.tau
		pts.append((0.0, cy - math.cos(a) * ry, math.sin(a) * rz))
		ny, nz = -math.cos(a) / ry, math.sin(a) / rz
		ln = math.hypot(ny, nz)
		nrm.append((0.0, ny / ln, nz / ln))
	parts.append(band(pts, nrm, 0.062, 0.012, leather, name="strap"))
	# The free end runs past the buckle, a keeper holds it down.
	tail_pts, tail_n = [], []
	for k in range(8):
		a = math.pi * 0.62 + k / 7.0 * 0.9
		tail_pts.append((0.0, cy - math.cos(a) * (ry + 0.013), math.sin(a) * (rz + 0.013)))
		ny, nz = -math.cos(a) / ry, math.sin(a) / rz
		ln = math.hypot(ny, nz)
		tail_n.append((0.0, ny / ln, nz / ln))
	parts.append(band(tail_pts, tail_n, 0.058, 0.010, leather, name="strap_tail", closed=False))
	# Holes along the tail, the much-used one stretched and pale round its edge.
	for k in (2, 3, 4, 5):
		p, nn = tail_pts[k], tail_n[k]
		ang = math.degrees(math.atan2(nn[2], nn[1]))
		at = (0.0, p[1] + nn[1] * 0.0055, p[2] + nn[2] * 0.0055)
		if k == 3:
			parts.append(geo.blob((0.016, 0.003, 0.022), at, (ang, 0, 0), mat=worn, subdivisions=1, name="worn_hole"))
		at = (0.0, p[1] + nn[1] * 0.0065, p[2] + nn[2] * 0.0065)
		parts.append(geo.blob((0.007, 0.003, 0.013 if k == 3 else 0.007), at, (ang, 0, 0), mat=hole, subdivisions=1, name="hole"))
	# Buckle at the back of the loop.
	by = cy + ry + 0.01
	frame = [(-0.042, by, -0.022), (-0.042, by, 0.022), (0.042, by, 0.022), (0.042, by, -0.022), (-0.042, by, -0.022)]
	parts.append(geo.tube(frame, 0.006, mat=steel, segments=5, name="buckle"))
	parts.append(geo.box((0.006, 0.006, 0.04), (0.0, by + 0.004, 0.0), mat=steel, name="tongue"))
	parts.append(geo.box((0.07, 0.016, 0.012), (0.0, by - 0.002, -0.04), mat=leather, name="keeper"))

	# The case, facing -Y, sitting against the front of the loop.
	case_y = -0.028
	case = geo.lathe([(0.060, 0.0), (0.072, 0.006), (0.078, 0.020), (0.076, 0.030), (0.068, 0.036), (0.064, 0.032)],
					 (0, case_y + 0.018, 0), (90, 0, 0), mat=steel, segments=20, name="case", close_top=False)
	parts.append(case)
	fy = case_y - 0.012
	parts.append(geo.cylinder(0.064, 0.004, (0, fy, 0), (90, 0, 0), mat=dial, segments=20, name="dial"))
	for k in range(12):
		a = k / 12.0 * math.tau
		long = k % 3 == 0
		parts.append(geo.box((0.005 if long else 0.003, 0.003, 0.014 if long else 0.008), (math.sin(a) * 0.052, fy - 0.002, math.cos(a) * 0.052), (0, math.degrees(a), 0),
							 mat=ink, name="marker"))
	# Hands at twenty to five.
	def hand(length, width, angle, m, dy, name):
		a = math.radians(angle)
		parts.append(geo.box((width, 0.002, length), (math.sin(a) * length * 0.4, fy - dy, math.cos(a) * length * 0.4), (0, angle, 0), mat=m, name=name))
	hand(0.036, 0.007, 140.0, ink, 0.004, "hour_hand")
	hand(0.054, 0.005, 240.0, ink, 0.006, "minute_hand")
	hand(0.056, 0.0018, 20.0, red, 0.008, "second_hand")
	parts.append(geo.cylinder(0.006, 0.006, (0, fy - 0.008, 0), (90, 0, 0), mat=steel, segments=8, name="pin"))
	# Cracked glass: lines running out from a knock near one o'clock.
	hit = (0.030, 0.042)
	for (ex, ez) in ((-0.044, -0.022), (0.058, -0.012), (-0.012, 0.058)):
		mid = ((hit[0] + ex) * 0.5 + 0.006, (hit[1] + ez) * 0.5 - 0.005)
		parts.append(geo.tube([(hit[0], fy - 0.011, hit[1]), (mid[0], fy - 0.011, mid[1]), (ex * 0.95, fy - 0.010, ez * 0.95)], 0.0014, mat=crack, segments=3, name="crack"))
	parts.append(geo.blob((0.004, 0.0015, 0.004), (hit[0], fy - 0.011, hit[1]), mat=crack, subdivisions=1, name="crack_hit"))
	# Crown at three o'clock and the lugs into the strap.
	parts.append(geo.cylinder(0.010, 0.016, (0.084, case_y + 0.004, 0), (0, 90, 0), mat=steel, segments=8, name="crown"))
	for sz in (-1.0, 1.0):
		for sx in (-1.0, 1.0):
			parts.append(geo.box((0.012, 0.022, 0.026), (sx * 0.024, case_y + 0.012, sz * 0.074), mat=steel, bevel=0.003, bevel_segments=1, name="lug"))

	settle(parts, (-16, 0, 20), (0.0, 0.0, 0.375), scale=1.18)
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("strap", "case", "buckle")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=7)

	return pipeline.static("relic_watch", parts, size=256)
