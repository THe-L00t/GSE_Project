#pragma once

#include <memory>
#include <vector>

#include "Actor.h"

class SceneGraph
{
public:
	Actor* Root() const { return root.get(); }

	// Pre-order, one pass per layer. An actor that is not drawn hides its subtree.
	void Draw(Renderer* renderer, const Vec3& viewCenter) const;

	void RemoveDestroyed();

	// Live actors of one kind below an actor, in tree order.
	template <typename T>
	static void Collect(const Actor* under, int kind, std::vector<T*>& out)
	{
		for (size_t i = 0; i < under->ChildCount(); ++i)
		{
			Actor* child = under->Child(i);
			if (child->IsDestroyed()) continue;
			if (child->Kind() == kind) out.push_back(static_cast<T*>(child));
			Collect(child, kind, out);
		}
	}

	// Nearest on the XZ plane within range; the first one found wins a tie.
	static Actor* FindNearest(const Actor* under, int kind, const Vec3& pos, float range);

private:
	static void DrawActor(const Actor& actor, const DrawContext& dc, int layer);
	static void RemoveDestroyedBelow(Actor& actor);

	std::unique_ptr<Actor> root{ new Actor(ACTOR_NODE) };
};
