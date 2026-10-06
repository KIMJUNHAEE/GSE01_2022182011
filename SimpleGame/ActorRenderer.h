#pragma once

class Prop;
class Character;
class Terrain;
class SceneEffect;
class Enemy;
class Projectile;
class Loot;
class CombatText;

// SceneGraph does not depend on OpenGL, Canvas, Game, or a specific level.
class ActorRenderer
{
public:

    virtual ~ActorRenderer() = default;
    virtual void Draw(const Prop& actor) = 0;
    virtual void Draw(const Character& actor) = 0;
    virtual void Draw(const Terrain& actor) = 0;
    virtual void Draw(const SceneEffect& actor) = 0;
    virtual void Draw(const Enemy& actor) = 0;
    virtual void Draw(const Projectile& actor) = 0;
    virtual void Draw(const Loot& actor) = 0;
    virtual void Draw(const CombatText& actor) = 0;
};
