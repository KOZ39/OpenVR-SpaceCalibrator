#include "platform.h"

#if OS_WINDOWS

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "renderer_dx11.h"

#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include "log.h"
#include "util.h"

namespace spacecal {
    namespace renderer {
        Renderer_DX11::~Renderer_DX11() {}

        void Renderer_DX11::enableRendererGlfwHints() {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        }

        bool Renderer_DX11::initialiseGraphicsApi(GLFWwindow* window) {
            return true;
        }

        bool Renderer_DX11::initialiseWindow(GLFWwindow* window, int width, int height) {
            ASSERT(window != nullptr, "window must not be null!");

            m_window = window;
            m_width = width;
            m_height = height;

            // Initialize Direct3D
            m_windowHwnd = glfwGetWin32Window(m_window);
            if (!create_d3d_device(m_windowHwnd))
            {
                cleanup_d3d_device();
                return false;
            }

            ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);

            return true;
        }

        void Renderer_DX11::shutdown() {
            ImGui_ImplDX11_Shutdown();
            cleanup_d3d_device();
        }

        void Renderer_DX11::newFrame() {
            ImGui_ImplDX11_NewFrame();
        }

        void Renderer_DX11::renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) {
            ASSERT(drawData != nullptr, "drawData must not be null!");
            const float clear_color_with_alpha[4] = { clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w };
            m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
            m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(drawData);
        }

        void Renderer_DX11::present(int width, int height) {

            HRESULT hr = m_pSwapChain->Present(1, 0);   // Present with vsync
            //HRESULT hr = m_pSwapChain->Present(0, 0); // Present without vsync
            m_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

            // Handle window being minimized or screen locked
            if (m_SwapChainOccluded && m_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED && false) {
                ::Sleep(10);
                // continue;
            }
            m_SwapChainOccluded = false;

            // Handle window resize (we don't resize directly in the WM_SIZE handler)
            if (width != m_width || height != m_height)
            {
                if (width > 0 && height > 0) {
                    cleanup_render_target();
                    m_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                    m_width = width;
                    m_height = height;
                    create_render_target();
                }
            }
        }

        vr::Texture_t Renderer_DX11::getVRTexture() {
            vr::Texture_t vrTexture = {
                .handle = (void*)m_pBackBuffer,
                .eType = vr::ETextureType::TextureType_DirectX,
                .eColorSpace = vr::EColorSpace::ColorSpace_Auto,
            };
            return vrTexture;
        }

        IDXGIAdapter* Renderer_DX11::get_dxgi_adapter_by_index_or_luid(int32_t adapterIndex, uint64_t dwLuid) {
            HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&m_pdxgiFactory);
            if (FAILED(hr)) {
                LOG_FATAL("Failed to create DXGI Factory");
                return nullptr;
            }

            bool searchForAdapterGivenLuid = false;

            if (adapterIndex < 0) {
                adapterIndex = 0;
                searchForAdapterGivenLuid = dwLuid != k_DEVICE_CREATION_AUTO_LUID;
            }

            if (searchForAdapterGivenLuid) {
                LUID adapterLuid = {
                    .LowPart = (DWORD)(dwLuid & 0xFFFFFFFF),
                    .HighPart = (LONG)(dwLuid >> 32),
                };

                IDXGIAdapter* pAdapterTmp = nullptr;
                // Loop through all adapters until we find the one given a LUID
                for (UINT i = 0; m_pdxgiFactory->EnumAdapters(i, &pAdapterTmp) != DXGI_ERROR_NOT_FOUND; ++i) {
                    DXGI_ADAPTER_DESC aDesc;
                    pAdapterTmp->GetDesc(&aDesc);

                    if (aDesc.AdapterLuid.HighPart == adapterLuid.HighPart &&
                        aDesc.AdapterLuid.LowPart == adapterLuid.LowPart) {
                        m_pdxgiAdapter = pAdapterTmp;
                        break;
                    } else {
                        // Current adapter is invalid. Skip it.
                        SAFE_RELEASE(pAdapterTmp);
                    }
                }

                if (pAdapterTmp == nullptr) {
                    LOG_ERROR("The specified DXGI adapter with LUID {0}.{1} does not exist.", adapterLuid.LowPart, adapterLuid.HighPart);
                    SAFE_RELEASE(m_pdxgiAdapter);
                    return nullptr;
                }
            } else {
                if (FAILED(m_pdxgiFactory->EnumAdapters(adapterIndex, &m_pdxgiAdapter))) {
                    if (adapterIndex == 0)
                        LOG_ERROR("Cannot find any DXGI adapters in the system.");
                    else
                        LOG_ERROR("The specified DXGI adapter {0} does not exist.", adapterIndex);
                    SAFE_RELEASE(m_pdxgiAdapter);
                    return nullptr;
                }
            }

            return m_pdxgiAdapter;
        }

        bool Renderer_DX11::create_d3d_device(HWND hWnd) {

            // we try getting the same gpu and adapter steamvr used to minimise the odds of failures
            uint64_t openVrLuidHandle = 0;
            int32_t openVrAdapterIndex = 0;
            if (vr::VRSystem()) {
                vr::VRSystem()->GetOutputDevice(&openVrLuidHandle, vr::ETextureType::TextureType_DirectX);
                vr::VRSystem()->GetDXGIOutputInfo(&openVrAdapterIndex);
            }

            IDXGIAdapter* pDxgiAdapter = get_dxgi_adapter_by_index_or_luid(openVrAdapterIndex, openVrLuidHandle);

            // Setup swap chain
            // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
            DXGI_SWAP_CHAIN_DESC sd;
            ZeroMemory(&sd, sizeof(sd));
            sd.BufferCount = 2;
            sd.BufferDesc.Width = 0;
            sd.BufferDesc.Height = 0;
            sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            sd.BufferDesc.RefreshRate.Numerator = 60;
            sd.BufferDesc.RefreshRate.Denominator = 1;
            sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
            sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            sd.OutputWindow = hWnd;
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
            sd.Windowed = TRUE;
            sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            UINT createDeviceFlags = 0;
#if _DEBUG
            createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
            D3D_FEATURE_LEVEL featureLevel;
            const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
            const D3D_DRIVER_TYPE driverType = pDxgiAdapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN;
            HRESULT res = D3D11CreateDeviceAndSwapChain(pDxgiAdapter, driverType, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
            if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
                res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain, &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext);
            if (res != S_OK)
                return false;

            create_render_target();
            return true;
        }

        void Renderer_DX11::cleanup_d3d_device() {
            if (m_pd3dDeviceContext) {
                m_pd3dDeviceContext->ClearState();
                m_pd3dDeviceContext->Flush();
            }
            cleanup_render_target();
            SAFE_RELEASE(m_pSwapChain);
            SAFE_RELEASE(m_pd3dDeviceContext);
            SAFE_RELEASE(m_pd3dDevice);
            SAFE_RELEASE(m_pdxgiAdapter);
            SAFE_RELEASE(m_pdxgiFactory);
        }

        void Renderer_DX11::create_render_target() {
            HRESULT hr = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&m_pBackBuffer));
            if (SUCCEEDED(hr)) {
                m_pd3dDevice->CreateRenderTargetView(m_pBackBuffer, nullptr, &m_mainRenderTargetView);
            }
        }

        void Renderer_DX11::cleanup_render_target() {
            SAFE_RELEASE(m_mainRenderTargetView);
            SAFE_RELEASE(m_pBackBuffer);
        }
    }
}

#endif // OS_WINDOWS