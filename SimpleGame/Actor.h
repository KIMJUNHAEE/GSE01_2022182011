#pragma once
#include "WorldMath.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using ActorId = uint64_t;
constexpr ActorId InvalidActor = 0;

class ActorRenderer;
class SceneGraph;

enum class RenderLayer
{
    Terrain,
    GroundEffect,
    World,
    Projectile,
    Overlay
};

struct SceneState
{
    uint64_t navigationRevision = 1;
};

class Actor
{
public:

    explicit Actor(std::string name = "Group", Vec2 position = {},
                   RenderLayer layer = RenderLayer::World);
    Actor(const Actor& other);
    Actor& operator=(const Actor&) = delete;
    virtual ~Actor() = default;
    virtual std::unique_ptr<Actor> Clone() const;
    virtual void Update(float dt);
    virtual void Draw(ActorRenderer& renderer) const;
    virtual bool BlocksNavigation() const;

    ActorId Id() const;
    ActorId ParentId() const;
    const std::string& Name() const;
    std::vector<ActorId> Children() const;
    Vec2 LocalPosition() const;
    Vec2 Position() const;
    void SetLocalPosition(Vec2 position);
    void SetPosition(Vec2 position);
    void SetActive(bool active);
    void SetVisible(bool visible);
    bool IsActive() const;
    bool IsVisible() const;
    bool IsPendingDestroy() const;
    RenderLayer Layer() const;
    void Destroy();
    void InvalidateNavigation();

private:

    friend class SceneGraph;
    ActorId id = InvalidActor;
    std::string name;
    Vec2 localPosition;
    RenderLayer layer;
    bool active = true, visible = true, pendingDestroy = false;
    std::weak_ptr<Actor> parent;
    std::vector<std::weak_ptr<Actor>> children;
    std::shared_ptr<SceneState> state;
    bool ContainsNavigationObstacle() const;
};
