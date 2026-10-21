# The village shrub with clusters of glossy red berries; picking swaps it for the plain bush.
from assets import bush
from gse import mat


def build():
	soft = bush.leaves()
	return bush.finish("berry_red", soft, bush.berries(soft, mat.get("berry_red"), seed=11))
