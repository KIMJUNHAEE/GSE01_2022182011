#pragma once
#include "Canvas.h"
#include "GameWorld.h"
#include "Renderer.h"

class Game {
public:
    Game(Renderer& renderer);
    GameWorld world;
    void Tick(float dt);
    void Render();
    void Key(unsigned char key, bool down);
    void Arrow(int key, bool down);
    void Click(float x, float y);
    void Wheel(int direction);
    void ClearInput();
    void SnapCamera();
    void SetRunning(bool value) { running = value; }
    bool WantsExit() const { return exitRequested; }
    float Zoom() const { return zoom; }
private:
    Renderer& renderer;
    Canvas canvas;
    GLuint sky = 0, portraits = 0;
    Vec2 camera;
    float zoom = 1.08f, desiredZoom = 1.08f, typing = 0, arrival = 0;
    bool keys[256] = {}, arrows[4] = {}, running = false, exitRequested = false;
    size_t lastDialogueSize = 0;
    float objectAlpha = 1;
    Vec2 P(Vec2 grid, float z = 0) const;
    void Poly(Vec2 a, Vec2 b, Vec2 c, Vec2 d, Color color);
    void Box(Vec2 p, float w, float d, float z, float h, Color top, Color left, Color right);
    void Ground();
    void Building(const Prop& prop);
    void DrawProp(const Prop& prop);
    void Person(Vec2 foot, int kind, float size, bool moving, float phase = 0);
    void Portrait(float x,float y,float w,float h,int kind);
    void WorldScene();
    void HUD();
    void Map(float x, float y, float w, float h, bool full);
    void Dialogue();
    void Journal();
    void Title();
    void Ending();
    void Panel(float x, float y, float w, float h, float alpha = .88f);
    void Keycap(float x, float y, const std::wstring& key, bool active = false);
    void Paragraph(float x, float y, const std::wstring& text, float width, int size, Color color, float lineHeight);
    void Marker(Vec2 p, Color color, float size = 7);
    void CenterText(float x, float y, const std::wstring& text, int size, Color color, bool bold = false);
};
