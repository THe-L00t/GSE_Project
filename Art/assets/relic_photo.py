# Relic: the family photo in its little wooden stand frame. Four people squinting into the sun in front of
# the house that is a hill now: grandfather, young Sunim, and the parents. The print has faded from one
# corner; a pressed wildflower is tucked into the frame.
import math

from gse import geo, mat, pipeline

FW, FH, FD = 0.40, 0.30, 0.03
PW, PH = 0.33, 0.23


def flat(w, h, x, z, layer, m, name):
	# One layer of the print, stacked toward the viewer.
	return geo.box((w, 0.0012, h), (x, -0.004 - layer * 0.0007, z), mat=m, name=name)


def shape(outline, layer, m, name):
	# A cut-out layer of the print from an (x, z) outline.
	return geo.polygon_slab(outline, 0.0012, (0.0, -0.004 - layer * 0.0007 + 0.0006, 0.0), (90, 0, 0), mat=m, name=name)


def person(parts, x, height, shoulders, coat, legs, skin, hair, layer, lean=0.0):
	foot = -PH * 0.5 + 0.022
	body_h = height * 0.48
	top = foot + height * 0.30 + body_h
	parts.append(shape([(x - shoulders * 0.5 + lean, top), (x + shoulders * 0.5 + lean, top), (x + shoulders * 0.42, foot + height * 0.28),
						(x - shoulders * 0.42, foot + height * 0.28)], layer + 1, coat, "figure_body"))
	for s in (-1.0, 1.0):
		parts.append(flat(shoulders * 0.18, height * 0.30, x + s * shoulders * 0.2, foot + height * 0.15, layer, legs, "figure_leg"))
	head_r = height * 0.11
	parts.append(geo.cylinder(head_r, 0.0012, (x + lean * 1.2, -0.004 - (layer + 2) * 0.0007, top + head_r * 0.95), (90, 0, 0), mat=skin, segments=10, name="figure_head"))
	parts.append(geo.cylinder(head_r * 1.08, 0.0012, (x + lean * 1.2, -0.004 - (layer + 1) * 0.0007, top + head_r * 1.2), (90, 0, 0), mat=hair, segments=10, name="figure_hair"))


def settle(parts, rot, loc):
	for o in parts:
		geo.apply_modifiers(o)
		geo.apply_transform(o)
		o.rotation_euler = [math.radians(a) for a in rot]
		o.location = loc
		geo.apply_transform(o)


