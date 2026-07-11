#include "window.h"
#include "renderer/renderer.h"
#include "user_interface.h"
#include "stb_image.h"
#include "platform.h"
#include "vr_core.h"
#include "log.h"
#include "util.h"

#include <filesystem>
#include <fmt/format.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include "imgui_extensions.h"

#if OS_WINDOWS
#include <dwmapi.h>
#endif

namespace spacecal {

#if OS_WINDOWS
    enum DWMA_USE_IMMSERSIVE_DARK_MODE_ENUM {
        DWMA_USE_IMMERSIVE_DARK_MODE = 20,
        DWMA_USE_IMMERSIVE_DARK_MODE_PRE_20H1 = 19,
    };

    const bool EnableDarkModeTopBar(const HWND windowHwmd) {
        const BOOL darkBorder = TRUE;
        return SUCCEEDED(DwmSetWindowAttribute(windowHwmd, DWMA_USE_IMMERSIVE_DARK_MODE, &darkBorder, sizeof(darkBorder)))
            || SUCCEEDED(DwmSetWindowAttribute(windowHwmd, DWMA_USE_IMMERSIVE_DARK_MODE_PRE_20H1, &darkBorder, sizeof(darkBorder)));
    }
#endif

    void GLFWErrorCallback(int error, const char* description) {
        LOG_ERROR("GLFW Error ({}): {}", error, description);
    }

    bool Window::CreateNativeWindow(renderer::GraphicsBackend gfxApi) {
        if (!glfwInit()) {
            const char* szGlfwError = nullptr;
            glfwGetError(&szGlfwError);
            LOG_FATAL("Failed to initialise GLFW, got {}.", szGlfwError);
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "Failed to initialize GLFW");
            return false;
        }
        
        m_desiredGraphicsBackend = gfxApi;
        if (!renderer::isGraphicsApiSupported(gfxApi)) {
            LOG_FATAL("Unsupported graphics API is selected! Falling back to OpenGL...");
        }
        m_isYFlipped = renderer::isYFlipped(gfxApi);
        m_graphicsContext = renderer::getRenderContext(m_desiredGraphicsBackend);
        ASSERT(m_graphicsContext != nullptr, "Render context must be valid!");

        glfwSetErrorCallback(GLFWErrorCallback);

        // multiple render backends require different hinting params
        m_graphicsContext->enableRendererGlfwHints();
        glfwWindowHint(GLFW_RESIZABLE, false);

        m_glfwWindow = glfwCreateWindow(m_windowWidth, m_windowHeight, "Space Calibrator", nullptr, nullptr);
        if (!m_glfwWindow) {
            LOG_FATAL("Failed to create GLFW window");
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "Failed to create GLFW window");
            return false;
        }

        constexpr const char* k_GRAPHICS_API_STRINGS[] = {
            "DirectX11",
            "Vulkan",
            "OpenGL",
        };

        if (!m_graphicsContext->initialiseGraphicsApi(m_glfwWindow)) {
            std::string szApiInitFail = fmt::format("Failed to load {}", k_GRAPHICS_API_STRINGS[(uint32_t)gfxApi]);
            LOG_FATAL("{}", szApiInitFail);
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", szApiInitFail);
            return false;
        }

        std::string szApiInitFail = fmt::format("Initialised {} renderer!", k_GRAPHICS_API_STRINGS[(uint32_t)gfxApi]);
        
        // set debug string if necessary
#if _DEBUG
        std::string szDebugTitle = fmt::format("Space Calibrator - {}", k_GRAPHICS_API_STRINGS[(uint32_t)gfxApi]);
        glfwSetWindowTitle(m_glfwWindow, szDebugTitle.c_str());
#endif

        // Minimise the window
        glfwWindowHint(GLFW_VISIBLE, false);

#if OS_WINDOWS
        HWND windowHwnd = glfwGetWin32Window(m_glfwWindow);
        EnableDarkModeTopBar(windowHwnd);
