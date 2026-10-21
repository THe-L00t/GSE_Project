# Relic: the pocket transistor radio grandmother kept by her pillow. Cream plastic, a slotted speaker grille,
# the tuning dial and its window, the aerial half drawn out. Deep in the dial window a spore-light breathes.
import math

from gse import geo, mat, pipeline


def settle(parts, rot, loc):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	shell = mat.make("radio_shell", (0.74, 0.67, 0.52), pattern="plain", rough=0.55, var=0.1, grime=0.4, edge=0.3, bump=0.08)
	face = mat.make("radio_face", (0.42, 0.30, 0.22), pattern="cloth", rough=0.8, var=0.12, grime=0.35, edge=0.15, bump=0.3)
	chrome = mat.make("radio_chrome", (0.70, 0.68, 0.62), pattern="plain", rough=0.3, var=0.05, grime=0.3, edge=0.35, bump=0.04)
	dark = mat.make("radio_dark", (0.16, 0.14, 0.13), pattern="plain", rough=0.6, grime=0.1, edge=0.2, bump=0.05)
	scale = mat.make("radio_scale", (0.86, 0.80, 0.64), pattern="paper", rough=0.5, grime=0.2, edge=0.0, bump=0.02)
	needle = mat.make("radio_needle", (0.72, 0.22, 0.16), pattern="plain", rough=0.5, grime=0.0, edge=0.0, bump=0.0)
	strap = mat.make("radio_strap", (0.36, 0.24, 0.16), pattern="cloth", rough=0.8, bump=0.3, edge=0.2)
	glow = mat.get("spore_glow")

	W, D, H = 0.40, 0.11, 0.25
	fy = -D * 0.5
	parts = []
	parts.append(geo.box((W, D, H), (0, 0, 0), mat=shell, bevel=0.022, bevel_segments=2, name="body"))
	# Front: a brown panel, slotted grille on the left, dial and its window on the right.
	parts.append(geo.box((W - 0.03, 0.01, H - 0.03), (0, fy - 0.002, 0), mat=face, bevel=0.006, bevel_segments=1, name="panel"))
	gx = -0.085
	parts.append(geo.box((0.19, 0.008, 0.18), (gx, fy - 0.008, -0.005), mat=dark, bevel=0.005, bevel_segments=1, name="grille_back"))
	for k in range(8):
		z = -0.082 + k * 0.022
		parts.append(geo.box((0.176, 0.006, 0.009), (gx, fy - 0.013, z), mat=chrome, name="grille_bar"))
	rim = [(gx - 0.095, fy - 0.013, -0.1), (gx - 0.095, fy - 0.013, 0.09), (gx + 0.095, fy - 0.013, 0.09), (gx + 0.095, fy - 0.013, -0.1), (gx - 0.095, fy - 0.013, -0.1)]
	parts.append(geo.tube(rim, 0.006, mat=chrome, segments=4, name="grille_rim"))
	parts.append(geo.box((0.05, 0.006, 0.012), (0.10, fy - 0.008, 0.096), mat=chrome, name="badge"))
	# Tuning window with its scale and needle.
	wx, wz = 0.10, 0.055
	parts.append(geo.box((0.15, 0.008, 0.05), (wx, fy - 0.008, wz), mat=chrome, bevel=0.004, bevel_segments=1, name="window_rim"))
	parts.append(geo.box((0.135, 0.004, 0.036), (wx, fy - 0.012, wz), mat=scale, name="window"))
	for k in range(9):
		x = wx - 0.058 + k * 0.0145
		parts.append(geo.box((0.002, 0.003, 0.012 if k % 2 == 0 else 0.007), (x, fy - 0.0145, wz + 0.006), mat=dark, name="tick"))
	parts.append(geo.box((0.0035, 0.003, 0.03), (wx + 0.018, fy - 0.0155, wz), mat=needle, name="needle"))
	# Where the needle rests, the faint light: the breathing she heard.
	g = geo.blob((0.008, 0.003, 0.008), (wx + 0.018, fy - 0.0145, wz - 0.004), mat=glow, subdivisions=1, name="breath")
	geo.tag(g, "pulse")
	parts.append(g)
	# Tuning knob and a small volume wheel on the side.
	parts.append(geo.cylinder(0.046, 0.022, (wx, fy - 0.016, -0.045), (90, 0, 0), mat=chrome, segments=14, name="dial_knob"))
	parts.append(geo.cylinder(0.034, 0.012, (wx, fy - 0.030, -0.045), (90, 0, 0), mat=shell, segments=14, name="dial_cap"))
	parts.append(geo.box((0.006, 0.004, 0.022), (wx, fy - 0.037, -0.032), mat=dark, name="dial_mark"))
	parts.append(geo.cylinder(0.030, 0.016, (W * 0.5 + 0.004, 0.0, 0.06), (0, 90, 0), mat=dark, segments=12, name="volume"))
	# Aerial: base swivel at the top right corner, three sections drawn half out, leaning back and over.
	ax, az = 0.165, H * 0.5
	parts.append(geo.sphere(0.016, (ax, 0.02, az + 0.004), mat=chrome, segments=8, rings=5, name="aerial_base"))
	d = (-0.42, 0.30, 0.86)
	ln = math.sqrt(sum(c * c for c in d))
	d = tuple(c / ln for c in d)
	start = (ax, 0.02, az + 0.01)
	segs = ((0.008, 0.11), (0.0062, 0.10), (0.0048, 0.08))
	t = 0.0
	for (r, l) in segs:
		p0 = tuple(start[i] + d[i] * t for i in range(3))
		p1 = tuple(start[i] + d[i] * (t + l) for i in range(3))
		parts.append(geo.tube([p0, p1], r, mat=chrome, segments=6, name="aerial"))
		t += l - 0.01
	tip = tuple(start[i] + d[i] * (t + 0.01) for i in range(3))
	parts.append(geo.sphere(0.009, tip, mat=chrome, segments=8, rings=5, name="aerial_tip"))
	# Leather carry strap over the top.
	parts.append(geo.tube([(-0.17, 0.0, az - 0.01), (-0.15, -0.005, az + 0.05), (-0.04, -0.005, az + 0.07), (0.07, 0.0, az + 0.04), (0.09, 0.0, az - 0.01)],
						  0.010, mat=strap, segments=5, name="strap"))
	for x in (-0.17, 0.09):
		parts.append(geo.box((0.03, 0.03, 0.02), (x, 0.0, az - 0.004), mat=chrome, name="lug"))
	# Back: the battery door and its screws.
	parts.append(geo.box((0.24, 0.006, 0.14), (-0.02, D * 0.5 + 0.002, -0.02), mat=shell, bevel=0.004, bevel_segments=1, name="battery_door"))
	for x in (-0.12, 0.08):
		parts.append(geo.cylinder(0.008, 0.006, (x, D * 0.5 + 0.005, 0.07), (90, 0, 0), mat=chrome, segments=6, name="screw"))

	settle(parts, (-8, 0, 18), (0.0, 0.0, 0.35))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("body", "panel", "battery_door")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=9)

	return pipeline.static("relic_radio", parts, size=256)
