# Relic: mother's smartphone. A black slab in a worn rose case, the dark screen cracked from one corner with
# a faint sheen across it, and the little bear charm she hung from the corner still swinging.
import math

from gse import geo, mat, pipeline

PW, PH = 0.19, 0.37


def rounded(w, h, r, segs=4):
	# Rounded rectangle outline (x, z), counter-clockwise.
	pts = []
	for (cx, cz, a0) in ((w * 0.5 - r, -h * 0.5 + r, -90.0), (w * 0.5 - r, h * 0.5 - r, 0.0), (-w * 0.5 + r, h * 0.5 - r, 90.0), (-w * 0.5 + r, -h * 0.5 + r, 180.0)):
		for k in range(segs + 1):
			a = math.radians(a0 + 90.0 * k / segs)
			pts.append((cx + math.cos(a) * r, cz + math.sin(a) * r))
	return pts


def slab(w, h, r, thick, y_front, m, name, segs=4, bevel=0.0, x=0.0, z=0.0):
	# A rounded slab standing in the XZ plane, its front face at y_front, running back by thick.
	return geo.polygon_slab(rounded(w, h, r, segs), thick, (x, y_front + thick, z), (90, 0, 0), mat=m, name=name, bevel=bevel)


def settle(parts, rot, loc):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	case = mat.make("phone_case", (0.62, 0.45, 0.45), pattern="plain", rough=0.75, var=0.12, grime=0.35, edge=0.35, bump=0.1)
	body = mat.make("phone_body", (0.10, 0.10, 0.11), pattern="plain", rough=0.4, var=0.05, grime=0.1, edge=0.3, bump=0.02)
	screen = mat.make("phone_screen", (0.05, 0.06, 0.07), pattern="plain", rough=0.08, var=0.04, grime=0.0, edge=0.05, bump=0.0)
	sheen = mat.make("phone_sheen", (0.20, 0.23, 0.25), pattern="plain", rough=0.08, var=0.05, grime=0.0, edge=0.0, bump=0.0)
	crack = mat.make("phone_crack", (0.66, 0.69, 0.70), pattern="plain", rough=0.1, grime=0.0, edge=0.0, bump=0.0)
	lens = mat.make("phone_lens", (0.03, 0.03, 0.04), pattern="plain", rough=0.1, grime=0.0, edge=0.2, bump=0.0)
	ring = mat.make("phone_ring", (0.55, 0.54, 0.52), pattern="plain", rough=0.3, grime=0.1, edge=0.3, bump=0.0)
	cord = mat.make("phone_cord", (0.70, 0.36, 0.32), pattern="knit", rough=0.9, bump=0.2)
	bear = mat.make("phone_bear", (0.80, 0.66, 0.42), pattern="fur", rough=0.95, var=0.1, grime=0.2, edge=0.1, bump=0.3)
	dot = mat.make("phone_bear_eye", (0.10, 0.08, 0.07), pattern="plain", rough=0.5, grime=0.0, edge=0.0, bump=0.0)

	parts = []
	T = 0.034
	# Case wraps the phone: back and sides, a raised lip round the screen.
	parts.append(slab(PW + 0.016, PH + 0.016, 0.034, T + 0.008, -T * 0.5 - 0.003, case, "case", segs=4, bevel=0.004))
	parts.append(slab(PW - 0.004, PH - 0.004, 0.026, 0.004, -T * 0.5 - 0.004, body, "bezel", segs=4))
	parts.append(slab(PW - 0.020, PH - 0.040, 0.016, 0.002, -T * 0.5 - 0.0055, screen, "screen", segs=3))
	# A faint sheen across the dark glass, and the crack running out from the bottom left.
	parts.append(geo.polygon_slab([(-0.084, 0.09), (-0.084, 0.145), (0.084, 0.035), (0.084, -0.02)], 0.001, (0.0, -T * 0.5 - 0.0056, 0.0), (90, 0, 0), mat=sheen, name="sheen"))
	parts.append(geo.polygon_slab([(-0.084, 0.155), (-0.084, 0.163), (0.084, 0.06), (0.084, 0.052)], 0.001, (0.0, -T * 0.5 - 0.0056, 0.0), (90, 0, 0), mat=sheen, name="sheen"))
	hit = (-0.058, -0.140)
	fy = -T * 0.5 - 0.0068
	for (ex, ez) in ((0.082, -0.02), (0.02, 0.12), (-0.082, 0.04), (0.082, -0.158), (-0.03, 0.05)):
		mid = ((hit[0] + ex) * 0.5 + 0.008, (hit[1] + ez) * 0.5 - 0.006)
		parts.append(geo.tube([(hit[0], fy, hit[1]), (mid[0], fy, mid[1]), (ex, fy, ez)], 0.0016, mat=crack, segments=3, name="crack"))
	web = [(hit[0] + math.cos(a) * 0.035, fy, hit[1] + math.sin(a) * 0.035) for a in (-0.3, 0.3, 0.9, 1.5, 2.1)]
	parts.append(geo.tube(web, 0.0013, mat=crack, segments=3, name="crack_web"))
	parts.append(geo.blob((0.008, 0.002, 0.008), (hit[0], fy, hit[1]), mat=crack, subdivisions=1, name="crack_hit"))
	# Earpiece and front camera.
	parts.append(geo.box((0.04, 0.002, 0.006), (0.0, -T * 0.5 - 0.0056, PH * 0.5 - 0.012), mat=lens, name="earpiece"))
	parts.append(geo.cylinder(0.005, 0.002, (0.03, -T * 0.5 - 0.0056, PH * 0.5 - 0.012), (90, 0, 0), mat=lens, segments=8, name="front_cam"))
	# Back: camera block with two lenses through the case.
	by = T * 0.5 + 0.006
	parts.append(slab(0.05, 0.10, 0.018, 0.008, by - 0.002, body, "cam_block", segs=3, x=-0.056, z=PH * 0.5 - 0.071))
	for (x, z) in ((-0.056, PH * 0.5 - 0.050), (-0.056, PH * 0.5 - 0.092)):
		parts.append(geo.cylinder(0.016, 0.006, (x, by + 0.008, z), (90, 0, 0), mat=ring, segments=12, name="lens_ring"))
		parts.append(geo.cylinder(0.011, 0.006, (x, by + 0.010, z), (90, 0, 0), mat=lens, segments=12, name="lens"))
	# Side buttons.
	for (z, l) in ((0.10, 0.05), (0.03, 0.035)):
		parts.append(geo.box((0.008, 0.016, l), (PW * 0.5 + 0.009, 0.0, z), mat=case, bevel=0.003, bevel_segments=1, name="button"))
	# Worn patches at the case corners, rubbed back to pale plastic.
	worn = mat.make("phone_case_worn", (0.76, 0.66, 0.62), pattern="plain", rough=0.8, grime=0.1, edge=0.1, bump=0.05)
	for (x, z) in ((PW * 0.5 + 0.004, PH * 0.5 - 0.01), (-PW * 0.5 - 0.004, -PH * 0.5 + 0.012), (PW * 0.5 + 0.004, -PH * 0.5 + 0.02)):
		parts.append(geo.blob((0.010, 0.018, 0.022), (x, 0.0, z), mat=worn, subdivisions=1, name="worn"))

	# The charm: a cord loop from the bottom left corner and a little bear.
	cx, cz = -PW * 0.5 + 0.012, -PH * 0.5 - 0.004
	loop = [(cx, 0.0, cz), (cx - 0.010, -0.006, cz - 0.022), (cx - 0.012, 0.0, cz - 0.045), (cx - 0.004, 0.006, cz - 0.022), (cx + 0.002, 0.0, cz)]
	parts.append(geo.tube(loop, 0.0032, mat=cord, segments=4, name="charm_cord"))
	bx, bz = cx - 0.012, cz - 0.072
	parts.append(geo.sphere((0.030, 0.022, 0.028), (bx, 0.0, bz), mat=bear, segments=10, rings=7, name="bear_head"))
	for s in (-1.0, 1.0):
		parts.append(geo.sphere(0.011, (bx + s * 0.022, 0.0, bz + 0.022), mat=bear, segments=8, rings=5, name="bear_ear"))
		parts.append(geo.sphere(0.004, (bx + s * 0.010, -0.021, bz + 0.004), mat=dot, segments=6, rings=4, name="bear_eye"))
	parts.append(geo.sphere((0.010, 0.006, 0.008), (bx, -0.021, bz - 0.008), mat=mat.make("phone_bear_snout", (0.88, 0.80, 0.66), pattern="plain", rough=0.9, grime=0.0, edge=0.0, bump=0.0),
							segments=8, rings=5, name="bear_snout"))
	parts.append(geo.sphere(0.003, (bx, -0.027, bz - 0.006), mat=dot, segments=6, rings=4, name="bear_nose"))

	settle(parts, (-14, 0, 15), (0.0, 0.0, 0.38))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("case", "button")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=15)

	return pipeline.static("relic_phone", parts, size=256)
