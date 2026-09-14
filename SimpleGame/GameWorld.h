#pragma once
#include "Canvas.h"
#include <array>
#include <deque>

enum class PropKind { Habitat, Archive, Tree, Crate, Lamp, Relay, Core, Ship, Citizen, Memory };
struct Prop {
    PropKind kind; Vec2 p; float w, d, h; int variant = 0; bool solid = true;
};
struct District { int x0, y0, x1, y1, kind; };
struct DialogueLine { std::wstring speaker, text; };
enum class View { Title, Explore, Dialogue, Map, Journal, Pause, Ending };

class GameWorld {
public:
    GameWorld();
    View view = View::Title;
    Vec2 player = { -5, 1 }, captain = { -5.7f, 2.2f }, facing = { 1,0 };
    std::vector<District> districts;
    std::vector<Prop> props;
    std::array<Vec2, 3> relays = { Vec2{ 5,-11 }, Vec2{ 15,1 }, Vec2{ 5,3 } };
    std::array<bool, 3> restored = { false,false,false };
    std::array<Vec2, 3> memories = { Vec2{ -8,-3 }, Vec2{ 1,-12 }, Vec2{ 18,-3 } };
    std::array<bool, 3> found = { false,false,false };
    std::deque<DialogueLine> dialogue;
    std::deque<Vec2> trail;
    int quest = 0, choice = -1, nearby = -1;
    bool choicePending = false, walking = false, captainWalking = false;
    float time = 0, scanCooldown = 0, scanWave = -1, toastTime = 0;
    std::wstring toast;
    Vec2 scanOrigin;
    int Tile(int x, int y) const;
    bool Walkable(Vec2 p, float radius = .22f) const;
    void Move(Vec2 screenDirection, float dt, bool running);
    void Update(float dt);
    void Start();
    void Interact();
    void Advance();
    void Choose(int option);
    void Scan();
    void Notify(const std::wstring& text);
    int RestoredCount() const;
    int MemoryCount() const;
    Vec2 Objective() const;
    std::wstring ObjectiveText() const;
    std::wstring NearbyText() const;
    std::wstring Region() const;
    static float Distance(Vec2 a, Vec2 b);
    static Vec2 Project(Vec2 grid) { return { (grid.x - grid.y) * 42, (grid.x + grid.y) * 21 }; }
private:
    void Say(std::initializer_list<DialogueLine> lines);
    void UpdateNearby();
};
