#include "stdafx.h"
#include "Actor.h"

Actor::Actor(int actorKind)
	: kind(actorKind)
{
}

Actor::~Actor()
{
}

void Actor::Attach(std::unique_ptr<Actor> child)
{
	child->parent = this;
	child->MarkDirty();
	children.push_back(std::move(child));
}

void Actor::SetPosition(const Vec3& p)
{
	position = p;
	MarkDirty();
}

void Actor::SetYaw(float angle)
{
	yaw = angle;
	MarkDirty();
}

void Actor::SetScale(const Vec3& s)
{
	scale = s;
	MarkDirty();
}

void Actor::SetPitch(float angle, const Vec3& around)
{
	pitch = angle;
	pivot = around;
	MarkDirty();
}

void Actor::MarkDirty()
{
	// A dirty actor's subtree is already dirty.
	if (dirty) return;

	dirty = true;
	for (size_t i = 0; i < children.size(); ++i)
		children[i]->MarkDirty();
}

const Mat4& Actor::WorldMatrix() const
{
	if (dirty)
	{
		Mat4 local = Mul(MatTranslate(position), MatRotateY(yaw));
		if (pitch != 0.0f)
			local = Mul(local, Mul(Mul(MatTranslate(pivot), MatRotateX(pitch)), MatTranslate(-pivot)));
		local = Mul(local, MatScale(scale));

		world = parent ? Mul(parent->WorldMatrix(), local) : local;
		dirty = false;
	}
	return world;
}

Vec3 Actor::WorldPosition() const
{
	const Mat4& m = WorldMatrix();
	return Vec3(m.m[12], m.m[13], m.m[14]);
}
