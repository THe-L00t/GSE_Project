# A pickup: the boar haunch roasted. Glazed browns with dark grill marks across the top, crisp golden
# fat round the cut, toasted bone, and a faint warm glow as if just off the fire.
import math

from assets import meat
from gse import geo, mat, pipeline


def build():
	m = {
		"meat": mat.make("roast_glaze", (0.42, 0.22, 0.11), pattern="onggi", rough=0.35, bump=0.25, edge=0.25, emit=0.1),
		"fat": mat.make("roast_fat", (0.68, 0.47, 0.22), pattern="skin", rough=0.4, bump=0.2, emit=0.1),
		"streak": mat.make("roast_streak", (0.56, 0.33, 0.14), pattern="skin", rough=0.4, bump=0.1, emit=0.1),
		"bone": mat.make("roast_bone", (0.80, 0.72, 0.58), pattern="plain", rough=0.6, bump=0.15),
		"hide": mat.make("roast_crackling", (0.30, 0.16, 0.08), pattern="fur", rough=0.6, bump=0.5),
	}
	char = mat.make("grill_char", (0.12, 0.07, 0.05), pattern="plain", rough=0.8, bump=0.2, grime=0.0)
	marks = []
	for x0 in (-0.05, 0.015, 0.08, 0.14):
		pts = [meat.surface(x0 + t * 0.03, math.pi * 0.5 + t * 1.05, -0.003) for t in (-1.0, -0.5, 0.0, 0.5, 1.0)]
		marks.append(geo.tube(pts, 0.0085, mat=char, segments=5, name="grill"))
	parts = meat.haunch(m, cap=False, extra=marks)
	for p in parts:
		geo.tag(p, "spin", "hover")
		if p.name.startswith(("body", "fat_rim")):
			geo.tag(p, "pulse")
	return pipeline.organic("cooked_meat", parts, voxel=0.007, tris=1300, size=256)
