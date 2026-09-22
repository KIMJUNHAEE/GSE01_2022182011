#pragma once
#include "Actor.h"
#include <map>
#include <typeindex>
#include <type_traits>
#include <utility>

// Snapshot views do not own a second game state. They retain nodes only while being traversed.
template <typename T> class ActorRange
{
public:

    struct Iterator
    {
        using Inner = typename std::vector<std::shared_ptr<T>>::const_iterator;
        Inner current;

        T& operator*() const
        {
            return **current;
        }

        Iterator& operator++()
        {
            ++current;
            return *this;
        }

        bool operator!=(const Iterator& other) const
        {
            return current != other.current;
        }
    };

    Iterator begin() const
    {
        return {nodes.begin()};
    }

    Iterator end() const
    {
        return {nodes.end()};
    }

    size_t size() const
    {
        return nodes.size();
    }

    bool empty() const
    {
        return nodes.empty();
    }

    T& operator[](size_t index) const
    {
        return *nodes.at(index);
    }

private:

    friend class SceneGraph;
    std::vector<std::shared_ptr<T>> nodes;
};

class SceneGraph
{
public:

    SceneGraph();
    SceneGraph(const SceneGraph& other);
    SceneGraph& operator=(const SceneGraph& other);
    SceneGraph(SceneGraph&&) noexcept = default;
    SceneGraph& operator=(SceneGraph&&) noexcept = default;

    template <typename T> T& Add(const T& prototype, ActorId parent = InvalidActor)
    {
        static_assert(std::is_base_of_v<Actor, T>);
        auto actor = std::make_shared<T>(prototype);
        Attach(actor, parent);
        return *actor;
    }

    template <typename T> ActorRange<T> Actors(bool includeInactive = false)
    {
        ActorRange<T> result;
        auto found = types.find(typeid(T));
        if (found != types.end())
        {
            for (ActorId id : found->second)
            {
                auto actor = nodes.at(id);
                if (!actor->IsPendingDestroy() && (includeInactive || actor->IsActive()))
                {
                    result.nodes.push_back(std::static_pointer_cast<T>(actor));
                }
            }
        }
        return result;
    }

    template <typename T> ActorRange<const T> Actors(bool includeInactive = false) const
    {
        ActorRange<const T> result;
        auto found = types.find(typeid(T));
        if (found != types.end())
        {
            for (ActorId id : found->second)
            {
                auto actor = nodes.at(id);
                if (!actor->IsPendingDestroy() && (includeInactive || actor->IsActive()))
                {
                    result.nodes.push_back(std::static_pointer_cast<const T>(actor));
                }
            }
        }
        return result;
    }

    template <typename T, typename Predicate> void RemoveIf(Predicate predicate)
    {
        for (auto& actor : Actors<T>(true))
        {
            if (predicate(actor))
            {
                actor.Destroy();
            }
        }
    }

    template <typename T> void Clear()
    {
        RemoveIf<T>(
            [](const T&)
            {
                return true;
            });
    }

    Actor* Find(ActorId id);
    const Actor* Find(ActorId id) const;

    template <typename T> T* Find(ActorId id)
    {
        return dynamic_cast<T*>(Find(id));
    }

    template <typename T> const T* Find(ActorId id) const
    {
        return dynamic_cast<const T*>(Find(id));
    }

    bool Reparent(ActorId child, ActorId parent, bool keepWorldPosition = true);
    void Destroy(ActorId id);
    void CollectDestroyed();
    void Update(float dt);
    void Draw(ActorRenderer& renderer) const;
    uint64_t NavigationRevision() const;
    size_t Size() const;

private:

    std::map<ActorId, std::shared_ptr<Actor>> nodes;
    std::map<std::type_index, std::vector<ActorId>> types;
    std::shared_ptr<SceneState> state;
    ActorId nextId = 1;
    void Attach(const std::shared_ptr<Actor>& actor, ActorId parent);
};
