#pragma once

#include <glad/glad.h>
#include "renderer.h"

namespace spacecal {
    namespace renderer {
        class Renderer_OpenGL: public IRenderContext {
        public:
            ~Renderer_OpenGL() override;

            void enableRendererGlfwHints() override;
            bool initialiseGraphicsApi(GLFWwindow* window) override;
            bool initialiseWindow(GLFWwindow* window, int width, int height) override;
            void shutdown() override;

            void newFrame() override;
            void renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) override;

            void present(int width, int height) override;

            vr::Texture_t getVRTexture() override;

            TextureData_t loadTexture(const std::string& szFilePath) override;
            void destroyTexture(TextureData_t hTexture) override;
        private:
            GLFWwindow* m_window = nullptr;
            GLuint m_fboHandle = 0;
            GLuint m_fboTextureHandle = 0;
            int m_width = 0;
            int m_height = 0;
        };
    }
}