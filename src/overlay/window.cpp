#include "window.h"
#include "log.h"
#include "user_interface.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>

namespace spacecal {
    bool Window::CreateNativeWindow() {
        if (!glfwInit())
        {
            const char* szGlfwError = nullptr;
            glfwGetError(&szGlfwError);
            LOG_FATAL("Failed to initialise GLFW, got {}.", szGlfwError);
            MessageBoxW(nullptr, L"Failed to initialize GLFW", L"An error occured initialising Space Calibrator Nova", 0);
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_RESIZABLE, false);

#ifdef _DEBUG
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif

        m_glfwWindow = glfwCreateWindow(m_fboTextureWidth, m_fboTextureHeight, "Space Calibrator", nullptr, nullptr);
        if (!m_glfwWindow) {
            LOG_FATAL("Failed to create GLFW window");
            MessageBoxW(nullptr, L"Failed to create GLFW window", L"An error occured initialising Space Calibrator Nova", 0);
            return false;
        }

        glfwMakeContextCurrent(m_glfwWindow);
        glfwSwapInterval(1);
        gladLoadGL();

        // Minimise the window
        glfwIconifyWindow(m_glfwWindow);
        HWND windowHwmd = glfwGetWin32Window(m_glfwWindow);
        // EnableDarkModeTopBar(windowHwmd);

        // Load icon and set it in the window
        // GLFWimage images[1] = {};
        // std::string iconPath = cwd;
        // iconPath += "\\taskbar_icon.png";
        // images[0].pixels = stbi_load(iconPath.c_str(), &images[0].width, &images[0].height, 0, 4);
        // glfwSetWindowIcon(glfwWindow, 1, images);
        // stbi_image_free(images[0].pixels);

#ifdef _DEBUG
        // glDebugMessageCallback(openGLDebugCallback, nullptr);
        // glEnable(GL_DEBUG_OUTPUT);
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
            MessageBoxW(nullptr, L"OpenGL framebuffer incomplete", L"An error occured initialising Space Calibrator Nova", 0);
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
        double lastFrameStartTime = glfwGetTime();
        while (!glfwWindowShouldClose(m_glfwWindow))
        {
            double time = glfwGetTime();

            int width, height;
            glfwGetFramebufferSize(m_glfwWindow, &width, &height);
            const bool windowVisible = (width > 0 && height > 0);

            auto& io = ImGui::GetIO();

            // These change state now, so we must execute these before doing our own modifications to the io state for VR
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();

            io.DisplaySize = ImVec2((float)m_fboTextureWidth, (float)m_fboTextureHeight);
            io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

            io.ConfigFlags = io.ConfigFlags & ~ImGuiConfigFlags_NoMouseCursorChange;
            // if (dashboardVisible) {
            // 	io.ConfigFlags = io.ConfigFlags | ImGuiConfigFlags_NoMouseCursorChange;
            // }

            ImGui::NewFrame();

            spacecal::DrawInterface();

            ImGui::EndFrame();
            ImGui::Render();

            glBindFramebuffer(GL_FRAMEBUFFER, m_fboHandle);
            glViewport(0, 0, m_fboTextureWidth, m_fboTextureHeight);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);

            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            if (width && height)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboHandle);
                glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glfwSwapBuffers(m_glfwWindow);
            }


            const double dashboardInterval = 1.0 / 90.0; // fps
            // double waitEventsTimeout = std::max(CalCtx.wantedUpdateInterval, dashboardInterval);
            double waitEventsTimeout = dashboardInterval;

            glfwWaitEventsTimeout(waitEventsTimeout);

            // If we're minimized rendering won't limit our frame rate so we need to do it ourselves.
            if (glfwGetWindowAttrib(m_glfwWindow, GLFW_ICONIFIED))
            {
                double targetFrameTime = 1 / k_MINIMIZED_MAX_FPS;
                double waitTime = targetFrameTime - (glfwGetTime() - lastFrameStartTime);
                if (waitTime > 0)
                {
                    std::this_thread::sleep_for(std::chrono::duration<double>(waitTime));
                }

                lastFrameStartTime += targetFrameTime;
            }
        }
    }
}