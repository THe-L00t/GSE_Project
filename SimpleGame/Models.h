#pragma once

#include <vector>

#include "Mesh.h"

enum ModelId
{
	MODEL_PLAYER,
	MODEL_PIPE,
	MODEL_PIPE_PICKUP,
	MODEL_SLEEPER,
	MODEL_SLEEPER_ELDER,
	MODEL_DREAM_MOTE,
	MODEL_LETTER,
	MODEL_HOUSE_A,
	MODEL_HOUSE_B,
	MODEL_TREE,
	MODEL_PINE,
	MODEL_WELL,
	MODEL_TRUCK,
	MODEL_FENCE_POST,
	MODEL_FENCE_RAIL,
	MODEL_SIGN,
	MODEL_ROCK,
	MODEL_BUSH,
	MODEL_RUIN,
	MODEL_CAR,
	MODEL_LANTERN,
	MODEL_MITE,
	MODEL_HUSK,
	MODEL_BOAR,
	MODEL_HERB,
	MODEL_WATER_FLASK,
	MODEL_RELIC,
	MODEL_SHADOW,
	MODEL_GROUND,
	MODEL_GRANDMA,
	MODEL_SHED,
	MODEL_GARDEN_BED,
	MODEL_VEGETABLE,
	MODEL_BERRY_RED,
	MODEL_BERRY_PALE,
	MODEL_DEER,
	MODEL_CANNED_FOOD,
	MODEL_BUS_STOP,
	MODEL_GINKGO,
	MODEL_POLE,
	MODEL_COUNT
};

const char* ModelName(int id);
ModelRecipe BuildRecipe(int id);

// Reads every model from Cache/Models. A model is rebuilt and rewritten only when its
// file is missing or was written from a different recipe.
void LoadModelMeshes(std::vector<MeshData>& meshes, int& builtCount);
