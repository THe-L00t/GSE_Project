# The same shrub hung with pale whitish berries, the kind not to eat.
from assets import bush
from gse import mat


def build():
	soft = bush.leaves()
	return bush.finish("berry_pale", soft, bush.berries(soft, mat.get("berry_pale"), seed=23))
