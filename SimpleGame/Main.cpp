/* Copyright 2022 Lee Taek Hee (Tech University of Korea).
   Base framework distributed under the What The Hell License. */
#include "stdafx.h"
#include "Game.h"
#include "Dependencies/freeglut.h"
#include "Dependencies/wglew.h"
#include <windows.h>
#include <filesystem>
#include <memory>
#include <iostream>
#include <algorithm>

namespace
{
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Game> game;
    int windowWidth = 1280, windowHeight = 800, lastTime = 0;
    bool shiftLeft = false, shiftRight = false;

    void Close()
    {
        game.reset();
        renderer.reset();
    }

    void Display()
    {
        if (game)
        {
            game->Render();
            glutSwapBuffers();
        }
    }

    void Reshape(int w, int h)
    {
        windowWidth = (std::max)(1, w);
        windowHeight = (std::max)(1, h);
        if (renderer)
        {
            renderer->SetOutputSize(windowWidth, windowHeight);
        }
    }

    void Timer(int)
    {
        if (!game)
        {
            return;
        }
        HWND ownWindow = WindowFromDC(wglGetCurrentDC());
        if (ownWindow && GetForegroundWindow() != ownWindow)
        {
            game->ClearInput();
            shiftLeft = shiftRight = false;
            if (game->world.view == View::Explore)
            {
                game->world.view = View::Pause;
            }
        }
        int now = glutGet(GLUT_ELAPSED_TIME);
        game->Tick((now - lastTime) / 1000.f);
        lastTime = now;
        if (game->WantsExit())
        {
            glutLeaveMainLoop();
            return;
        }
        glutPostRedisplay();
        glutTimerFunc(16, Timer, 0);
    }

    void KeyDown(unsigned char key, int, int)
    {
        if (game)
        {
            game->Key(key, true);
        }
    }

    void KeyUp(unsigned char key, int, int)
    {
        if (game)
        {
            game->Key(key, false);
        }
    }

    void Special(int key, bool down)
    {
        if (!game)
        {
            return;
        }
        if (key == GLUT_KEY_SHIFT_L)
        {
            shiftLeft = down;
        }
        if (key == GLUT_KEY_SHIFT_R)
        {
            shiftRight = down;
        }
        game->SetRunning(shiftLeft || shiftRight);
        game->Arrow(key, down);
    }

    void SpecialDown(int key, int, int)
    {
        Special(key, true);
    }

    void SpecialUp(int key, int, int)
    {
        Special(key, false);
    }

    Vec2 MousePosition(int x, int y)
    {
        float scale = (std::min)(windowWidth / 1440.f, windowHeight / 900.f);
        return {(x - (windowWidth - 1440 * scale) / 2) / scale,
                (y - (windowHeight - 900 * scale) / 2) / scale};
    }

    void Motion(int x, int y)
    {
        if (game)
        {
            Vec2 p = MousePosition(x, y);
            game->Pointer(p.x, p.y);
        }
    }

    void Mouse(int button, int state, int x, int y)
    {
        if (!game || button != GLUT_LEFT_BUTTON)
        {
            return;
        }
        Vec2 p = MousePosition(x, y);
        game->MouseButton(p.x, p.y, state == GLUT_DOWN);
    }

    void Wheel(int, int direction, int, int)
    {
        if (game)
        {
            game->Wheel(direction);
        }
    }

    void Entry(int state)
    {
        if (game && state == GLUT_LEFT)
        {
            game->ClearInput();
            shiftLeft = shiftRight = false;
        }
    }

    // The project links as a Windows-subsystem app (see SimpleGame.vcxproj) so the
    // shipped game never flashes a console behind it. That also means std::cout/
    // cerr have nowhere to go by default - in Debug builds, open a real console
    // window and redirect the standard streams into it so things like the
    // [Perf] FPS/draw-call log (RenderStats) are actually visible while testing.
#ifdef _DEBUG
    void AttachDebugConsole()
    {
        if (AllocConsole())
        {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            freopen_s(&dummy, "CONIN$", "r", stdin);
            std::cout.clear();
            std::cerr.clear();
        }
    }
#endif

