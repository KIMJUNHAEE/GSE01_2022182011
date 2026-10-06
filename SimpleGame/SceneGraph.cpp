#include "stdafx.h"
#include "SceneGraph.h"
#include <algorithm>
#include <stdexcept>

SceneGraph::SceneGraph() : state(std::make_shared<SceneState>())
{
}

SceneGraph::SceneGraph(const SceneGraph& other) : SceneGraph()
{
    nextId = other.nextId;
    state->navigationRevision = other.state->navigationRevision;
    for (const auto& entry : other.nodes)
    {
        if (entry.second->IsPendingDestroy())
        {
            continue;
        }
        std::shared_ptr<Actor> clone = entry.second->Clone();
        clone->id = entry.first;
        clone->state = state;
        nodes.emplace(entry.first, clone);
        types[typeid(*clone)].push_back(entry.first);
    }
    for (const auto& entry : nodes)
    {
        ActorId parent = other.nodes.at(entry.first)->ParentId();
        if (parent != InvalidActor)
        {
            auto owner = nodes.at(parent);
            entry.second->parent = owner;
            owner->children.push_back(entry.second);
        }
    }
}

SceneGraph& SceneGraph::operator=(const SceneGraph& other)
{
    if (this != &other)
    {
        SceneGraph copy(other);
        *this = std::move(copy);
    }
    return *this;
}

void SceneGraph::Attach(const std::shared_ptr<Actor>& actor, ActorId parent)
{
    if (parent != InvalidActor && !Find(parent))
    {
        throw std::invalid_argument("SceneGraph parent does not exist");
    }
    actor->id = nextId++;
    actor->state = state;
    nodes.emplace(actor->id, actor);
    types[typeid(*actor)].push_back(actor->id);
    if (parent != InvalidActor)
    {
        actor->parent = nodes.at(parent);
        nodes.at(parent)->children.push_back(actor);
    }
    actor->InvalidateNavigation();
}

Actor* SceneGraph::Find(ActorId id)
{
    auto found = nodes.find(id);
    return found == nodes.end() || found->second->IsPendingDestroy() ? nullptr
                                                                     : found->second.get();
}

const Actor* SceneGraph::Find(ActorId id) const
{
    auto found = nodes.find(id);
    return found == nodes.end() || found->second->IsPendingDestroy() ? nullptr
                                                                     : found->second.get();
}

bool SceneGraph::Reparent(ActorId child, ActorId parent, bool keepWorldPosition)
{
    Actor* actor = Find(child);
    if (!actor || child == parent || (parent != InvalidActor && !Find(parent)))
    {
        return false;
    }
    for (const Actor* node = Find(parent); node; node = Find(node->ParentId()))
    {
        if (node->Id() == child)
        {
            return false;
        }
    }
    Vec2 position = actor->Position();
    if (auto old = actor->parent.lock())
    {
        auto& children = old->children;
        children.erase(std::remove_if(children.begin(), children.end(),
                                      [child](const auto& ref)
                                      {
                                          auto node = ref.lock();
                                          return !node || node->Id() == child;
                                      }),
                       children.end());
    }
    actor->parent.reset();
    if (parent != InvalidActor)
    {
        actor->parent = nodes.at(parent);
        nodes.at(parent)->children.push_back(nodes.at(child));
    }
    if (keepWorldPosition)
    {
        actor->SetPosition(position);
    }
    actor->InvalidateNavigation();
    return true;
}

void SceneGraph::Destroy(ActorId id)
{
    if (auto actor = Find(id))
    {
        actor->Destroy();
    }
}

void SceneGraph::CollectDestroyed()
{
    for (auto& entry : nodes)
    {
        auto& children = entry.second->children;
        children.erase(std::remove_if(children.begin(), children.end(),
                                      [](const auto& ref)
                                      {
                                          auto node = ref.lock();
                                          return !node || node->IsPendingDestroy();
                                      }),
                       children.end());
    }
    for (auto& entry : types)
    {
        auto& ids = entry.second;
        ids.erase(std::remove_if(ids.begin(), ids.end(),
                                 [&](ActorId id)
                                 {
                                     return nodes.at(id)->IsPendingDestroy();
                                 }),
                  ids.end());
    }
    for (auto it = nodes.begin(); it != nodes.end();)
    {
        if (it->second->IsPendingDestroy())
        {
            it = nodes.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void SceneGraph::Update(float dt)
{
    // A snapshot keeps deletion/spawning from invalidating this traversal.
    std::vector<std::shared_ptr<Actor>> snapshot;
    for (const auto& entry : nodes)
    {
        snapshot.push_back(entry.second);
    }
    for (const auto& actor : snapshot)
    {
        if (actor->IsActive())
        {
            actor->Update(dt);
        }
    }
    CollectDestroyed();
}

void SceneGraph::Draw(ActorRenderer& renderer) const
{
    std::vector<std::shared_ptr<Actor>> snapshot;
    for (const auto& entry : nodes)
    {
        if (entry.second->IsVisible())
        {
            snapshot.push_back(entry.second);
        }
    }
    std::stable_sort(snapshot.begin(), snapshot.end(),
                     [](const auto& a, const auto& b)
                     {
                         if (a->Layer() != b->Layer())
                         {
                             return a->Layer() < b->Layer();
                         }
                         Vec2 pa = a->Position(), pb = b->Position();
                         return pa.x + pa.y < pb.x + pb.y;
                     });
    for (const auto& actor : snapshot)
    {
        if (actor->IsVisible())
        {
            actor->Draw(renderer);
        }
    }
}

uint64_t SceneGraph::NavigationRevision() const
{
    return state->navigationRevision;
}

size_t SceneGraph::Size() const
{
    return nodes.size();
}
