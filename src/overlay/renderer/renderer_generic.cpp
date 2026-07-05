#include "renderer.h"
#include "renderer_opengl.h"
#include "renderer_vulkan.h"
#include <cstdint>
#if OS_WINDOWS
#include "renderer_dx11.h"
#endif // OS_WINDOWS
#include "log.h"

namespace spacecal {
    namespace renderer {

        static IRenderContext* g_renderContext = nullptr;

        bool isGraphicsApiSupported(GraphicsBackend gfxApi) {
            switch (gfxApi) {
            case GraphicsBackend::OpenGL: return true;
            case GraphicsBackend::Vulkan: return true;
#if OS_WINDOWS
            case GraphicsBackend::DirectX11: return true;
#endif
            default: return false;
            }
        }
        
        bool isYFlipped(GraphicsBackend gfxApi) {
            switch (gfxApi) {
            case GraphicsBackend::OpenGL: return false;
            case GraphicsBackend::Vulkan: return true;
#if OS_WINDOWS
            case GraphicsBackend::DirectX11: return true;
#endif
            default: return false;
            }
        }

        IRenderContext* getRenderContext(GraphicsBackend gfxApi) {
            if (g_renderContext) {
                return g_renderContext;
            }

            switch (gfxApi)
            {
#if OS_WINDOWS
            case spacecal::renderer::GraphicsBackend::DirectX11:
                g_renderContext = new Renderer_DX11();
                break;
#endif // OS_WINDOWS
            case spacecal::renderer::GraphicsBackend::Vulkan:
                g_renderContext = new Renderer_Vulkan();
                break;
            case spacecal::renderer::GraphicsBackend::OpenGL:
                g_renderContext = new Renderer_OpenGL();
                break;
            default:
                LOG_FATAL("Unknown graphics backend {} selected! Defaulting to OpenGL...", (uint32_t)gfxApi);
                g_renderContext = new Renderer_OpenGL();
                break;
            }

            return g_renderContext;
        }

        void shutdownRenderer() {
            if (g_renderContext) {
                delete g_renderContext;
            }
        }
    }
}