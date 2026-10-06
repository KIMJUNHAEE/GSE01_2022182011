#include "stdafx.h"
#include "GameWorld.h"
#include <stdexcept>

namespace
{
    const Actor& RequiredActor(const SceneGraph& scene, ActorId id)
    {
        const Actor* actor = scene.Find(id);
        if (!actor)
        {
            throw std::logic_error("Required world actor is missing");
        }
        return *actor;
    }

    Actor& RequiredActor(SceneGraph& scene, ActorId id)
    {
        Actor* actor = scene.Find(id);
        if (!actor)
        {
            throw std::logic_error("Required world actor is missing");
        }
        return *actor;
    }
} // namespace

bool GameWorld::PlayerActive() const
{
    const Actor* actor = scene.Find(playerId);
    return actor && actor->IsActive();
}

Vec2 GameWorld::PlayerPosition() const
{
    return RequiredActor(scene, playerId).Position();
}

Vec2 GameWorld::CaptainPosition() const
{
    return RequiredActor(scene, captainId).Position();
}

void GameWorld::SetPlayerPosition(Vec2 position)
{
    RequiredActor(scene, playerId).SetPosition(position);
}

void GameWorld::SetCaptainPosition(Vec2 position)
{
    RequiredActor(scene, captainId).SetPosition(position);
}

Vec2 GameWorld::RelayPosition(int index) const
{
    return RequiredActor(scene, relays.at(index)).Position();
}

Vec2 GameWorld::MemoryPosition(int index) const
{
    return RequiredActor(scene, memories.at(index)).Position();
}

Vec2 GameWorld::ShipPosition() const
{
    return RequiredActor(scene, shipId).Position();
}

Vec2 GameWorld::CorePosition() const
{
    return RequiredActor(scene, coreId).Position();
}

Vec2 GameWorld::ResidentPosition() const
{
    return RequiredActor(scene, residentId).Position();
}
