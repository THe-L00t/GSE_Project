# Relic: grandfather's hand torch. Yellow and black plastic, the grip rubbed pale by his hand, a crack
# bound up with cloth tape, a cord loop at the tail. The lens still holds a warm glint.
import math

from gse import geo, mat, pipeline


def settle(parts, rot, loc):
	# Built round the origin along +Z; turned and lifted to float as a pickup.
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	yellow = mat.make("torch_yellow", (0.80, 0.64, 0.24), pattern="plain", rough=0.45, var=0.1, grime=0.25, edge=0.3, bump=0.1)
	black = mat.make("torch_black", (0.10, 0.10, 0.10), pattern="plain", rough=0.55, var=0.1, grime=0.1, edge=0.35, bump=0.1)
	worn = mat.make("torch_worn", (0.62, 0.56, 0.42), pattern="plain", rough=0.7, var=0.15, grime=0.1, edge=0.1, bump=0.1)
	tape = mat.make("torch_tape", (0.50, 0.47, 0.40), pattern="cloth", rough=0.85, bump=0.4, edge=0.15)
	chrome = mat.make("torch_chrome", (0.62, 0.60, 0.55), pattern="plain", rough=0.3, var=0.05, grime=0.2, edge=0.3, bump=0.05)
	lens = mat.make("torch_lens", (0.72, 0.66, 0.50), pattern="plain", rough=0.15, var=0.05, grime=0.0, edge=0.2, bump=0.0)
	cord = mat.make("torch_cord", (0.45, 0.30, 0.22), pattern="knit", rough=0.9, bump=0.3)
	glint = mat.get("lamp_glow")

	parts = []
	L = 0.36
	z0 = -L * 0.5
	# Tail cap, body, grip rings, flared head, bezel and lens, along +Z.
	parts.append(geo.lathe([(0.0, 0.0), (0.030, 0.0), (0.038, 0.006), (0.040, 0.030), (0.036, 0.034)], (0, 0, z0), mat=black, segments=14, name="tail"))
	parts.append(geo.cylinder(0.041, 0.19, (0, 0, z0 + 0.034 + 0.095), mat=yellow, segments=14, name="body"))
	for k in range(4):
		parts.append(geo.cylinder(0.044, 0.012, (0, 0, z0 + 0.06 + k * 0.026), mat=black, segments=14, name="grip_ring"))
	parts.append(geo.lathe([(0.041, 0.0), (0.045, 0.02), (0.062, 0.07), (0.066, 0.10), (0.064, 0.11)], (0, 0, z0 + 0.22), mat=yellow, segments=16, name="head"))
	parts.append(geo.lathe([(0.064, 0.0), (0.070, 0.004), (0.070, 0.020), (0.058, 0.024), (0.056, 0.016)], (0, 0, z0 + 0.325), mat=black, segments=16, name="bezel"))
	parts.append(geo.cylinder(0.057, 0.006, (0, 0, z0 + 0.338), mat=chrome, segments=16, name="reflector"))
	parts.append(geo.lathe([(0.056, 0.0), (0.03, 0.005), (0.0, 0.007)], (0, 0, z0 + 0.341), mat=lens, segments=16, name="lens"))
	parts.append(geo.blob((0.013, 0.008, 0.003), (-0.022, 0.012, z0 + 0.348), (0, 0, 30), mat=glint, subdivisions=1, name="glint"))
	parts.append(geo.blob((0.006, 0.004, 0.003), (-0.006, 0.026, z0 + 0.348), mat=glint, subdivisions=1, name="glint"))
	# Slide switch on top (local -X is up once the torch lies down).
	parts.append(geo.box((0.016, 0.034, 0.05), (-0.042, 0, z0 + 0.20), mat=black, bevel=0.005, bevel_segments=1, name="switch_base"))
	parts.append(geo.box((0.014, 0.022, 0.018), (-0.051, 0, z0 + 0.212), mat=black, bevel=0.004, bevel_segments=1, name="switch"))
	# Where his thumb and palm rubbed the paint away.
	for (a, z, s) in ((180.0, 0.17, (0.008, 0.03, 0.035)), (200.0, 0.205, (0.007, 0.02, 0.02)), (150.0, 0.14, (0.007, 0.018, 0.025))):
		r = 0.039
		ra = math.radians(a)
		parts.append(geo.blob(s, (math.cos(ra) * r, math.sin(ra) * r, z0 + z), (0, 0, a), mat=worn, subdivisions=1, name="worn"))
	# A crack in the body, bound with cloth tape, the loose end lifting.
	parts.append(geo.cylinder(0.0450, 0.034, (0, 0, z0 + 0.187), (5, 0, 0), mat=tape, segments=14, name="tape"))
	parts.append(geo.box((0.024, 0.003, 0.028), (0.028, -0.036, z0 + 0.19), (0, 0, 40), mat=tape, name="tape_end"))
	# Wrist cord through the tail eyelet.
	parts.append(geo.cylinder(0.010, 0.012, (0.0, 0.0, z0 - 0.004), (90, 0, 0), mat=black, segments=8, name="eyelet"))
	loop = [(0.030 * math.sin(a / 10.0 * math.tau), 0.0, z0 - 0.006 - 0.035 * (1.0 - math.cos(a / 10.0 * math.tau))) for a in range(11)]
	parts.append(geo.tube(loop, 0.006, mat=cord, segments=4, name="cord"))

	# Lie it down along X, lens a little raised, floating at pickup height.
	settle(parts, (0, 78, 20), (0.0, 0.0, 0.35))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("body", "head", "tail", "bezel")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=3)

	return pipeline.static("relic_torch", parts, size=256)
