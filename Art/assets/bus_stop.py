# A rural bus shelter: steel frame on a cracked concrete pad, back wall with a faded timetable,
# a bench with one slat fallen, moss on the roof and vines down the right side, the route map on the
# outside of the back. The round bus sign for route 32 stands apart at the front right corner.
# The open side faces -Y.
import math

import bmesh

from gse import geo, mat, pipeline


def flat_text(body, size, loc, rot, m, angle=16.0):
	# Painted lettering: geo.text, then only the front faces kept and the curves thinned
	# (the full extruded glyphs cost thousands of triangles).
	obj = geo.text(body, size, loc, rot, m, depth=0.004)
	bm = bmesh.new()
	bm.from_mesh(obj.data)
	bm.normal_update()
	bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.normal.z < 0.5], context='FACES')
	bmesh.ops.dissolve_limit(bm, angle_limit=math.radians(angle), use_dissolve_boundaries=False, verts=bm.verts[:], edges=bm.edges[:])
	bmesh.ops.triangulate(bm, faces=bm.faces[:])
	bm.to_mesh(obj.data)
	bm.free()
	return obj


def build():
	frame = mat.make("busstop_frame", (0.27, 0.40, 0.38), pattern="rust", rough=0.7, bump=0.35, amount=0.7)
	panel = mat.make("busstop_panel", (0.52, 0.51, 0.45), pattern="plaster", rough=0.9, bump=0.3)
	roof_mat = mat.make("busstop_roof", (0.27, 0.33, 0.34), pattern="slate", rough=0.75, bump=0.5)
	sign_blue = mat.make("busstop_sign", (0.22, 0.36, 0.50), pattern="rust", rough=0.6, bump=0.3, amount=0.4)
	sign_face = mat.make("busstop_sign_face", (0.72, 0.70, 0.62), pattern="rust", rough=0.6, bump=0.2, amount=0.3)
	letter = mat.make("busstop_letter", (0.14, 0.22, 0.34), rough=0.6, bump=0.05)
	sheet = mat.make("busstop_timetable", (0.68, 0.66, 0.58), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	ink = mat.make("busstop_ink", (0.24, 0.25, 0.27), rough=0.9, bump=0.0, edge=0.0)
	poster = mat.make("busstop_poster", (0.58, 0.44, 0.38), pattern="paper", rough=0.95, bump=0.1, edge=0.05)
	paint_red = mat.get("paint_red")
	concrete = mat.get("concrete")
	wood = mat.get("wood_grey")
	metal = mat.get("metal")
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")
	glow = mat.get("spore_glow")

	parts = []
	shell = []

	# Concrete pad, one front corner broken off and sunk.
	shell.append(geo.box((3.2, 1.5, 0.10), (0.0, 0.0, 0.05), mat=concrete, bevel=0.02, name="pad"))
	shell.append(geo.box((0.42, 0.34, 0.08), (-1.48, -0.72, 0.03), (4, -6, 18), mat=concrete, bevel=0.01, name="pad_chunk"))
	parts.append(geo.box((0.02, 0.9, 0.012), (0.35, -0.05, 0.101), (0, 0, 28), mat=mat.get("stone_dark"), name="pad_crack"))

	# Steel frame: four posts, a roof sloping back, a fascia board with the stop's name.
	for (x, y, top) in ((-1.5, -0.66, 2.40), (1.5, -0.66, 2.40), (-1.5, 0.66, 2.31), (1.5, 0.66, 2.31)):
		shell.append(geo.box((0.08, 0.08, top - 0.10), (x, y, 0.10 + (top - 0.10) * 0.5), mat=frame, name="post"))
	roof = geo.box((3.40, 1.72, 0.07), (0.0, -0.06, 2.42), (-4, 0, 0), mat=roof_mat, bevel=0.015, bevel_segments=1, name="roof")
	shell.append(roof)
	shell.append(geo.box((3.36, 0.05, 0.20), (0.0, -0.92, 2.36), mat=frame, name="fascia"))
	parts.append(flat_text("물안개 마을", 0.13, (-0.85, -0.947, 2.36), (90, 0, 0), sign_face))
	parts.append(geo.box((3.0, 0.05, 0.06), (0.0, -0.66, 2.30), mat=frame, name="beam_front"))

	# Back wall with a timetable and a poster peeling off; a half side wall on the left.
	wall = geo.box((3.04, 0.05, 2.20), (0.0, 0.66, 1.20), mat=panel, name="back_wall")
	shell.append(wall)
	shell.append(geo.box((0.05, 0.80, 1.70), (-1.5, 0.24, 1.05), mat=panel, name="side_wall"))
	shell.append(geo.box((0.06, 0.80, 0.06), (-1.5, 0.24, 1.92), mat=frame, name="side_rail"))
	parts.append(geo.box((0.96, 0.025, 0.72), (-0.62, 0.628, 1.30), mat=sheet, name="timetable"))
	parts.append(geo.box((1.00, 0.02, 0.76), (-0.62, 0.632, 1.30), mat=frame, name="timetable_frame"))
	parts.append(flat_text("시간표", 0.11, (-0.62, 0.612, 1.56), (90, 0, 0), ink))
	for k in range(6):
		z = 1.42 - k * 0.085
		parts.append(geo.box((0.80, 0.006, 0.012), (-0.62, 0.614, z), mat=ink, name="timetable_rule"))
		for c in range(2):
			if (k + c) % 3 != 2:
				parts.append(geo.box((0.14, 0.006, 0.035), (-0.84 + c * 0.40, 0.613, z - 0.04), mat=ink, name="timetable_time"))
	parts.append(geo.box((0.40, 0.012, 0.52), (0.62, 0.630, 1.30), (0, 4, -6), mat=poster, name="poster"))
	parts.append(geo.box((0.30, 0.008, 0.08), (0.62, 0.622, 1.46), (0, 4, -6), mat=ink, name="poster_band"))

	# Outside on the back: the route map for the 32, faded, with the stops as dots along the line.
	parts.append(geo.box((1.50, 0.03, 0.78), (0.20, 0.698, 1.42), mat=frame, name="map_frame"))
	parts.append(geo.box((1.40, 0.02, 0.68), (0.20, 0.712, 1.42), mat=sheet, name="map_sheet"))
	parts.append(flat_text("32번 읍내 방면", 0.10, (0.20, 0.726, 1.66), (90, 0, 180), letter))
	route = [(0.78, 1.22), (0.50, 1.30), (0.30, 1.42), (0.02, 1.40), (-0.22, 1.50), (-0.40, 1.36)]
	parts.append(geo.tube([(x, 0.726, z) for (x, z) in route], 0.012, mat=sign_blue, segments=4, name="map_line"))
	for (x, z) in route:
		parts.append(geo.cylinder(0.03, 0.012, (x, 0.728, z), (90, 0, 0), mat=paint_red if (x, z) == route[2] else letter, segments=6, name="map_stop"))

	# Bench: two slats left, the front one fallen at one end.
	for (y, z) in ((0.36, 0.45), (0.48, 0.45)):
		parts.append(geo.box((2.0, 0.11, 0.04), (0.0, y, z), mat=wood, bevel=0.01, bevel_segments=1, name="slat"))
	parts.append(geo.box((2.0, 0.11, 0.04), (-0.08, 0.20, 0.30), (0, 13, 2), mat=wood, bevel=0.01, bevel_segments=1, name="slat_fallen"))
	parts.append(geo.box((2.0, 0.04, 0.12), (0.0, 0.61, 0.76), mat=wood, bevel=0.01, bevel_segments=1, name="backrest"))
	for x in (-0.85, 0.85):
		parts.append(geo.box((0.05, 0.36, 0.05), (x, 0.42, 0.41), mat=metal, name="bench_arm"))
		parts.append(geo.box((0.05, 0.05, 0.33), (x, 0.30, 0.265), mat=metal, name="bench_leg"))
		parts.append(geo.box((0.05, 0.05, 0.33), (x, 0.56, 0.265), mat=metal, name="bench_leg"))

	# The stop sign: a round "bus" disc on top of the pole and a route box "32" under it, lettered both sides.
	sx, sy = 1.80, -0.60
	parts.append(geo.cylinder(0.09, 0.10, (sx, sy, 0.05), mat=concrete, segments=10, name="sign_foot"))
	parts.append(geo.cylinder(0.035, 1.92, (sx, sy, 0.96), mat=metal, segments=8, name="sign_pole"))
	parts.append(geo.box((0.10, 0.06, 0.08), (sx, sy, 1.98), mat=metal, name="sign_clamp"))
	parts.append(geo.cylinder(0.28, 0.03, (sx, sy, 2.28), (90, 0, 0), mat=sign_blue, segments=20, name="sign_disc"))
	parts.append(geo.box((0.34, 0.08, 0.18), (sx, sy, 1.66), mat=sign_blue, bevel=0.01, bevel_segments=1, name="route_box"))
	for (side, rz) in ((-1.0, 0), (1.0, 180)):
		parts.append(geo.cylinder(0.225, 0.006, (sx, sy + side * 0.017, 2.28), (90, 0, 0), mat=sign_face, segments=20, name="sign_face"))
		parts.append(flat_text("버스", 0.15, (sx, sy + side * 0.022, 2.29), (90, 0, rz), letter))
		parts.append(geo.box((0.29, 0.006, 0.13), (sx, sy + side * 0.042, 1.66), mat=sign_face, name="route_face"))
		parts.append(flat_text("32", 0.12, (sx, sy + side * 0.047, 1.66), (90, 0, rz), letter))

	# Moss: a cushion over the roof, creeping in at the wall foot and the pad corners.
	m = geo.blob((1.10, 0.62, 0.12), (0.35, 0.00, 2.47), (-4, 0, 0), mat=moss, lumpy=0.3, seed=2, name="moss_roof")
	geo.tag(m, "pulse")
	parts.append(m)
	parts.append(geo.blob((0.55, 0.40, 0.08), (-0.95, -0.35, 2.48), (-4, 0, 0), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=3, name="moss_roof2"))
	parts.append(geo.blob((0.45, 0.10, 0.14), (1.05, -0.92, 2.32), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=4, name="moss_fascia"))
	parts.append(geo.blob((0.80, 0.18, 0.16), (0.40, 0.80, 0.10), mat=moss, subdivisions=2, lumpy=0.3, seed=5, name="moss_wallfoot"))
	parts.append(geo.blob((0.40, 0.30, 0.08), (-1.30, 0.55, 0.12), mat=moss_dark, subdivisions=2, lumpy=0.3, seed=6, name="moss_corner"))
	parts.append(geo.blob((0.30, 0.25, 0.06), (1.35, -0.55, 0.11), mat=moss, subdivisions=2, lumpy=0.3, seed=7, name="moss_corner2"))

	# Vines hanging off the roof's right edge and climbing the right posts.
	rnd = geo.rng(11)
	for (y, bottom) in ((-0.70, 0.85), (-0.35, 1.35), (0.05, 0.55), (0.42, 1.10), (0.70, 0.30)):
		pts = []
		steps = 6
		for k in range(steps + 1):
			t = k / float(steps)
			z = 2.38 - (2.38 - bottom) * t
			pts.append((1.56 + 0.03 * rnd.uniform(-1, 1) + 0.02 * t, y + 0.05 * rnd.uniform(-1, 1), z))
		vine = geo.tube(pts, [0.018] * steps + [0.008], mat=leaf_dark, segments=4, name="vine")
		geo.tag(vine, "sway", "foliage")
		parts.append(vine)
		for k in range(1, steps + 1):
			if rnd.random() < 0.6:
				p = pts[k]
				lf = geo.blob((0.07, 0.06, 0.05), (p[0] + 0.03, p[1] + rnd.uniform(-0.05, 0.05), p[2]), (rnd.uniform(0, 40), 0, rnd.uniform(0, 90)),
							  mat=leaf if rnd.random() < 0.6 else leaf_dark, subdivisions=1, lumpy=0.3, seed=k, name="vine_leaf")
				geo.tag(lf, "sway", "foliage")
				parts.append(lf)
	for (cx, cy, n) in ((1.56, -0.48, 7), (1.56, 0.30, 5)):
		for k in range(n):
			lf = geo.blob((0.11, 0.09, 0.08), (cx + rnd.uniform(-0.02, 0.06), cy + rnd.uniform(-0.12, 0.12), 2.35 - k * 0.05 + rnd.uniform(-0.04, 0.04)),
						  (0, 0, rnd.uniform(0, 90)), mat=leaf, subdivisions=1, lumpy=0.3, seed=20 + k, name="vine_clump")
			geo.tag(lf, "sway", "foliage")
			parts.append(lf)

	# Ivy climbing the back wall from the moss at its foot.
	for (x0, top) in ((1.30, 2.25), (1.05, 1.60)):
		pts = [(x0 + 0.06 * rnd.uniform(-1, 1), 0.70 + 0.008 * k, 0.10 + (top - 0.10) * k / 6.0) for k in range(7)]
		vine = geo.tube(pts, [0.02] * 6 + [0.008], mat=leaf_dark, segments=4, name="ivy")
		geo.tag(vine, "sway", "foliage")
		parts.append(vine)
		for k, p in enumerate(pts[1:]):
			lf = geo.blob((0.10, 0.045, 0.08), (p[0] + rnd.uniform(-0.06, 0.06), 0.73, p[2] + rnd.uniform(-0.04, 0.04)), (0, rnd.uniform(0, 90), 0),
						  mat=leaf if rnd.random() < 0.5 else leaf_dark, subdivisions=1, lumpy=0.3, seed=40 + k, name="ivy_leaf")
			geo.tag(lf, "sway", "foliage")
			parts.append(lf)

	# Grass along the pad edges.
	for (cx, cy) in ((-1.62, -0.30), (-0.80, -0.80), (0.70, -0.82), (1.64, 0.35), (-1.0, 0.82), (1.92, -0.48)):
		for k in range(4):
			h = rnd.uniform(0.2, 0.42)
			w = geo.cone(rnd.uniform(0.05, 0.08), h, (cx + rnd.uniform(-0.1, 0.1), cy + rnd.uniform(-0.06, 0.06), h * 0.5),
						 (rnd.uniform(-15, 15), rnd.uniform(-15, 15), 0), mat=leaf, segments=5, name="grass")
			geo.tag(w, "sway", "foliage")
			parts.append(w)

	for (loc, r) in (((0.55, -0.10, 2.60), 0.05), ((-1.25, 0.45, 0.22), 0.045)):
		g = geo.blob(r, loc, mat=glow, subdivisions=1, name="spore")
		geo.tag(g, "pulse")
		parts.append(g)

	for i, p in enumerate(shell):
		geo.moss_mask(p, seed=i + 1)
	parts += shell

	return pipeline.static("bus_stop", parts, size=512)