#endif

        // Load icon and set it in the window
        GLFWimage images[1] = {};
        std::string iconPath = (platform::getExeDir() / "taskbar_icon.png").string();
        images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4);
        glfwSetWindowIcon(m_glfwWindow, 1, images);
        stbi_image_free(images[0].pixels);

        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.IniFilename = nullptr;

        // load resources
        ImFontConfig cfg;
        // io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 24.0f, &cfg);
        // @FIXME: embed fonts later

        static const ImWchar k_RANGE_CORE_LATIN[] = { 0x0020, 0x00FF, 0, };

        // default font
        std::string fontPath = (platform::getExeDir() / "assets" / "fonts" / "Poppins-Regular.ttf").string();
        cfg.MergeMode = false;
        ImGui::fonts::pDefault = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f, &cfg);

        fontPath = (platform::getExeDir() / "assets" / "fonts" / "MPLUS1p-Regular.ttf").string();
        cfg.GlyphExcludeRanges = k_RANGE_CORE_LATIN;
        cfg.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f, &cfg);

        // bold font
        fontPath = (platform::getExeDir() / "assets" / "fonts" / "Poppins-Bold.ttf").string();
        cfg.GlyphExcludeRanges = NULL;
        cfg.MergeMode = false;
        ImGui::fonts::pHeading = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f, &cfg);

        fontPath = (platform::getExeDir() / "assets" / "fonts" / "MPLUS1p-Bold.ttf").string();
        cfg.GlyphExcludeRanges = k_RANGE_CORE_LATIN;
        cfg.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f, &cfg);

        ImGui_ImplGlfw_InitForOther(m_glfwWindow, true);
        m_graphicsContext->initialiseWindow(m_glfwWindow, m_windowWidth, m_windowHeight);

        ImGui::StyleColorsDark();
        SetupImGuiStyle();

        return true;
    }

    void Window::SetupImGuiStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 10;
        
        // colours
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Button] = ImVec4(0.74f, 0.74f, 0.74f, 0.24f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.74f, 0.74f, 0.74f, 0.40f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.74f, 0.74f, 0.74f, 0.51f);


    }

    void Window::Shutdown() {
        m_graphicsContext->shutdown();
        ImGui_ImplGlfw_Shutdown();

        ImPlot::DestroyContext();
        ImGui::DestroyContext();

        if (m_glfwWindow)
            glfwDestroyWindow(m_glfwWindow);

        glfwTerminate();
    }

    void Window::RunLoop() {
        VRState::getInstance()->updateVrState();
        CalibrationManager::getInstance()->init();

        double lastFrameStartTime = glfwGetTime();
        bool bKeyboardOpen = false;
        bool bKeyboardJustClosed = false;
        char textBuf[0x400] = {};

        while (!glfwWindowShouldClose(m_glfwWindow))
        {
            double time = glfwGetTime();
            VRState::getInstance()->updateVrState();
            CalibrationManager::getInstance()->calibrationTick(time);

            bool dashboardVisible = false;
            int width = 0, height = 0;
            glfwGetFramebufferSize(m_glfwWindow, &width, &height);
            const bool windowVisible = (width > 0 && height > 0);

            if (VRState::getInstance()->getOverlayHandle()) {
                vr::VROverlayHandle_t hOverlayHandle = VRState::getInstance()->getOverlayHandle();
                // @TODO: SteamVR overlay handling
                auto& io = ImGui::GetIO();
                dashboardVisible = vr::VROverlay()->IsActiveDashboardOverlay(hOverlayHandle);

                // @FIXME: This is currently hard-coded for windows as i copy pasted this from the 1.5.1 codebase
                //         need to figure out if imgui handles UTF8 directly and SteamVR does UTF8 directly, and adjust code accordingly
                //         also abstract the code into platform_win32 and platform_linux

                // After closing the keyboard, this code waits one frame for ImGui to pick up the new text from SetActiveText
                // before clearing the active widget. Then it waits another frame before allowing the keyboard to open again,
                // otherwise it will do so instantly since WantTextInput is still true on the second frame.
                if (bKeyboardJustClosed && bKeyboardOpen) {
                    ImGui::ClearActiveID();
                    bKeyboardOpen = false;
                } else if (bKeyboardJustClosed) {
                    bKeyboardJustClosed = false;
                } else if (!io.WantTextInput) {
                    // User might close the keyboard without hitting Done, so we unset the flag to allow it to open again.
                    bKeyboardOpen = false;
                } else if (io.WantTextInput && !bKeyboardOpen && !bKeyboardJustClosed) {
                    int id = ImGui::GetActiveID();
                    auto textInfo = ImGui::GetInputTextState(id);

                    if (textInfo != nullptr) {
#if 0
                        // @TODO: do we even need this? imgui and openvr afaik are both utf8, so no need to convert encoding between the two
                        textBuf[0] = 0;
                        int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)textInfo->TextA.Data, textInfo->TextA.Size, textBuf, sizeof(textBuf), nullptr, nullptr);
                        textBuf[std::min(static_cast<size_t>(len), sizeof(textBuf) - 1)] = 0;
#endif                   
                        memset(textBuf, 0, sizeof(textBuf));
                        size_t dwTextLength = std::min(static_cast<size_t>(textInfo->TextLen), sizeof(textBuf) - 1);
                        memcpy(textBuf, textInfo->TextA.Data, dwTextLength);


                        uint32_t unFlags = vr::EKeyboardFlags::KeyboardFlag_Minimal | vr::EKeyboardFlags::KeyboardFlag_ShowArrowKeys; // EKeyboardFlags 

                        vr::EVROverlayError err = vr::VROverlay()->ShowKeyboardForOverlay(
                            hOverlayHandle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
                            unFlags, "Space Calibrator Overlay", sizeof(textBuf), textBuf, 0
                        );
                        bKeyboardOpen = err == vr::EVROverlayError::VROverlayError_None;
                    }
                }

                vr::VREvent_t vrEvent;
                while (vr::VROverlay()->PollNextOverlayEvent(hOverlayHandle, &vrEvent, sizeof(vrEvent)))
                {
                    switch (vrEvent.eventType) {
                    case vr::VREvent_MouseMove:
                        io.AddMousePosEvent(vrEvent.data.mouse.x, m_isYFlipped ? m_windowHeight - vrEvent.data.mouse.y : vrEvent.data.mouse.y);
                        break;
                    case vr::VREvent_MouseButtonDown:
                        io.AddMouseButtonEvent((vrEvent.data.mouse.button & vr::VRMouseButton_Left) == vr::VRMouseButton_Left ? 0 : 1, true);
                        break;
                    case vr::VREvent_MouseButtonUp:
                        io.AddMouseButtonEvent((vrEvent.data.mouse.button & vr::VRMouseButton_Left) == vr::VRMouseButton_Left ? 0 : 1, false);
                        break;
                    case vr::VREvent_ScrollDiscrete:
                    {
                        float x = vrEvent.data.scroll.xdelta * 360.0f * 8.0f;
                        float y = vrEvent.data.scroll.ydelta * 360.0f * 8.0f;
                        io.AddMouseWheelEvent(x, m_isYFlipped ? y : y);
                        break;
                    }
                    case vr::VREvent_KeyboardDone: {
                        uint32_t dwTextBufSize = vr::VROverlay()->GetKeyboardText(textBuf, sizeof(textBuf));

                        int id = ImGui::GetActiveID();
                        auto textInfo = ImGui::GetInputTextState(id);

                        textInfo->TextA.resize(dwTextBufSize + 1); // null terminator
                        memcpy(textInfo->TextA.Data, textBuf, dwTextBufSize);
                        textInfo->TextLen = dwTextBufSize;

#if 0
                        // @TODO: do we even need this? imgui and openvr afaik are both utf8, so no need to convert encoding between the two
                        int bufSize = MultiByteToWideChar(CP_UTF8, 0, textBuf, -1, nullptr, 0);
                        textInfo->TextA.resize(bufSize);
                        MultiByteToWideChar(CP_UTF8, 0, textBuf, -1, (LPWSTR)textInfo->TextA.Data, bufSize);
                        textInfo->TextLen = bufSize;
                        textInfo->TextLen = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)textInfo->TextA.Data, textInfo->TextA.Size, nullptr, 0, nullptr, nullptr);
#endif

                        bKeyboardJustClosed = true;
                        break;
                    }
                    case vr::VREvent_Quit:
                        return;
                    }
                }
            }

            if (windowVisible || dashboardVisible) {

                auto& io = ImGui::GetIO();

                // These change state now, so we must execute these before doing our own modifications to the io state for VR
                m_graphicsContext->newFrame();
                ImGui_ImplGlfw_NewFrame();

                io.DisplaySize = ImVec2((float)m_windowWidth, (float)m_windowHeight);
                io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

                io.ConfigFlags = io.ConfigFlags & ~ImGuiConfigFlags_NoMouseCursorChange;
                if (dashboardVisible) {
                    io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NoMouseCursorChange;
                }

                ImGui::NewFrame();

                spacecal::drawInterface(dashboardVisible);

#ifdef _DEBUG
                // ImGui::ShowDemoWindow();
#endif

                ImGui::EndFrame();
                ImGui::Render();

                m_graphicsContext->renderDrawData(ImGui::GetDrawData(), ImVec4(0.0f, 0.0f, 0.0f, 1.0f));

                // if window is not minimised
                if (width && height) {
                    m_graphicsContext->present(width, height);
                }

                // update vr dashboard if viisble
                if (dashboardVisible) {
                    vr::Texture_t vrTexture = m_graphicsContext->getVRTexture();

                    vr::HmdVector2_t mouseScale = { .v = { (float)m_windowWidth, (float)m_windowHeight } };

                    vr::VROverlay()->SetOverlayTexture(VRState::getInstance()->getOverlayHandle(), &vrTexture);
                    vr::VROverlay()->SetOverlayMouseScale(VRState::getInstance()->getOverlayHandle(), &mouseScale);
                }
            }

            constexpr double dashboardInterval = 1.0 / 90.0; // fps
            double waitEventsTimeout = std::max(CalibrationManager::getInstance()->getWantedUpdateInterval(), dashboardInterval);
            
            if (dashboardVisible && waitEventsTimeout > dashboardInterval)
                waitEventsTimeout = dashboardInterval;

            glfwWaitEventsTimeout(waitEventsTimeout);

            // If we're minimized rendering won't limit our frame rate so we need to do it ourselves.
            if (glfwGetWindowAttrib(m_glfwWindow, GLFW_ICONIFIED)) {
                double targetFrameTime = 1 / k_MINIMIZED_MAX_FPS;
                double waitTime = targetFrameTime - (glfwGetTime() - lastFrameStartTime);
                if (waitTime > 0) {
                    std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
                }

                lastFrameStartTime += targetFrameTime;
            }
        }

        CalibrationManager::getInstance()->shutdown();
    }
}