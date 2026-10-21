# The pipe lying on the ground as a pickup: the same pipe as pipe.py laid along X, its middle at 0.4 m,
# spinning and hovering. The grip end is at -X, the elbow at +X.
import math

from mathutils import Matrix

from gse import geo, mat, pipeline


def make_pipe():
	steel = mat.make("pipe_steel", (0.35, 0.31, 0.28), pattern="rust", rough=0.75, bump=0.45, amount=1.0)
	iron = mat.make("pipe_iron", (0.25, 0.24, 0.23), pattern="rust", rough=0.7, bump=0.4, amount=0.7)
	tape = mat.make("pipe_tape", (0.58, 0.54, 0.46), pattern="cloth", rough=0.92, bump=0.5)
	tie = mat.make("pipe_tie", (0.40, 0.50, 0.44), pattern="cloth", rough=0.92, bump=0.45)
	moss = mat.get("moss")

	parts = []
	parts.append(geo.cylinder(0.035, 0.85, (0, 0, 0.325), mat=steel, segments=10, name="pipe"))
	parts.append(geo.cylinder(0.038, 0.016, (0, 0, -0.092), mat=iron, segments=10, name="pipe_lip"))
	parts.append(geo.cylinder(0.022, 0.004, (0, 0, -0.101), mat=mat.get("rubber"), segments=10, name="pipe_bore"))

	# Cloth tape: a sleeve with the turns standing proud, a loose tail, a teal strip tied at the top.
	parts.append(geo.cylinder(0.040, 0.22, (0, 0, 0.015), mat=tape, segments=10, name="tape"))
	turns = 6
	pts = []
	for k in range(turns * 8 + 1):
		t = k / float(turns * 8)
		a = t * turns * math.tau
		pts.append((math.cos(a) * 0.040, math.sin(a) * 0.040, -0.09 + 0.21 * t))
	parts.append(geo.tube(pts, 0.004, mat=tape, segments=4, name="tape_turns"))
	parts.append(geo.box((0.022, 0.005, 0.06), (0.03, -0.034, -0.105), (0, -12, -30), mat=tape, name="tape_tail"))
	parts.append(geo.cylinder(0.043, 0.03, (0, 0, 0.135), mat=tie, segments=10, name="tie"))
	parts.append(geo.box((0.02, 0.005, 0.06), (-0.035, -0.03, 0.11), (0, 15, 40), mat=tie, name="tie_end"))

	# Elbow fitting: a collar, a quarter bend, a threaded mouth facing -Y.
	parts.append(geo.cylinder(0.046, 0.05, (0, 0, 0.745), mat=iron, segments=10, name="collar"))
	bend = []
	for k in range(7):
		p = math.radians(90.0 * k / 6.0)
		bend.append((0.0, -0.07 + 0.07 * math.cos(p), 0.77 + 0.07 * math.sin(p)))
	bend.append((0.0, -0.10, 0.84))
	parts.append(geo.tube(bend, 0.043, mat=iron, segments=10, name="elbow"))
	parts.append(geo.cylinder(0.048, 0.04, (0, -0.10, 0.84), (90, 0, 0), mat=iron, segments=10, name="mouth"))
	parts.append(geo.cylinder(0.030, 0.004, (0, -0.121, 0.84), (90, 0, 0), mat=mat.get("rubber"), segments=10, name="mouth_bore"))

	# A tuft of moss caught in the bend.
	parts.append(geo.blob((0.03, 0.025, 0.02), (0.03, -0.035, 0.85), mat=moss, subdivisions=1, lumpy=0.3, seed=2, name="moss"))
	return parts


def build():
	parts = make_pipe()
	# Lay it down: the pipe's middle (z 0.39 when held) to the origin, its length onto X, then up to 0.4 m.
	lay = Matrix.Translation((0.0, 0.0, 0.40)) @ Matrix.Rotation(math.radians(90.0), 4, 'Y') @ Matrix.Translation((0.0, 0.0, -0.39))
	for p in parts:
		p.matrix_basis = lay @ p.matrix_basis
	for p in parts:
		if p.name.startswith(("pipe", "collar", "elbow")):
			geo.moss_mask(p, seed=1)
		geo.tag(p, "spin", "hover")
	return pipeline.static("pipe_pickup", parts, size=256)
