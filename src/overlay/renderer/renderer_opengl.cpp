#include "renderer_opengl.h"
#include <imgui/backends/imgui_impl_opengl3.h>
#include "log.h"
#include "util.h"

namespace spacecal {
    namespace renderer {
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

        Renderer_OpenGL::~Renderer_OpenGL() {}

        void Renderer_OpenGL::enableRendererGlfwHints() {
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef _DEBUG
            glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#endif
        }

        bool Renderer_OpenGL::initialiseGraphicsApi(GLFWwindow* window) {
            glfwMakeContextCurrent(window);
            glfwSwapInterval(1);

            if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
                return false;
            }

#ifdef _DEBUG
            glDebugMessageCallback(OpenGLDebugCallback, nullptr);
            glEnable(GL_DEBUG_OUTPUT);
#endif

            return true;
        }

        bool Renderer_OpenGL::initialiseWindow(GLFWwindow* window, int width, int height) {
            ASSERT(window != nullptr, "window must not be null!");

            m_window = window;
            m_width = width;
            m_height = height;

            ImGui_ImplOpenGL3_Init("#version 330");

            glGenTextures(1, &m_fboTextureHandle);
            glBindTexture(GL_TEXTURE_2D, m_fboTextureHandle);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
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

            return false;
        }

        void Renderer_OpenGL::shutdown() {
            if (m_fboHandle) {
                glDeleteFramebuffers(1, &m_fboHandle);
                m_fboHandle = 0;
            }

            ImGui_ImplOpenGL3_Shutdown();
        }

        void Renderer_OpenGL::newFrame() {
            ImGui_ImplOpenGL3_NewFrame();
        }

        void Renderer_OpenGL::renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) {
            ASSERT(drawData != nullptr, "drawData must not be null!");

            glBindFramebuffer(GL_FRAMEBUFFER, m_fboHandle);
            glViewport(0, 0, m_width, m_height);
            glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
            glClear(GL_COLOR_BUFFER_BIT);
            
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        
        void Renderer_OpenGL::present(int width, int height) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fboHandle);
            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glfwSwapBuffers(m_window);
        }

        vr::Texture_t Renderer_OpenGL::getVRTexture() {
            vr::Texture_t vrTexture = {
                .handle = (void*) (uintptr_t) m_fboTextureHandle,
                .eType = vr::ETextureType::TextureType_OpenGL,
                .eColorSpace = vr::EColorSpace::ColorSpace_Auto,
            };
            return vrTexture;
        }
    }
}