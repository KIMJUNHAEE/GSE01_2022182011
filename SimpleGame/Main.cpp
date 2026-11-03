/* Copyright 2022 Lee Taek Hee (Tech University of Korea).
   Base framework distributed under the What The Hell License. */
#include "stdafx.h"
#include "Game.h"
#include "MonitorPublisher.h"
#include "RenderStats.h"
#include "GlfwInput.h"
#include "WindowMetrics.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
    GLFWwindow* window = nullptr;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<Game> game;
    std::unique_ptr<MonitorPublisher> monitor;
    WindowMetrics metrics;
    std::exception_ptr callbackError;
    bool glfwInitialized = false, focused = true, iconified = false, changingDisplay = false;
    bool shiftLeft = false, shiftRight = false, vsync = true;
    double scrollRemainder = 0, lastTime = 0;
    int savedX = 100, savedY = 70, savedWidth = 1280, savedHeight = 800;
    char lastGlfwError[1024] = {};

    void Close()
    {
        // OpenGL resources MUST be destroyed while their context is still alive/current.
        if (window)
        {
            glfwMakeContextCurrent(window);
        }
        monitor.reset();
        game.reset();
        renderer.reset();
        if (window)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        if (glfwInitialized)
        {
            glfwTerminate();
            glfwInitialized = false;
        }
    }

    void ClearInput()
    {
        shiftLeft = shiftRight = false;
        scrollRemainder = 0;
        if (game)
        {
            game->ClearInput();
        }
    }

    // Never unwind a C++ exception through GLFW's C callback stack.
    template <typename F> void Callback(F&& action) noexcept
    {
        try
        {
            action();
        }
        catch (...)
        {
            if (!callbackError)
            {
                callbackError = std::current_exception();
            }
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    void GlfwError(int code, const char* description)
    {
        std::snprintf(lastGlfwError, sizeof(lastGlfwError), "GLFW error %d: %s", code,
                      description ? description : "unknown");
        std::fprintf(stderr, "%s\n", lastGlfwError);
    }

    void RefreshPointer()
    {
        if (game)
        {
            double x = 0, y = 0;
            glfwGetCursorPos(window, &x, &y);
            const Vec2 point = metrics.ToCanvas(x, y);
            game->Pointer(point.x, point.y);
        }
    }

    void UpdateMetrics()
    {
        glfwGetWindowSize(window, &metrics.width, &metrics.height);
        glfwGetFramebufferSize(window, &metrics.framebufferWidth, &metrics.framebufferHeight);
        if (renderer && metrics.Drawable())
        {
            // Scene attachments resize once at the next frame boundary; mesh caches stay intact.
            renderer->SetOutputSize(metrics.framebufferWidth, metrics.framebufferHeight);
        }
        RefreshPointer();
    }

    GLFWmonitor* BestMonitor(int x, int y, int width, int height)
    {
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        GLFWmonitor* best = glfwGetPrimaryMonitor();
        long long bestArea = -1;
        for (int i = 0; i < count; ++i)
        {
            int mx = 0, my = 0, mw = 0, mh = 0;
            glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
            const long long overlapX =
                (std::max)(0LL, (std::min)((long long)x + width, (long long)mx + mw) -
                                    (std::max)((long long)x, (long long)mx));
            const long long overlapY =
                (std::max)(0LL, (std::min)((long long)y + height, (long long)my + mh) -
                                    (std::max)((long long)y, (long long)my));
            const long long area = overlapX * overlapY;
            if (area > bestArea)
            {
                bestArea = area;
                best = monitors[i];
            }
        }
        return best;
    }

    void ToggleFullscreen()
    {
        ClearInput();
        changingDisplay = true;
        if (!glfwGetWindowMonitor(window))
        {
            glfwGetWindowPos(window, &savedX, &savedY);
            glfwGetWindowSize(window, &savedWidth, &savedHeight);
            GLFWmonitor* target = BestMonitor(savedX, savedY, savedWidth, savedHeight);
            const GLFWvidmode* mode = target ? glfwGetVideoMode(target) : nullptr;
            if (!mode)
            {
                changingDisplay = false;
                throw std::runtime_error("No monitor video mode is available");
            }
            // Reuse this window/context and the monitor's current mode; never recreate GPU caches.
            glfwSetWindowMonitor(window, target, 0, 0, mode->width, mode->height,
                                 mode->refreshRate);
        }
        else
        {
            GLFWmonitor* target = BestMonitor(savedX, savedY, savedWidth, savedHeight);
            int x = 0, y = 0, width = savedWidth, height = savedHeight;
            if (target)
            {
                glfwGetMonitorWorkarea(target, &x, &y, &width, &height);
            }
            const int w = (std::min)(savedWidth, width);
            const int h = (std::min)(savedHeight, height);
            glfwSetWindowMonitor(window, nullptr, std::clamp(savedX, x, x + width - w),
                                 std::clamp(savedY, y, y + height - h), w, h, GLFW_DONT_CARE);
        }
        glfwSwapInterval(vsync ? 1 : 0);
        changingDisplay = false;
        UpdateMetrics();
        lastTime = glfwGetTime();
        GLFWmonitor* current = glfwGetWindowMonitor(window);
        if (!current)
        {
            int x = 0, y = 0;
            glfwGetWindowPos(window, &x, &y);
            current = BestMonitor(x, y, metrics.width, metrics.height);
        }
        const GLFWvidmode* currentMode = current ? glfwGetVideoMode(current) : nullptr;
        float scaleX = 1, scaleY = 1;
        glfwGetWindowContentScale(window, &scaleX, &scaleY);
        std::cout << "[Display] " << (glfwGetWindowMonitor(window) ? "fullscreen " : "windowed ")
                  << metrics.framebufferWidth << "x" << metrics.framebufferHeight << " / "
                  << (currentMode ? currentMode->refreshRate : 0) << " Hz / DPI scale " << scaleX
                  << 'x' << scaleY << '\n';
    }

    void Resize(GLFWwindow*, int, int)
    {
        Callback(
            []
            {
                UpdateMetrics();
            });
    }

    void ContentScale(GLFWwindow*, float, float)
    {
        Callback(
            []
            {
                UpdateMetrics();
            });
    }

    void PositionChanged(GLFWwindow*, int, int)
    {
        Callback(
            []
            {
                UpdateMetrics();
            });
    }

    void MonitorChanged(GLFWmonitor*, int)
    {
        Callback(
            []
            {
                UpdateMetrics();
            });
    }

    void Focus(GLFWwindow*, int value)
    {
        Callback(
            [value]
            {
                focused = value == GLFW_TRUE;
                ClearInput();
                if (!focused && !changingDisplay && game && game->world.view == View::Explore)
                {
                    game->world.view = View::Pause;
                }
                lastTime = glfwGetTime();
                RenderStats::RestartWindow();
                RefreshPointer();
            });
    }

    void Iconify(GLFWwindow*, int value)
    {
        Callback(
            [value]
            {
                iconified = value == GLFW_TRUE;
                if (iconified)
                {
                    ClearInput();
                    if (game && game->world.view == View::Explore)
                    {
                        game->world.view = View::Pause;
                    }
                }
                else
                {
                    UpdateMetrics();
                }
                lastTime = glfwGetTime();
                RenderStats::RestartWindow();
            });
    }

    void Key(GLFWwindow*, int key, int, int action, int mods)
    {
        Callback(
            [key, action, mods]
            {
                if (!game || action == GLFW_REPEAT)
                {
                    return;
                }
                const bool down = action == GLFW_PRESS;
                if (down &&
                    (key == GLFW_KEY_F11 || ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) &&
                                             (mods & GLFW_MOD_ALT))))
                {
                    ToggleFullscreen();
                    return;
                }
                if (key == GLFW_KEY_LEFT_SHIFT)
                {
                    shiftLeft = down;
                }
                if (key == GLFW_KEY_RIGHT_SHIFT)
                {
                    shiftRight = down;
                }
                game->SetRunning(shiftLeft || shiftRight);
                const auto special = GlfwInput::Special(key);
                if (special != SpecialKey::None)
                {
                    game->Arrow(special, down);
                }
                const auto character = GlfwInput::Character(key);
                if (character)
                {
                    game->Key(character, down);
                }
            });
    }

    void Motion(GLFWwindow*, double x, double y)
    {
        Callback(
            [x, y]
            {
                if (game)
                {
                    const Vec2 point = metrics.ToCanvas(x, y);
                    game->Pointer(point.x, point.y);
                }
            });
    }

    void Mouse(GLFWwindow*, int button, int action, int)
    {
        Callback(
            [button, action]
            {
                if (!game || button != GLFW_MOUSE_BUTTON_LEFT)
                {
                    return;
                }
                double x = 0, y = 0;
                glfwGetCursorPos(window, &x, &y);
                const Vec2 point = metrics.ToCanvas(x, y);
                game->MouseButton(point.x, point.y, action == GLFW_PRESS);
            });
    }

    void Scroll(GLFWwindow*, double, double y)
    {
        Callback(
            [y]
            {
                if (!game || !std::isfinite(y))
                {
                    return;
                }
                // Preserve fractional high-resolution wheel/trackpad input.
                scrollRemainder = std::clamp(scrollRemainder + y, -32.0, 32.0);
                while (std::abs(scrollRemainder) >= 1)
                {
                    const int direction = scrollRemainder > 0 ? 1 : -1;
                    game->Wheel(direction);
                    scrollRemainder -= direction;
                }
            });
    }

    void Entry(GLFWwindow*, int entered)
    {
        Callback(
            [entered]
            {
                if (!entered)
                {
                    ClearInput();
                    if (game)
                    {
                        game->Pointer(-1, -1);
                    }
                }
                else
                {
                    RefreshPointer();
                }
            });
    }

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

    void Run()
    {
        lastTime = glfwGetTime();
        while (!glfwWindowShouldClose(window))
        {
            const double frameStart = glfwGetTime();
            glfwPollEvents();
            if (callbackError)
            {
                std::rethrow_exception(callbackError);
            }
            if (glfwWindowShouldClose(window))
            {
                break;
            }
            if (!focused || iconified || !metrics.Drawable())
            {
                if (monitor)
                {
                    monitor->Capture(game->world);
                }
                // Pause input/world time and avoid a busy-loop or useless GPU work while hidden.
                glfwWaitEventsTimeout(.1);
                if (callbackError)
                {
                    std::rethrow_exception(callbackError);
                }
                lastTime = glfwGetTime();
                continue;
            }
            const double now = glfwGetTime();
            const float dt = float((std::max)(0.0, now - lastTime));
            lastTime = now;
            const double updateStart = glfwGetTime();
            game->Tick(dt);
            if (monitor)
            {
                monitor->Capture(game->world);
            }
            if (game->WantsExit())
            {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                continue;
            }
            const double renderStart = glfwGetTime();
            game->Render();
            const double beforeSwap = glfwGetTime();
            glfwSwapBuffers(window);
            const double end = glfwGetTime();
            RenderStats::RecordTiming((end - frameStart) * 1000, (beforeSwap - frameStart) * 1000,
                                      (end - beforeSwap) * 1000);
            RenderStats::RecordWork((renderStart - updateStart) * 1000,
                                    (beforeSwap - renderStart) * 1000);
            RenderStats::LogFrame();
        }
    }
} // namespace

