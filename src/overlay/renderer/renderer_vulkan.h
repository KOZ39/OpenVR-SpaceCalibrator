#pragma once

#include <volk.h>

#include "renderer.h"

#include <imgui/backends/imgui_impl_vulkan.h>

namespace spacecal {
    namespace renderer {
        class Renderer_Vulkan : public IRenderContext {
        public:
            ~Renderer_Vulkan() override;

            void enableRendererGlfwHints() override;
            bool initialiseGraphicsApi(GLFWwindow* window) override;
            bool initialiseWindow(GLFWwindow* window, int width, int height) override;
            void shutdown() override;

            void newFrame() override;
            void renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) override;

            void present(int width, int height) override;

            vr::Texture_t getVRTexture() override;
            
        private:
            bool setupVulkan(ImVector<const char*> instance_extensions);
            bool getOpenvrVulkanInstanceExtensionsRequired(std::vector<std::string>& outInstanceExtensionList);
            bool getOpenvrVulkanDeviceExtensionsRequired(VkPhysicalDevice pPhysicalDevice, std::vector<std::string>& outDeviceExtensionList);
        private:
            GLFWwindow* m_window = nullptr;
            int m_width = 0;
            int m_height = 0;

            VkDebugReportCallbackEXT m_debugReport = VK_NULL_HANDLE;
            VkAllocationCallbacks*   m_allocator = nullptr;
            VkInstance               m_instance = VK_NULL_HANDLE;
            VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
            VkDevice                 m_device = VK_NULL_HANDLE;
            uint32_t                 m_queueFamily = (uint32_t)-1;
            VkQueue                  m_queue = VK_NULL_HANDLE;
            VkPipelineCache          m_pipelineCache = VK_NULL_HANDLE;
            VkDescriptorPool         m_descriptorPool = VK_NULL_HANDLE;

            ImGui_ImplVulkanH_Window m_mainWindowData = {};
            uint32_t                 m_minImageCount = 2;
            bool                     m_swapChainRebuild = false;

            vr::VRVulkanTextureData_t m_vrTextureData = {};
        };
    }
}