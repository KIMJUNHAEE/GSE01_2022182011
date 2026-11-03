#pragma once
#include "Input.h"
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include "Dependencies/GLFW/include/GLFW/glfw3.h"

namespace GlfwInput
{
    constexpr unsigned char Character(int key)
    {
        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        {
            return static_cast<unsigned char>('a' + key - GLFW_KEY_A);
        }
        if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9)
        {
            return static_cast<unsigned char>('0' + key - GLFW_KEY_0);
        }
        if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9)
        {
            return static_cast<unsigned char>('0' + key - GLFW_KEY_KP_0);
        }
        switch (key)
        {
            case GLFW_KEY_ENTER:
            case GLFW_KEY_KP_ENTER:
                return 13;
            case GLFW_KEY_ESCAPE:
                return 27;
            case GLFW_KEY_SPACE:
                return ' ';
            default:
                return 0;
        }
    }

    constexpr SpecialKey Special(int key)
    {
        switch (key)
        {
            case GLFW_KEY_UP:
                return SpecialKey::Up;
            case GLFW_KEY_DOWN:
                return SpecialKey::Down;
            case GLFW_KEY_LEFT:
                return SpecialKey::Left;
            case GLFW_KEY_RIGHT:
                return SpecialKey::Right;
            case GLFW_KEY_F7:
                return SpecialKey::ReloadModels;
            case GLFW_KEY_F8:
                return SpecialKey::ToggleModels;
            default:
                return SpecialKey::None;
        }
    }
} // namespace GlfwInput
