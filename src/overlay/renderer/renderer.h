#pragma once

#include "platform.h"
#include <inttypes.h>
// tell GLFW to NOT include OpenGL as it'll conflict with GLAD in the OpenGL backend
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <openvr.h>

namespace spacecal {
    namespace renderer {

        enum class GraphicsBackend {
            DirectX11,
            Vulkan,
            OpenGL
        };

        typedef uintptr_t RenderHandle_t;
        constexpr RenderHandle_t k_INVALID_RENDER_HANDLE = (RenderHandle_t)-1;

        class IRenderContext {
        public:
            virtual ~IRenderContext() = default;
            
            virtual void enableRendererGlfwHints() = 0;
            virtual bool initialiseGraphicsApi(GLFWwindow* window) = 0;
            virtual bool initialiseWindow(GLFWwindow* window, int width, int height) = 0;
            virtual void shutdown() = 0;
            
            virtual void newFrame() = 0;
            virtual void renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) = 0;
            
            virtual void present(int width, int height) = 0;
            
            virtual vr::Texture_t getVRTexture() = 0;
        };

        bool isGraphicsApiSupported(GraphicsBackend gfxApi);
        bool isYFlipped(GraphicsBackend gfxApi);
        IRenderContext* getRenderContext(GraphicsBackend gfxApi);
        void shutdownRenderer();
    }
}