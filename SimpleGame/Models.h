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
	MODEL_MEAT,
	MODEL_COOKED_MEAT,
	MODEL_WISP,
	MODEL_MOTE,
	MODEL_JANGDOK,        // earthenware jars on their stone stand
	MODEL_PYEONGSANG,     // low wooden platform bench
	MODEL_TILLER,         // two-wheeled power tiller
	MODEL_GATE,           // blue iron gate between wall stubs
	MODEL_GREENHOUSE,     // vinyl greenhouse hoops
	MODEL_RELIC_TORCH,    // the eight relics, in RelicId order
	MODEL_RELIC_PEACHES,
	MODEL_RELIC_WATCH,
	MODEL_RELIC_RADIO,
	MODEL_RELIC_TICKET,
	MODEL_RELIC_PHOTO,
	MODEL_RELIC_PHONE,
	MODEL_RELIC_BADGE,
	MODEL_OFFICE_4F,      // the old town around the start of Route 32
	MODEL_VILLA_3F,
	MODEL_SHOP_2F,
	MODEL_STORE_1F,
	MODEL_COLLAPSED_BUILDING,
	MODEL_EMPTY_HOUSE,    // what is left further out
	MODEL_HOUSE_SITE,
	MODEL_BLOCK_WALL,
	MODEL_GUARDRAIL,
	MODEL_STREETLAMP,
	MODEL_TRAFFIC_LIGHT,
	MODEL_ROAD_SIGN,
	MODEL_WATER,          // a pond's surface: the oval with its wandering shore, scaled to each pond
	MODEL_REEDS,          // a clump of reeds at the water's edge
	MODEL_PYLON,          // the transmission tower on the ridge, from where the great tree shows
	MODEL_GREAT_TREE,     // the tree at the root, drawn at full size
	MODEL_DOHYEON,        // the collector of Ginkgo Town
	MODEL_YEOUL,          // the child who grew up in the spores
	MODEL_COUNT
};

// How far a model reaches around and above its origin, in its own units, when it could hide the player.
// Zero for models that are never cut away to show what stands behind them.
struct OccluderSize
{
	float radius;
	float height;
};
OccluderSize ModelOccluder(int id);

const char* ModelName(int id);
ModelRecipe BuildRecipe(int id);

// Reads every model from Cache/Models. A model is rebuilt and rewritten only when its
// file is missing or was written from a different recipe.
void LoadModelMeshes(std::vector<MeshData>& meshes, int& builtCount);
