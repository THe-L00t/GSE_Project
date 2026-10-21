# Relic: grandfather's bus ticket. Route 32, one adult: the outward half punched, the return stub still
# whole past the perforation, because the bus never came back. Curled from years in a coat pocket,
# a paper clip still on its edge.
import math

import bmesh
import bpy

from gse import core, geo, mat, pipeline

W, H = 0.40, 0.22


def paper(nx, nz, holes, mat_obj, name="ticket"):
	# The ticket as a grid in the XZ plane facing -Y, with the punched cells left out.
	bm = bmesh.new()
	verts = [[bm.verts.new((-W * 0.5 + W * i / nx, 0.0, -H * 0.5 + H * k / nz)) for k in range(nz + 1)] for i in range(nx + 1)]
	for i in range(nx):
		for k in range(nz):
			if (i, k) in holes:
				continue
			bm.faces.new((verts[i][k], verts[i + 1][k], verts[i + 1][k + 1], verts[i][k + 1]))
	# Round the punched hole a little: pull its four outer corners in.
	if holes:
		cx = sum(i for i, k in holes) / len(holes) + 0.5
		cz = sum(k for i, k in holes) / len(holes) + 0.5
		for (i, k) in ((int(cx - 1), int(cz - 1)), (int(cx + 1), int(cz - 1)), (int(cx - 1), int(cz + 1)), (int(cx + 1), int(cz + 1))):
			v = verts[i][k]
			c = (-W * 0.5 + W * cx / nx, -H * 0.5 + H * cz / nz)
			v.co.x = c[0] + (v.co.x - c[0]) * 0.72
			v.co.z = c[1] + (v.co.z - c[1]) * 0.72
	bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
	mesh = bpy.data.meshes.new(name)
	bm.to_mesh(mesh)
	bm.free()
	obj = core.link(bpy.data.objects.new(name, mesh))
	mesh.materials.append(mat_obj)
	mod = obj.modifiers.new("solid", 'SOLIDIFY')
	mod.thickness = 0.006
	mod.offset = 0.0
	return obj


def curl(objs):
	# Both ends curl back, the bottom right corner more, as if pressed long in a pocket.
	for o in objs:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		for v in o.data.vertices:
			u = v.co.x / (W * 0.5)
			w = v.co.z / (H * 0.5)
			v.co.y += 0.075 * u * u + 0.022 * max(0.0, u) * max(0.0, -w) * 2.0 - 0.01 * w * w
		o.data.update()


def settle(parts, rot, loc):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	card = mat.make("ticket_paper", (0.86, 0.80, 0.65), pattern="paper", rough=0.9, var=0.12, grime=0.3, edge=0.12, bump=0.15)
	stub_mat = mat.make("ticket_stub", (0.84, 0.78, 0.60), pattern="paper", rough=0.9, var=0.12, grime=0.3, edge=0.12, bump=0.15)
	band = mat.make("ticket_band", (0.34, 0.50, 0.47), pattern="paper", rough=0.85, var=0.1, grime=0.2, edge=0.0, bump=0.05)
	ink = mat.make("ticket_ink", (0.16, 0.14, 0.13), pattern="plain", rough=0.8, grime=0.0, edge=0.0, bump=0.0)
	red_ink = mat.make("ticket_red_ink", (0.58, 0.22, 0.18), pattern="plain", rough=0.8, grime=0.0, edge=0.0, bump=0.0)
	wire = mat.make("ticket_clip", (0.70, 0.70, 0.66), pattern="plain", rough=0.3, var=0.05, grime=0.2, edge=0.3, bump=0.0)

	nx, nz = 14, 6
	perf = 10
	px = -W * 0.5 + W * perf / nx
	parts = []
	parts.append(paper(nx, nz, {(7, 1), (8, 1), (7, 2), (8, 2)}, card, name="ticket"))
	# The return stub is a slightly different stock, printed over: a separate skin on its face.
	sx0, sx1 = px + 0.004, W * 0.5 - 0.004
	stub = geo.grid(sx1 - sx0, H - 0.008, 3, 4, ((sx0 + sx1) * 0.5, -0.0042, 0.0), (90, 0, 0), mat=stub_mat, name="stub")
	parts.append(stub)
	# Coloured band across the top, both halves.
	parts.append(geo.grid(px - (-W * 0.5) - 0.006, 0.040, 7, 1, ((px - W * 0.5) * 0.5, -0.0044, H * 0.5 - 0.026), (90, 0, 0), mat=band, name="band"))
	parts.append(geo.grid(sx1 - sx0 - 0.004, 0.040, 3, 1, ((sx0 + sx1) * 0.5, -0.0050, H * 0.5 - 0.026), (90, 0, 0), mat=band, name="band"))
	# Perforation between the halves.
	for k in range(7):
		z = -H * 0.5 + 0.016 + k * (H - 0.032) / 6.0
		parts.append(geo.box((0.004, 0.009, 0.010), (px, 0.0, z), mat=ink, name="perf"))
	# Lettering: route and fare on the outward half, "return" on the stub.
	t1 = geo.text("32번", 0.08, (-0.088, -0.0055, 0.024), (90, 0, 0), mat=ink, depth=0.0015, name="route")
	parts.append(geo.decimate(t1, 220))
	t2 = geo.text("성인 1", 0.046, (-0.105, -0.0055, -0.068), (90, 0, 0), mat=ink, depth=0.0015, name="fare")
	parts.append(geo.decimate(t2, 190))
	t3 = geo.text("돌아오는", 0.024, ((sx0 + sx1) * 0.5, -0.0062, 0.020), (90, 0, 0), mat=red_ink, depth=0.0012, name="return")
	parts.append(geo.decimate(t3, 240))
	t4 = geo.text("편", 0.030, ((sx0 + sx1) * 0.5, -0.0062, -0.012), (90, 0, 0), mat=red_ink, depth=0.0012, name="return")
	parts.append(geo.decimate(t4, 130))
	for k in range(2):
		parts.append(geo.box((0.06 - k * 0.016, 0.002, 0.005), ((sx0 + sx1) * 0.5, -0.0055, -0.045 - k * 0.018), mat=ink, name="fine_print"))
	# Conditions of carriage printed small on the back.
	for k in range(5):
		parts.append(geo.box((0.20 - (k % 3) * 0.03, 0.002, 0.006), (-0.06, 0.0035, 0.078 - k * 0.017), mat=ink, name="back_print"))
	# Paper clip over the top edge of the stub, front and back.
	cx = sx1 - 0.045
	ztop = H * 0.5
	t = 0.007
	clip = [(cx - 0.012, -t, ztop - 0.075), (cx - 0.012, -t, ztop + 0.004), (cx - 0.012, 0.0, ztop + 0.014), (cx - 0.012, t, ztop + 0.004),
			(cx - 0.012, t, ztop - 0.095), (cx, t + 0.0005, ztop - 0.107), (cx + 0.012, t, ztop - 0.095), (cx + 0.012, t, ztop + 0.004),
			(cx + 0.012, 0.0, ztop + 0.016), (cx + 0.012, -t, ztop + 0.004), (cx + 0.012, -t, ztop - 0.058), (cx + 0.004, -t - 0.0005, ztop - 0.066),
			(cx - 0.004, -t, ztop - 0.058), (cx - 0.004, -t, ztop - 0.02)]
	parts.append(geo.tube(clip, 0.0028, mat=wire, segments=3, name="clip"))

	curl(parts)
	settle(parts, (-22, 0, 15), (0.0, 0.0, 0.35))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("ticket", "stub")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=11)

	return pipeline.static("relic_ticket", parts, size=256)
