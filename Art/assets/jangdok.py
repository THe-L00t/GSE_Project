# Jangdokdae: a low stone platform of onggi jars; one lid gone and rain standing inside, one jar cracked.
import math

from gse import geo, mat, pipeline


def jar_profile(h, r):
	return [(0.56 * r, 0.0), (0.74 * r, 0.07 * h), (0.92 * r, 0.28 * h), (r, 0.52 * h), (0.96 * r, 0.68 * h),
			(0.80 * r, 0.82 * h), (0.58 * r, 0.94 * h), (0.60 * r, h)]


def radius_at(profile, z):
	for (r0, z0), (r1, z1) in zip(profile, profile[1:]):
		if z0 <= z <= z1:
			t = (z - z0) / max(z1 - z0, 1e-5)
			return r0 + (r1 - r0) * t
	return profile[-1][0]


def build():
	onggi = mat.get("onggi")
	stone = mat.get("stone")
	cement = mat.make("jang_cement", (0.44, 0.43, 0.40), pattern="concrete", rough=0.93, bump=0.35)
	water = mat.make("jang_rain", (0.09, 0.15, 0.16), pattern="plain", rough=0.08, var=0.05, grime=0.0, edge=0.0, bump=0.02)
	crack = mat.make("jang_crack", (0.07, 0.05, 0.04), pattern="plain", rough=0.9, grime=0.0, edge=0.0, bump=0.0)
	moss = mat.get("moss")
	moss_dark = mat.get("moss_dark")
	leaf = mat.get("leaf")
	leaf_dark = mat.get("leaf_dark")

	parts = []
	man_made = []

	# Platform: a mortared stone base with a cement skim on top.
	base = geo.box((1.80, 1.20, 0.21), (0, 0, 0.105), mat=stone, bevel=0.04, bevel_segments=1, name="base", subdiv=1)
	geo.jitter(base, 0.012, 3.0, seed=1)
	top = geo.box((1.74, 1.14, 0.05), (0, 0, 0.225), mat=cement, bevel=0.015, bevel_segments=1, name="top", subdiv=1)
	geo.jitter(top, 0.006, 3.0, seed=2, axes=(1, 1, 0.4))
	parts += [base, top]
	man_made += [base, top]
	rnd = geo.rng(11)
	for (x, y, s) in ((-0.95, -0.55, 0.10), (0.70, -0.66, 0.08), (0.96, 0.30, 0.09), (-0.30, 0.66, 0.07), (0.25, -0.67, 0.06)):
		st = geo.blob((s * 1.3, s, s * 0.7), (x, y, s * 0.4), (0, 0, rnd.uniform(0, 180)), mat=stone, lumpy=0.25, seed=int(x * 10 + y * 7), subdivisions=1, name="stone")
		parts.append(st)
		man_made.append(st)

	# Jars: big ones at the back, smaller to the front, as they were always lined up.
	tz = 0.25
	jars = [
		(-0.52, 0.26, 0.74, 0.31, "lid"),
		(0.18, 0.30, 0.80, 0.33, "open"),
		(0.66, 0.08, 0.56, 0.24, "cracked"),
		(-0.12, -0.24, 0.55, 0.23, "lid"),
		(-0.62, -0.30, 0.38, 0.17, "lid"),
		(0.34, -0.33, 0.34, 0.15, "lid"),
	]
	for i, (x, y, h, r, kind) in enumerate(jars):
		prof = jar_profile(h, r)
		if kind == "open":
			lip = prof[-1][0]
			prof = prof + [(lip - 0.025, h), (lip - 0.03, h - 0.07)]
		seg = 14 if r > 0.2 else 12
		body = geo.lathe(prof, (x, y, tz), mat=onggi, segments=seg, name="jar")
		parts.append(body)
		man_made.append(body)
		# A pale band where the glaze was wiped, like a hand-painted mark.
		band = geo.lathe([(radius_at(prof, 0.66 * h) + 0.004, 0.64 * h), (radius_at(prof, 0.7 * h) + 0.004, 0.7 * h)],
						 (x, y, tz), mat=mat.make("jang_band", (0.36, 0.24, 0.15), pattern="onggi", rough=0.4, bump=0.1),
						 segments=seg, name="band", close_top=False, close_bottom=False)
		parts.append(band)
		if kind == "open":
			lip = prof[-3][0]
			wd = geo.cylinder(lip - 0.03, 0.01, (x, y, tz + h - 0.045), mat=water, segments=16, name="rain")
			parts.append(wd)
			ring = [((lip - 0.015) * math.cos(a / 12.0 * math.tau) + x, (lip - 0.015) * math.sin(a / 12.0 * math.tau) + y, tz + h + 0.005) for a in range(13)]
			mr = geo.tube(ring, 0.028, mat=moss, segments=5, name="lip_moss")
			geo.tag(mr, "pulse")
			parts.append(mr)
			for k in range(3):
				a = k * 2.1 + 0.4
				fl = geo.blob((0.05, 0.03, 0.006), (x + math.cos(a) * 0.08, y + math.sin(a) * 0.08, tz + h - 0.035), (0, 0, math.degrees(a)), mat=leaf, subdivisions=1, name="float_leaf")
				geo.tag(fl, "sway", "foliage")
				parts.append(fl)
			# The lid lies on the platform where it fell, tipped against the jar.
			# The lid slid off the platform and leans against its side, moss starting in its bowl.
			lid = geo.lathe([(0.0, 0.0), (0.09 * r, 0.0), (0.09 * r, 0.03), (0.16 * r, 0.04), (0.46 * r, 0.07), (0.62 * r, 0.11), (0.58 * r, 0.11), (0.44 * r, 0.085), (0.12 * r, 0.06)],
							(0.98, -0.18, 0.15), (0, 68, -10), mat=onggi, segments=12, name="lid_fallen", close_bottom=False)
			parts.append(lid)
			man_made.append(lid)
			lm = geo.blob((0.09, 0.05, 0.11), (1.01, -0.20, 0.10), mat=moss_dark, lumpy=0.3, seed=9, subdivisions=2, name="lid_moss")
			geo.tag(lm, "pulse")
			parts.append(lm)
		else:
			lid = geo.lathe([(0.66 * r, 0.0), (0.64 * r, 0.02), (0.50 * r, 0.06), (0.18 * r, 0.085), (0.10 * r, 0.09), (0.10 * r, 0.12), (0.0, 0.13)],
							(x, y, tz + h - 0.005), (rnd.uniform(-3, 3), rnd.uniform(-3, 3), 0), mat=onggi, segments=12, name="lid")
			parts.append(lid)
			man_made.append(lid)
		if kind == "cracked":
			# A crack runs from the lip down the front; a shard broke off and lies at the foot.
			pts = []
			a0 = math.radians(-55.0)
			for k, zf in enumerate((0.99, 0.86, 0.74, 0.62, 0.50, 0.40, 0.30)):
				a = a0 + (0.12 if k % 2 else -0.08)
				rr = radius_at(prof, zf * h) + 0.006
				pts.append((x + math.cos(a) * rr, y + math.sin(a) * rr, tz + zf * h))
			c = geo.tube(pts, 0.011, mat=crack, segments=4, name="crack")
			parts.append(c)
			pts2 = [pts[3], (pts[3][0] + 0.05, pts[3][1] + 0.03, pts[3][2] - 0.05), (pts[3][0] + 0.07, pts[3][1] + 0.08, pts[3][2] - 0.11)]
			c2 = geo.tube(pts2, 0.009, mat=crack, segments=4, name="crack2")
			parts.append(c2)
			shard = geo.box((0.09, 0.06, 0.012), (x + 0.02, y - 0.34, tz + 0.01), (4, -6, 35), mat=onggi, name="shard")
			parts.append(shard)
			sh2 = geo.box((0.05, 0.04, 0.01), (x + 0.16, y - 0.30, tz + 0.008), (0, 5, -20), mat=onggi, name="shard")
			parts.append(sh2)

	# Moss: cushions on two lids and spilling over the platform's back and side edges.
	cushions = [
		((-0.52, 0.26, tz + 0.74 + 0.10), (0.14, 0.12, 0.04), moss, 1),
		((-0.62, -0.30, tz + 0.38 + 0.08), (0.08, 0.07, 0.03), moss_dark, 2),
		((-0.86, 0.40, 0.20), (0.10, 0.22, 0.12), moss, 3),
		((0.40, 0.60, 0.21), (0.30, 0.08, 0.10), moss_dark, 4),
		((-0.92, -0.20, 0.12), (0.07, 0.26, 0.12), moss_dark, 5),
		((0.88, -0.50, 0.08), (0.14, 0.12, 0.08), moss, 6),
		((-0.40, 0.62, 0.10), (0.22, 0.07, 0.10), moss, 7),
	]
	for (loc, rad, m, seed) in cushions:
		b = geo.blob(rad, loc, mat=m, lumpy=0.3, seed=seed, subdivisions=2, name="moss")
		geo.tag(b, "pulse")
		parts.append(b)

	# Grass and weeds round the foot of the platform, and a few between the jars.
	spots = [(-0.95, -0.62), (-0.50, -0.68), (0.10, -0.70), (0.55, -0.70), (0.98, -0.20), (0.98, 0.45),
			 (0.50, 0.68), (-0.20, 0.68), (-0.98, 0.10), (-0.98, 0.58)]
	for (cx, cy) in spots:
		for k in range(3):
			hgt = rnd.uniform(0.18, 0.40)
			w = geo.cone(rnd.uniform(0.035, 0.06), hgt, (cx + rnd.uniform(-0.1, 0.1), cy + rnd.uniform(-0.08, 0.08), hgt * 0.5),
						 (rnd.uniform(-18, 18), rnd.uniform(-18, 18), 0), mat=leaf if k % 2 else leaf_dark, segments=4, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)
	for (cx, cy) in ((0.42, 0.02), (-0.36, -0.02), (0.02, -0.50)):
		for k in range(2):
			hgt = rnd.uniform(0.12, 0.22)
			w = geo.cone(0.03, hgt, (cx + rnd.uniform(-0.05, 0.05), cy + rnd.uniform(-0.05, 0.05), tz + hgt * 0.5),
						 (rnd.uniform(-20, 20), rnd.uniform(-20, 20), 0), mat=leaf, segments=4, name="weed")
			geo.tag(w, "sway", "foliage")
			parts.append(w)
	# A broad-leafed butterbur plant at one corner.
	for k in range(4):
		a = k * 1.6
		lf = geo.blob((0.13, 0.11, 0.015), (-1.02 + math.cos(a) * 0.10, -0.50 + math.sin(a) * 0.10, 0.20 + k * 0.02),
					  (math.cos(a) * 18, math.sin(a) * 18, math.degrees(a)), mat=leaf_dark, subdivisions=1, name="butterbur")
		geo.tag(lf, "sway", "foliage")
		parts.append(lf)

	for i, p in enumerate(man_made):
		geo.moss_mask(p, seed=i + 2)

	res = pipeline.static("jangdok", parts, size=512)
	res["specular"] = 0.4
	return res
