#pragma once

#include <glad/glad.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "ipc_client.h"
#include "calibration.h"

namespace spacecal {
    // handles window creation, setting the icon, and initialising a renderer for UI, also handles connecting it to SteamVR as an overlay
    class Window {
    public:
        bool CreateNativeWindow();
        void Shutdown();
        void RunLoop();

    private:
        GLFWwindow* m_glfwWindow = nullptr;
        GLuint m_fboHandle = 0;
        GLuint m_fboTextureHandle = 0;
        int m_fboTextureWidth = 1200;
        int m_fboTextureHeight = 800;

        const float k_MINIMIZED_MAX_FPS = 60.0f;
    };
}