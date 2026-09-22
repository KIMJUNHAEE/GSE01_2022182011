#include "stdafx.h"
#include "Actor.h"
#include <utility>

Actor::Actor(std::string name, Vec2 position, RenderLayer layer)
    : name(std::move(name)), localPosition(position), layer(layer)
{
}

Actor::Actor(const Actor& other)
    : name(other.name), localPosition(other.localPosition), layer(other.layer),
      active(other.active), visible(other.visible)
{
    // Clones are detached. SceneGraph reconnects hierarchy and assigns ownership/identity.
}

std::unique_ptr<Actor> Actor::Clone() const
{
    return std::make_unique<Actor>(*this);
}

void Actor::Update(float)
{
}

void Actor::Draw(ActorRenderer&) const
{
}

bool Actor::BlocksNavigation() const
{
    return false;
}

ActorId Actor::Id() const
{
    return id;
}

ActorId Actor::ParentId() const
{
    auto node = parent.lock();
    return node ? node->Id() : InvalidActor;
}

const std::string& Actor::Name() const
{
    return name;
}

std::vector<ActorId> Actor::Children() const
{
    std::vector<ActorId> result;
    for (const auto& child : children)
    {
        auto node = child.lock();
        if (node && !node->IsPendingDestroy())
        {
            result.push_back(node->Id());
        }
    }
    return result;
}

Vec2 Actor::LocalPosition() const
{
    return localPosition;
}

Vec2 Actor::Position() const
{
    auto node = parent.lock();
    return node ? node->Position() + localPosition : localPosition;
}

void Actor::SetLocalPosition(Vec2 position)
{
    if (position.x != localPosition.x || position.y != localPosition.y)
    {
        localPosition = position;
        InvalidateNavigation();
    }
}

void Actor::SetPosition(Vec2 position)
{
    auto node = parent.lock();
    SetLocalPosition(node ? position - node->Position() : position);
}

void Actor::SetActive(bool value)
{
    if (active != value)
    {
        active = value;
        InvalidateNavigation();
    }
}

void Actor::SetVisible(bool value)
{
    visible = value;
}

bool Actor::IsActive() const
{
    if (!active || pendingDestroy)
    {
        return false;
    }
    auto node = parent.lock();
    return !node || node->IsActive();
}

bool Actor::IsVisible() const
{
    if (!visible || !IsActive())
    {
        return false;
    }
    auto node = parent.lock();
    return !node || node->IsVisible();
}

bool Actor::IsPendingDestroy() const
{
    return pendingDestroy;
}

RenderLayer Actor::Layer() const
{
    return layer;
}

bool Actor::ContainsNavigationObstacle() const
{
    if (BlocksNavigation())
    {
        return true;
    }
    for (const auto& child : children)
    {
        auto node = child.lock();
        if (node && node->ContainsNavigationObstacle())
        {
            return true;
        }
    }
    return false;
}

void Actor::InvalidateNavigation()
{
    if (state && ContainsNavigationObstacle())
    {
        ++state->navigationRevision;
    }
}

void Actor::Destroy()
{
    if (pendingDestroy)
    {
        return;
    }
    InvalidateNavigation();
    pendingDestroy = true;
    for (const auto& child : children)
    {
        auto node = child.lock();
        if (node)
        {
            node->Destroy();
        }
    }
}