def build():
	wood = mat.make("photo_frame_wood", (0.52, 0.37, 0.24), pattern="wood", rough=0.6, var=0.12, grime=0.35, edge=0.3, bump=0.25, axis='X')
	back = mat.make("photo_back", (0.44, 0.35, 0.25), pattern="paper", rough=0.9, bump=0.15)
	brass = mat.make("photo_brass", (0.72, 0.60, 0.36), pattern="plain", rough=0.35, grime=0.3, edge=0.3, bump=0.0)
	sky = mat.make("photo_sky", (0.64, 0.71, 0.72), pattern="paper", rough=0.4, var=0.08, grime=0.0, edge=0.0, bump=0.02)
	fade = mat.make("photo_fade", (0.86, 0.82, 0.70), pattern="paper", rough=0.4, var=0.06, grime=0.0, edge=0.0, bump=0.02)
	hill = mat.make("photo_hill", (0.44, 0.54, 0.38), pattern="paper", rough=0.4, var=0.1, grime=0.0, edge=0.0, bump=0.02)
	hill_far = mat.make("photo_hill_far", (0.52, 0.60, 0.52), pattern="paper", rough=0.4, var=0.06, grime=0.0, edge=0.0, bump=0.02)
	yard = mat.make("photo_yard", (0.66, 0.58, 0.44), pattern="paper", rough=0.4, var=0.08, grime=0.0, edge=0.0, bump=0.02)
	wall = mat.make("photo_wall", (0.84, 0.80, 0.70), pattern="paper", rough=0.4, grime=0.0, edge=0.0, bump=0.02)
	roof = mat.make("photo_roof", (0.36, 0.42, 0.50), pattern="paper", rough=0.4, grime=0.0, edge=0.0, bump=0.02)
	door = mat.make("photo_door", (0.46, 0.34, 0.26), pattern="paper", rough=0.4, grime=0.0, edge=0.0, bump=0.02)
	skin = mat.make("photo_skin", (0.82, 0.66, 0.54), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	hair_d = mat.make("photo_hair", (0.20, 0.17, 0.15), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	hair_g = mat.make("photo_hair_grey", (0.46, 0.44, 0.42), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	jacket = mat.make("photo_jacket", (0.34, 0.33, 0.36), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	blouse = mat.make("photo_blouse", (0.74, 0.44, 0.40), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	shirt = mat.make("photo_shirt", (0.88, 0.86, 0.80), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	dress = mat.make("photo_dress", (0.84, 0.72, 0.42), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	trousers = mat.make("photo_trousers", (0.28, 0.28, 0.32), pattern="plain", rough=0.4, grime=0.0, edge=0.0, bump=0.0)
	petal = mat.make("photo_petal", (0.80, 0.76, 0.86), pattern="plain", rough=0.7, grime=0.1, edge=0.1, bump=0.0)
	stem = mat.make("photo_stem", (0.40, 0.46, 0.30), pattern="plain", rough=0.8, grime=0.0, edge=0.0, bump=0.0)

	parts = []
	# Frame: four bevelled rails, a backboard and a fold-out stand.
	b = (FW - PW) * 0.5
	parts.append(geo.box((FW, FD, b), (0, 0, FH * 0.5 - b * 0.5), mat=wood, bevel=0.008, bevel_segments=1, name="frame"))
	parts.append(geo.box((FW, FD, b), (0, 0, -FH * 0.5 + b * 0.5), mat=wood, bevel=0.008, bevel_segments=1, name="frame"))
	for s in (-1.0, 1.0):
		parts.append(geo.box((b, FD, FH - 2 * b + 0.002), (s * (FW * 0.5 - b * 0.5), 0, 0), mat=wood, bevel=0.008, bevel_segments=1, name="frame"))
	parts.append(geo.box((PW + 0.01, 0.006, PH + 0.01), (0, 0.006, 0), mat=back, name="backboard"))
	parts.append(geo.box((FW - 0.02, 0.006, FH - 0.02), (0, FD * 0.5 + 0.002, 0), mat=back, name="back"))
	parts.append(geo.box((0.06, 0.008, 0.20), (0, FD * 0.5 + 0.06, -0.05), (-28, 0, 0), mat=back, bevel=0.003, bevel_segments=1, name="stand"))
	parts.append(geo.cylinder(0.012, 0.006, (0, FD * 0.5 + 0.007, 0.07), (90, 0, 0), mat=brass, segments=8, name="hinge"))
	for (x, z) in ((-0.15, 0.10), (0.15, 0.10), (-0.15, -0.10), (0.15, -0.10)):
		parts.append(geo.box((0.03, 0.005, 0.01), (x, FD * 0.5 + 0.006, z), (0, 35, 0), mat=brass, name="turn_button"))

	# The print: sky, far and near hills, the yard, the house, then the four of them.
	parts.append(flat(PW, PH, 0, 0, 0, sky, "sky"))
	parts.append(shape([(-PW * 0.5, -0.01), (-0.10, 0.045), (0.02, 0.025), (0.12, 0.060), (PW * 0.5, 0.040), (PW * 0.5, -0.03), (-PW * 0.5, -0.03)], 1, hill_far, "hill_far"))
	parts.append(shape([(-PW * 0.5, -0.04), (-0.08, 0.005), (0.06, -0.02), (PW * 0.5, 0.010), (PW * 0.5, -0.06), (-PW * 0.5, -0.06)], 2, hill, "hill"))
	parts.append(flat(PW, 0.06, 0, -PH * 0.5 + 0.03, 3, yard, "yard"))
	hx = 0.03
	parts.append(flat(0.22, 0.085, hx, -0.020, 4, wall, "house_wall"))
	parts.append(shape([(hx - 0.135, 0.020), (hx + 0.135, 0.020), (hx + 0.10, 0.068), (hx - 0.10, 0.068)], 5, roof, "house_roof"))
	parts.append(flat(0.03, 0.05, hx + 0.085, -0.035, 5, door, "house_door"))
	parts.append(flat(0.03, 0.022, hx - 0.085, -0.005, 5, roof, "house_window"))
	parts.append(flat(0.03, 0.022, hx - 0.025, -0.005, 5, roof, "house_window"))
	person(parts, -0.115, 0.150, 0.050, jacket, trousers, skin, hair_g, 6)
	person(parts, -0.058, 0.128, 0.044, blouse, trousers, skin, hair_d, 6, lean=0.004)
	person(parts, 0.000, 0.152, 0.050, shirt, trousers, skin, hair_d, 10)
	person(parts, 0.054, 0.132, 0.044, dress, dress, skin, hair_d, 10, lean=-0.004)
	# Sun-faded from the top right corner, where the window light fell for years.
	parts.append(shape([(PW * 0.5, PH * 0.5), (PW * 0.5 - 0.075, PH * 0.5), (PW * 0.5, PH * 0.5 - 0.06)], 15, fade, "fade"))
	parts.append(shape([(PW * 0.5, PH * 0.5), (PW * 0.5 - 0.13, PH * 0.5), (PW * 0.5, PH * 0.5 - 0.10)], 14, mat.make("photo_fade_soft", (0.72, 0.74, 0.70), pattern="paper", rough=0.4, grime=0.0, edge=0.0, bump=0.02), "fade"))

	# A pressed wildflower tucked into the top left corner of the frame.
	fx, fz = -FW * 0.5 + 0.018, FH * 0.5 - 0.035
	parts.append(geo.tube([(fx + 0.004, -FD * 0.5 - 0.004, fz - 0.12), (fx - 0.002, -FD * 0.5 - 0.005, fz - 0.06), (fx, -FD * 0.5 - 0.006, fz)], 0.003, mat=stem, segments=4, name="stem"))
	parts.append(geo.blob((0.012, 0.003, 0.005), (fx + 0.008, -FD * 0.5 - 0.006, fz - 0.07), (0, -30, 0), mat=stem, subdivisions=1, name="stem_leaf"))
	for k in range(6):
		a = k / 6.0 * math.tau
		parts.append(geo.blob((0.016, 0.003, 0.008), (fx + math.cos(a) * 0.016, -FD * 0.5 - 0.008, fz + math.sin(a) * 0.016), (0, -math.degrees(a), 0), mat=petal, subdivisions=1, name="petal"))
	parts.append(geo.blob((0.008, 0.004, 0.008), (fx, -FD * 0.5 - 0.01, fz), mat=dress, subdivisions=1, name="flower_heart"))

	settle(parts, (-12, 0, 15), (0.0, 0.0, 0.35))
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("frame", "back", "stand")):
			geo.moss_mask(p, up=0.4, low=0.0, grain=0.35, seed=13)

	return pipeline.static("relic_photo", parts, size=256)
