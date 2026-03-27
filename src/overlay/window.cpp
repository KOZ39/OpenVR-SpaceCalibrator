#include "window.h"
#include "user_interface.h"
#include "stb_image.h"
#include "platform.h"
#include "vr_core.h"
#include "log.h"

#include <filesystem>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

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
        const bool ok =
            SUCCEEDED(DwmSetWindowAttribute(windowHwmd, DWMA_USE_IMMERSIVE_DARK_MODE, &darkBorder, sizeof(darkBorder)))
            || SUCCEEDED(DwmSetWindowAttribute(windowHwmd, DWMA_USE_IMMERSIVE_DARK_MODE_PRE_20H1, &darkBorder, sizeof(darkBorder)));
        return ok;
    }
#endif

    void GLFWErrorCallback(int error, const char* description) {
        LOG_ERROR("GLFW Error ({}): {}", error, description);
    }

    void OpenGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
        std::string_view szGlMessage(message, length);
        switch (severity) {
        case GL_DEBUG_SEVERITY_LOW:
            LOG_INFO("OpenGL (type: {} id: {}): {}", type, id, szGlMessage);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            LOG_WARN("OpenGL (type: {} id: {}): {}", type, id, szGlMessage);
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            LOG_ERROR("OpenGL (type: {} id: {}): {}", type, id, szGlMessage);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            LOG_DEBUG("OpenGL (type: {} id: {}): {}", type, id, szGlMessage);
            break;
        }
    }

    bool Window::CreateNativeWindow() {
        if (!glfwInit())
        {
            const char* szGlfwError = nullptr;
            glfwGetError(&szGlfwError);
            LOG_FATAL("Failed to initialise GLFW, got {}.", szGlfwError);
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "Failed to initialize GLFW");
            return false;
        }

        glfwSetErrorCallback(GLFWErrorCallback);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, false);

#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

        m_glfwWindow = glfwCreateWindow(m_fboTextureWidth, m_fboTextureHeight, "Space Calibrator", nullptr, nullptr);
        if (!m_glfwWindow) {
            LOG_FATAL("Failed to create GLFW window");
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "Failed to create GLFW window");
            return false;
        }

        glfwMakeContextCurrent(m_glfwWindow);
        glfwSwapInterval(1);
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            LOG_FATAL("Failed to load OpenGL");
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "Failed to load OpenGL");
            return false;
        }

        // Minimise the window
        glfwIconifyWindow(m_glfwWindow);
#if OS_WINDOWS
        HWND windowHwmd = glfwGetWin32Window(m_glfwWindow);
        EnableDarkModeTopBar(windowHwmd);
#endif

        // Load icon and set it in the window
        GLFWimage images[1] = {};
        std::string iconPath = (std::filesystem::current_path() / "taskbar_icon.png").string();
        images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4);
        glfwSetWindowIcon(m_glfwWindow, 1, images);
        stbi_image_free(images[0].pixels);

#ifdef _DEBUG
        glDebugMessageCallback(OpenGLDebugCallback, nullptr);
        glEnable(GL_DEBUG_OUTPUT);
#endif

        ImGui::CreateContext();
        // ImPlot::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.IniFilename = nullptr;

        // load resources
        // io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 24.0f);

        ImGui_ImplGlfw_InitForOpenGL(m_glfwWindow, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        ImGui::StyleColorsDark();

        glGenTextures(1, &m_fboTextureHandle);
        glBindTexture(GL_TEXTURE_2D, m_fboTextureHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_fboTextureWidth, m_fboTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glGenFramebuffers(1, &m_fboHandle);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fboHandle);
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_fboTextureHandle, 0);

        GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        glDrawBuffers(1, drawBuffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            LOG_FATAL("OpenGL framebuffer incomplete.");
            platform::showMessageDialog("An error occured initialising Space Calibrator Nova", "OpenGL framebuffer incomplete");
            return false;
        }

        return true;
    }

    void Window::Shutdown() {
        if (m_fboHandle)
            glDeleteFramebuffers(1, &m_fboHandle);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        // ImPlot::DestroyContext();
        ImGui::DestroyContext();

        if (m_glfwWindow)
            glfwDestroyWindow(m_glfwWindow);

        glfwTerminate();
    }

    void Window::RunLoop() {
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
                        textBuf[0] = 0;
                        int len = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)textInfo->TextA.Data, textInfo->TextA.Size, textBuf, sizeof(textBuf), nullptr, nullptr);
                        textBuf[std::min(static_cast<size_t>(len), sizeof(textBuf) - 1)] = 0;

                        uint32_t unFlags = 0; // EKeyboardFlags 

                        vr::VROverlay()->ShowKeyboardForOverlay(
                            hOverlayHandle, vr::k_EGamepadTextInputModeNormal, vr::k_EGamepadTextInputLineModeSingleLine,
                            unFlags, "Space Calibrator Overlay", sizeof(textBuf), textBuf, 0
                        );
                        bKeyboardOpen = true;
                    }
                }

                vr::VREvent_t vrEvent;
                while (vr::VROverlay()->PollNextOverlayEvent(hOverlayHandle, &vrEvent, sizeof(vrEvent)))
                {
                    switch (vrEvent.eventType) {
                    case vr::VREvent_MouseMove:
                        io.AddMousePosEvent(vrEvent.data.mouse.x, vrEvent.data.mouse.y);
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
                        io.AddMouseWheelEvent(x, y);
                        break;
                    }
                    case vr::VREvent_KeyboardDone: {
                        vr::VROverlay()->GetKeyboardText(textBuf, sizeof(textBuf));

                        int id = ImGui::GetActiveID();
                        auto textInfo = ImGui::GetInputTextState(id);
                        int bufSize = MultiByteToWideChar(CP_UTF8, 0, textBuf, -1, nullptr, 0);
                        textInfo->TextA.resize(bufSize);
                        MultiByteToWideChar(CP_UTF8, 0, textBuf, -1, (LPWSTR)textInfo->TextA.Data, bufSize);
                        textInfo->TextLen = bufSize;
                        textInfo->TextLen = WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)textInfo->TextA.Data, textInfo->TextA.Size, nullptr, 0, nullptr, nullptr);

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
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();

                io.DisplaySize = ImVec2((float)m_fboTextureWidth, (float)m_fboTextureHeight);
                io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

                io.ConfigFlags = io.ConfigFlags & ~ImGuiConfigFlags_NoMouseCursorChange;
                if (dashboardVisible) {
                    io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NoMouseCursorChange;
                }

                ImGui::NewFrame();

                spacecal::drawInterface(dashboardVisible);

                ImGui::EndFrame();
                ImGui::Render();

                glBindFramebuffer(GL_FRAMEBUFFER, m_fboHandle);
                glViewport(0, 0, m_fboTextureWidth, m_fboTextureHeight);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);

                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

                glBindFramebuffer(GL_FRAMEBUFFER, 0);

                // if window is not minimised
                if (width && height) {
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboHandle);
                    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                    glfwSwapBuffers(m_glfwWindow);
                }

                // update vr dashboard if viisble
                if (dashboardVisible) {
                    vr::Texture_t vrTexture = {
                        .handle = (void*) (uintptr_t) m_fboTextureHandle,
                        .eType = vr::TextureType_OpenGL,
                        .eColorSpace = vr::ColorSpace_Auto,
                    };

                    vr::HmdVector2_t mouseScale = { .v = { (float)m_fboTextureWidth, (float)m_fboTextureHeight } };

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