int RunPrototypeVerification(Game& game, Renderer& renderer, int width, int height,
                             const std::filesystem::path& output);

int main(int argc, char** argv)
{
#ifdef _DEBUG
    AttachDebugConsole();
#endif
    bool verify = false, startFullscreen = true, enableMonitor = true;
    unsigned short monitorPort = 8765;
    std::filesystem::path output = std::filesystem::current_path() / "output/verification";
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--windowed")
        {
            startFullscreen = false;
        }
        else if (argument == "--fullscreen")
        {
            startFullscreen = true;
        }
        else if (argument == "--no-vsync")
        {
            vsync = false;
        }
        else if (argument == "--no-monitor")
        {
            enableMonitor = false;
        }
        else if (argument == "--monitor-port")
        {
            char* end = nullptr;
            const long port = i + 1 < argc ? std::strtol(argv[++i], &end, 10) : 0;
            if (!end || *end || port < 1024 || port > 65535)
            {
                std::cerr << "--monitor-port requires an integer between 1024 and 65535\n";
                return 1;
            }
            monitorPort = (unsigned short)port;
        }
        else if (argument == "--verify")
        {
            verify = true;
        }
        else if (argument == "--output" && i + 1 < argc)
        {
            output = std::filesystem::absolute(argv[++i]);
        }
        else
        {
            std::cerr << "Unknown or incomplete argument: " << argument << '\n';
            return 1;
        }
    }
    try
    {
        FindResources();
        glfwSetErrorCallback(GlfwError);
        if (!glfwInit())
        {
            throw std::runtime_error(lastGlfwError[0] ? lastGlfwError
                                                      : "GLFW initialization failed");
        }
        glfwInitialized = true;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        // Renderer still uses compatibility-profile vertex arrays. Do not silently request core.
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
        // Depth belongs to the existing scene FBO, not the final 2D output framebuffer.
        glfwWindowHint(GLFW_DEPTH_BITS, 0);
        glfwWindowHint(GLFW_STENCIL_BITS, 0);
        glfwWindowHint(GLFW_SAMPLES, 0);
        glfwWindowHint(GLFW_VISIBLE, verify ? GLFW_FALSE : GLFW_TRUE);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, verify ? GLFW_FALSE : GLFW_TRUE);
        GLFWmonitor* primary = glfwGetPrimaryMonitor();
        int x = 100, y = 70, width = 1280, height = 800;
        const GLFWvidmode* mode = primary ? glfwGetVideoMode(primary) : nullptr;
        if (!verify && primary)
        {
            int workWidth = 1280, workHeight = 800;
            glfwGetMonitorWorkarea(primary, &x, &y, &workWidth, &workHeight);
            width = (std::max)(1, int(workWidth * .85));
            height = (std::max)(1, int(workHeight * .85));
            x += (workWidth - width) / 2;
            y += (workHeight - height) / 2;
            if (mode)
            {
                glfwWindowHint(GLFW_RED_BITS, mode->redBits);
                glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
                glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
                glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            }
        }
        window = glfwCreateWindow(width, height, "STAR DRIFTER | GLFW", nullptr, nullptr);
        if (!window)
        {
            throw std::runtime_error(lastGlfwError[0] ? lastGlfwError : "Window creation failed");
        }
        glfwSetWindowPos(window, x, y);
        // Work-area dimensions already use native units. Creation-time DPI scaling must not
        // enlarge the fitted rectangle a second time (e.g. 200% on a laptop display).
        glfwSetWindowSize(window, width, height);
        glfwMakeContextCurrent(window);
        glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK || !GLEW_VERSION_3_3)
        {
            throw std::runtime_error("OpenGL 3.3 is required");
        }
        while (glGetError() != GL_NO_ERROR)
        {
        }
        glfwSwapInterval(verify ? 0 : (vsync ? 1 : 0));
        UpdateMetrics();
        renderer = std::make_unique<Renderer>(1440, 900);
        renderer->SetOutputSize(metrics.framebufferWidth, metrics.framebufferHeight);
        if (!renderer->IsInitialized())
        {
            throw std::runtime_error("Renderer initialization failed");
        }
        game = std::make_unique<Game>(*renderer);
        glfwSetWindowSizeCallback(window, Resize);
        glfwSetFramebufferSizeCallback(window, Resize);
        glfwSetWindowContentScaleCallback(window, ContentScale);
        glfwSetWindowPosCallback(window, PositionChanged);
        glfwSetMonitorCallback(MonitorChanged);
        if (verify)
        {
            const int result = RunPrototypeVerification(*game, *renderer, metrics.framebufferWidth,
                                                        metrics.framebufferHeight, output);
            if (callbackError)
            {
                std::rethrow_exception(callbackError);
            }
            Close();
            return result;
        }
        focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
        iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
        glfwSetWindowFocusCallback(window, Focus);
        glfwSetWindowIconifyCallback(window, Iconify);
        glfwSetKeyCallback(window, Key);
        glfwSetCursorPosCallback(window, Motion);
        glfwSetMouseButtonCallback(window, Mouse);
        glfwSetScrollCallback(window, Scroll);
        glfwSetCursorEnterCallback(window, Entry);
        if (enableMonitor)
        {
            monitor = std::make_unique<MonitorPublisher>(monitorPort);
            monitor->Capture(game->world);
        }
        if (startFullscreen)
        {
            ToggleFullscreen();
        }
        RefreshPointer();
        RenderStats::RestartWindow();
        std::cout << "\nSTAR DRIFTER ready (GLFW " << glfwGetVersionString()
                  << "). WASD move, Shift run, E interact, Q scan, M map, J journal.\n"
                     "F11 / Alt+Enter fullscreen, F7 reload art, F8 compare graphics.\n";
#ifdef _DEBUG
        std::cout << "[Perf] Debug build (unoptimized). Use x64 Release for FPS comparisons.\n";
#else
        std::cout << "[Perf] Release build.\n";
#endif
        Run();
        Close();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "\nSTAR DRIFTER: " << error.what() << std::endl;
        if (!verify)
        {
            MessageBoxA(nullptr, error.what(), "STAR DRIFTER - startup error",
                        MB_OK | MB_ICONERROR);
        }
        Close();
        return 1;
    }
}