    void FindResources()
    {
        wchar_t buffer[32768] = {};
        GetModuleFileNameW(nullptr, buffer, 32768);
        const auto exe = std::filesystem::path(buffer).parent_path();
        for (const auto& path :
             {exe, exe / "SimpleGame", exe.parent_path() / "SimpleGame",
              exe.parent_path().parent_path() / "SimpleGame",
              std::filesystem::current_path() / "SimpleGame", std::filesystem::current_path()})
        {
            if (std::filesystem::exists(path / "Shaders/Sprite.vs") &&
                std::filesystem::exists(path / "Shaders/Canvas.vs") &&
                std::filesystem::exists(path / "Shaders/Canvas.fs") &&
                std::filesystem::exists(path / "Resources/Textures/elysium_sky.png"))
            {
                std::filesystem::current_path(path);
                return;
            }
        }
        throw std::runtime_error("Resources not found. Build the project to copy Shaders and "
                                 "Resources next to the executable.");
    }
} // namespace

int RunPrototypeVerification(Game& game, Renderer& renderer, int width, int height,
                             const std::filesystem::path& output);

int main(int argc, char** argv)
{
#ifdef _DEBUG
    AttachDebugConsole();
#endif
    bool verify = false;
    std::filesystem::path output = std::filesystem::current_path() / "output/verification";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verify")
        {
            verify = true;
        }
        if (std::string(argv[i]) == "--output" && i + 1 < argc)
        {
            output = std::filesystem::absolute(argv[++i]);
        }
    }
    try
    {
        FindResources();
        SetProcessDPIAware();
        int glutArgc = 1;
        glutInit(&glutArgc, argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
        glutInitContextVersion(3, 3);
        glutInitContextProfile(GLUT_COMPATIBILITY_PROFILE);
        glutInitWindowSize(windowWidth, windowHeight);
        glutInitWindowPosition(100, 70);
        glutCreateWindow("STAR DRIFTER | First Contact");
        if (verify)
        {
            glutHideWindow();
        }
        glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
        if (glewInit() != GLEW_OK || !GLEW_VERSION_3_3)
        {
            throw std::runtime_error("OpenGL 3.3 is required");
        }
        while (glGetError() != GL_NO_ERROR)
        {
        }
        if (WGLEW_EXT_swap_control)
        {
            wglSwapIntervalEXT(verify ? 0 : 1);
        }
        renderer = std::make_unique<Renderer>(1440, 900);
        renderer->SetOutputSize(windowWidth, windowHeight);
        if (!renderer->IsInitialized())
        {
            throw std::runtime_error("Renderer initialization failed");
        }
        game = std::make_unique<Game>(*renderer);
        glutDisplayFunc(Display);
        glutReshapeFunc(Reshape);
        glutCloseFunc(Close);
        if (verify)
        {
            int result =
                RunPrototypeVerification(*game, *renderer, windowWidth, windowHeight, output);
            Close();
            glutDestroyWindow(glutGetWindow());
            return result;
        }
        glutKeyboardFunc(KeyDown);
        glutKeyboardUpFunc(KeyUp);
        glutSpecialFunc(SpecialDown);
        glutSpecialUpFunc(SpecialUp);
        glutMouseFunc(Mouse);
        glutMotionFunc(Motion);
        glutPassiveMotionFunc(Motion);
        glutMouseWheelFunc(Wheel);
        glutEntryFunc(Entry);
        glutIgnoreKeyRepeat(1);
        lastTime = glutGet(GLUT_ELAPSED_TIME);
        glutTimerFunc(16, Timer, 0);
        std::cout << "\nSTAR DRIFTER ready. WASD move, Shift run, E interact, Q scan, M map, J "
                     "journal.\n";
        glutMainLoop();
        Close();
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "\nSTAR DRIFTER: " << e.what() << std::endl;
        if (!verify)
        {
            MessageBoxA(nullptr, e.what(), "STAR DRIFTER - startup error", MB_OK | MB_ICONERROR);
        }
        Close();
        return 1;
    }
}
