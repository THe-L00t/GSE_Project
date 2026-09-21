#pragma once

#include <memory>
#include <vector>

#include "Math3D.h"

class Renderer;

enum ActorKind
{
	ACTOR_NODE,
	ACTOR_CAMERA,
	ACTOR_PLAYER,
	ACTOR_WEAPON,
	ACTOR_GROUND,
	ACTOR_WATER,
	ACTOR_PROP,
	ACTOR_SLEEPER,
	ACTOR_LETTER,
	ACTOR_EXIT,
	ACTOR_SAFE_POINT,
	ACTOR_LANTERN,
	ACTOR_CHUNK,
	ACTOR_ENEMY,
	ACTOR_ITEM
};

// Drawn one pass per layer, so every ground is down before the blob shadows that sit on it.
enum RenderLayer
{
	LAYER_GROUND,
	LAYER_WATER,
	LAYER_OBJECT,
	LAYER_COUNT
};

enum ColliderShape
{
	COLLIDER_NONE,
	COLLIDER_BOX,
	COLLIDER_CIRCLE
};

// Footprint on the XZ plane, centred on the actor.
struct Collider
{
	int   shape = COLLIDER_NONE;
	float halfX = 0.0f;
	float halfZ = 0.0f;
	float radius = 0.0f;
};

struct DrawContext
{
	Renderer* renderer = nullptr;
	Vec3      viewCenter;
};

class Actor
{
public:
	explicit Actor(int actorKind);
	virtual ~Actor();

	Actor(const Actor&) = delete;
	Actor& operator=(const Actor&) = delete;

	int    Kind() const { return kind; }
	Actor* Parent() const { return parent; }
	size_t ChildCount() const { return children.size(); }
	Actor* Child(size_t index) const { return children[index].get(); }

	template <typename T>
	T* AddChild(T* child)
	{
		Attach(std::unique_ptr<Actor>(child));
		return child;
	}

	// The actor and its subtree leave when the scene graph next removes destroyed actors.
	void Destroy() { destroyed = true; }
	bool IsDestroyed() const { return destroyed; }

	const Vec3& Position() const { return position; }
	float       Yaw() const { return yaw; }
	const Vec3& Scale() const { return scale; }
	void SetPosition(const Vec3& p);
	void SetYaw(float angle);
	void SetScale(const Vec3& s);
	void SetPitch(float angle, const Vec3& around);

	const Mat4& WorldMatrix() const;
	Vec3        WorldPosition() const;

	virtual bool IsDrawn(const DrawContext& dc) const { return visible; }
	virtual void Draw(const DrawContext& dc) const {}

	bool     visible = true;
	int      layer = LAYER_OBJECT;
	Collider collider;

private:
	void Attach(std::unique_ptr<Actor> child);
	void MarkDirty();

	friend class SceneGraph;

	int    kind = ACTOR_NODE;
	Actor* parent = nullptr;
	std::vector<std::unique_ptr<Actor>> children;
	bool   destroyed = false;

	Vec3  position;
	float yaw = 0.0f;
	float pitch = 0.0f;
	Vec3  pivot;
	Vec3  scale{ 1.0f, 1.0f, 1.0f };

	mutable Mat4 world;
	mutable bool dirty = true;
};
