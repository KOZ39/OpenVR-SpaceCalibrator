#pragma once

#include "renderer.h"

#if OS_WINDOWS

#include <d3d11.h>

namespace spacecal {
    namespace renderer {

#define SAFE_RELEASE(x) if (x) { x->Release(); x = nullptr; }
        constexpr uint64_t k_DEVICE_CREATION_AUTO_LUID = 0xFFFFFFFFFFFFFFFF;

        class Renderer_DX11 : public IRenderContext {
        public:
            ~Renderer_DX11() override;

            void enableRendererGlfwHints() override;
            bool initialiseGraphicsApi(GLFWwindow* window) override;
            bool initialiseWindow(GLFWwindow* window, int width, int height) override;
            void shutdown() override;

            void newFrame() override;
            void renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) override;

            void present(int width, int height) override;

            vr::Texture_t getVRTexture() override;
        private:
            IDXGIAdapter* get_dxgi_adapter_by_index_or_luid(int32_t adapterIndex, uint64_t dwLuid);
            bool create_d3d_device(HWND hWnd);
            void cleanup_d3d_device();
            void create_render_target();
            void cleanup_render_target();
        private:
            GLFWwindow* m_window = nullptr;

            int m_width = 0;
            int m_height = 0;

            HWND m_windowHwnd                               = (HWND) INVALID_HANDLE_VALUE;
            IDXGIFactory* m_pdxgiFactory                    = nullptr;
            IDXGIAdapter* m_pdxgiAdapter                    = nullptr;
            ID3D11Device* m_pd3dDevice                      = nullptr;
            ID3D11DeviceContext* m_pd3dDeviceContext        = nullptr;
            IDXGISwapChain* m_pSwapChain                    = nullptr;
            ID3D11Texture2D* m_pBackBuffer                  = nullptr;
            ID3D11RenderTargetView* m_mainRenderTargetView  = nullptr;
            bool m_SwapChainOccluded                        = false;
        };
    }
}

#endif // OS_WINDOWS