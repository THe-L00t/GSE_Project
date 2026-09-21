#include "stdafx.h"
#include "SceneGraph.h"

#include <algorithm>

void SceneGraph::Draw(Renderer* renderer, const Vec3& viewCenter) const
{
	DrawContext dc;
	dc.renderer = renderer;
	dc.viewCenter = viewCenter;

	for (int layer = 0; layer < LAYER_COUNT; ++layer)
		DrawActor(*root, dc, layer);
}

void SceneGraph::DrawActor(const Actor& actor, const DrawContext& dc, int layer)
{
	if (actor.IsDestroyed() || !actor.IsDrawn(dc)) return;

	if (actor.layer == layer) actor.Draw(dc);

	for (size_t i = 0; i < actor.ChildCount(); ++i)
		DrawActor(*actor.Child(i), dc, layer);
}

void SceneGraph::RemoveDestroyed()
{
	RemoveDestroyedBelow(*root);
}

void SceneGraph::RemoveDestroyedBelow(Actor& actor)
{
	std::vector<std::unique_ptr<Actor>>& children = actor.children;
	children.erase(std::remove_if(children.begin(), children.end(),
		[](const std::unique_ptr<Actor>& child) { return child->IsDestroyed(); }), children.end());

	for (size_t i = 0; i < children.size(); ++i)
		RemoveDestroyedBelow(*children[i]);
}

Actor* SceneGraph::FindNearest(const Actor* under, int kind, const Vec3& pos, float range)
{
	std::vector<Actor*> candidates;
	Collect(under, kind, candidates);

	Actor* nearest = nullptr;
	float best = range;
	for (size_t i = 0; i < candidates.size(); ++i)
	{
		float d = DistXZ(pos, candidates[i]->WorldPosition());
		if (d < best)
		{
			best = d;
			nearest = candidates[i];
		}
	}
	return nearest;
}
