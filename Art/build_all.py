# Rebuilds every asset in Art/assets, each in its own Blender process so one failure stops nothing else,
# then the ground layers.
#   blender -b --factory-startup --python Art/build_all.py [-- name ...]
import os
import subprocess
import sys

import bpy

ART = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, ART)

from gse import core


def main():
	names = core.args_after_dashes()
	if not names:
		names = sorted(f[:-3] for f in os.listdir(os.path.join(ART, "assets")) if f.endswith(".py") and not f.startswith("_"))
	blender = bpy.app.binary_path
	failed = []
	for n in names:
		cmd = [blender, "-b", "--factory-startup", "--python", os.path.join(ART, "build.py"), "--", n, "--no-blend"]
		run = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
		ok = "GSE_BUILT " + n in run.stdout
		print(("ok    " if ok else "FAIL  ") + n)
		if not ok:
			failed.append(n)
			print(run.stdout[-2000:])
			print(run.stderr[-2000:])
	if not core.args_after_dashes():
		subprocess.run([blender, "-b", "--factory-startup", "--python", os.path.join(ART, "ground.py")])
	print("GSE_BUILD_ALL failed=%d %s" % (len(failed), " ".join(failed)))


main()
