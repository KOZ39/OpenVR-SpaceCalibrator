#pragma once

#include "platform.h"
#include <inttypes.h>
#include <string>
#include <filesystem>
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

        typedef uintptr_t TextureHandle_t;
        constexpr TextureHandle_t k_INVALID_TEXTURE_HANDLE = (TextureHandle_t)-1;

        struct TextureData_t {
            TextureHandle_t hTexture = k_INVALID_TEXTURE_HANDLE;
            uint32_t dwWidth = 0;
            uint32_t dwHeight = 0;
            uintptr_t hInternalData = (uintptr_t)-1; // index, for vulkan
        };

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

            virtual TextureData_t loadTexture(const std::string& szFilePath) = 0;
            inline TextureData_t loadTexture(const std::filesystem::path& path) { return loadTexture(path.string()); }
            virtual void destroyTexture(TextureData_t hTexture) = 0;
        };

        bool isGraphicsApiSupported(GraphicsBackend gfxApi);
        bool isYFlipped(GraphicsBackend gfxApi);
        IRenderContext* getRenderContext(GraphicsBackend gfxApi);
        void shutdownRenderer();
    }
}