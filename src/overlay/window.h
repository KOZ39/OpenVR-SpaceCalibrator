#pragma once

#include "calibration.h"
#include "ipc_client.h"
#include "platform.h"


#if OS_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif // OS_WINDOWS
#include <GLFW/glfw3.h>
#if OS_WINDOWS
#include <GLFW/glfw3native.h>
#endif // OS_WINDOWS

#include "renderer/renderer.h"

namespace spacecal {

// handles window creation, setting the icon, and initialising a renderer for UI, also handles connecting it to SteamVR as an overlay
class Window {
public:
    bool CreateNativeWindow(renderer::GraphicsBackend gfxApi);
    void SetupImGuiStyle();
    void Shutdown();
    void RunLoop();

private:
    renderer::GraphicsBackend m_desiredGraphicsBackend = renderer::GraphicsBackend::OpenGL;
    renderer::IRenderContext* m_graphicsContext = nullptr;

    GLFWwindow* m_glfwWindow = nullptr;
    int m_windowWidth = 1580;
    int m_windowHeight = 950;

    bool m_isYFlipped = false;

    const float k_MINIMIZED_MAX_FPS = 60.0f;
};
}