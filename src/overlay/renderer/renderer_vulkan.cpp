#include "renderer_vulkan.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <volk.h>
#include "log.h"
#include "util.h"

namespace spacecal {
    namespace renderer {
        Renderer_Vulkan::~Renderer_Vulkan() {}

        [[nodiscard]] inline bool check_vk_result(VkResult err) {
            if (err == VK_SUCCESS) {
                return true;
            }
            LOG_ERROR("[vulkan] Error: VkResult = {}", err);
            if (err < 0) {
                return false;
            }
            return true;
        }

        void check_vk_result_callback(VkResult err) {
            if (err == VK_SUCCESS)
                return;
            LOG_ERROR("[vulkan] Error: VkResult = {}", err);
            if (err < 0)
                return;
        }

#if defined(_DEBUG) || defined(RENDER_USE_VULKAN_DEBUG_REPORT)
        VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
            (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
            LOG_ERROR("[vulkan] Debug report from ObjectType: {}\nMessage: {}\n", (uint32_t)objectType, pMessage);
            return VK_FALSE;
        }
#endif // RENDER_USE_VULKAN_DEBUG_REPORT

        bool Renderer_Vulkan::getOpenvrVulkanInstanceExtensionsRequired(std::vector<std::string>& outInstanceExtensionList) {
            if (!vr::VRCompositor()) {
                return false;
            }

            outInstanceExtensionList.clear();
            uint32_t nBufferSize = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(nullptr, 0);
            if (nBufferSize > 0) {
                // Allocate memory for the space separated list and query for it
                char* pExtensionStr = new char[nBufferSize];
                memset(pExtensionStr, 0, nBufferSize);
                vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(pExtensionStr, nBufferSize);

                // Break up the space separated list into entries on the CUtlStringList
                std::string curExtStr;
                uint32_t nIndex = 0;
                while ((nIndex < nBufferSize) && pExtensionStr[nIndex] != 0) {
                    if (pExtensionStr[nIndex] == ' ') {
                        outInstanceExtensionList.push_back(curExtStr);
                        curExtStr.clear();
                    } else {
                        curExtStr += pExtensionStr[nIndex];
                    }
                    nIndex++;
                }
                if (curExtStr.size() > 0) {
                    outInstanceExtensionList.push_back(curExtStr);
                }

                delete[] pExtensionStr;
            }

            return true;
        }

        bool Renderer_Vulkan::getOpenvrVulkanDeviceExtensionsRequired(VkPhysicalDevice pPhysicalDevice, std::vector<std::string>& outDeviceExtensionList) {
            if (!vr::VRCompositor()) {
                return false;
            }
             
            outDeviceExtensionList.clear();
            uint32_t nBufferSize = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired((VkPhysicalDevice_T*)pPhysicalDevice, nullptr, 0);
            if (nBufferSize > 0) {
                // Allocate memory for the space separated list and query for it
                char* pExtensionStr = new char[nBufferSize];
                memset(pExtensionStr, 0, nBufferSize);
                vr::VRCompositor()->GetVulkanDeviceExtensionsRequired((VkPhysicalDevice_T*)pPhysicalDevice, pExtensionStr, nBufferSize);

                // Break up the space separated list into entries on the CUtlStringList
                std::string curExtStr;
                uint32_t nIndex = 0;
                while ((nIndex < nBufferSize) && pExtensionStr[nIndex] != 0) {
                    if (pExtensionStr[nIndex] == ' ') {
                        outDeviceExtensionList.push_back(curExtStr);
                        curExtStr.clear();
                    } else {
                        curExtStr += pExtensionStr[nIndex];
                    }
                    nIndex++;
                }
                if (curExtStr.size() > 0) {
                    outDeviceExtensionList.push_back(curExtStr);
                }

                delete[] pExtensionStr;
            }

            return true;
        }

        inline bool isExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension) {
            for (const VkExtensionProperties& p : properties)
                if (strcmp(p.extensionName, extension) == 0)
                    return true;
            return false;
        }

        void Renderer_Vulkan::enableRendererGlfwHints() {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            // @TODO: vulkan glfw hints
        }

        bool Renderer_Vulkan::initialiseGraphicsApi(GLFWwindow* window) {
            VkResult eResult = volkInitialize();
            if (eResult != VK_SUCCESS) {
                LOG_FATAL("Failed to load Vulkan, failed with {}!", (uint32_t) eResult);
                return false;
            }

            if (!glfwVulkanSupported()) {
                LOG_FATAL("GLFW: Vulkan Not Supported");
                return false;
            }

            // final extension buffer
            ImVector<const char*> instance_extensions;

            uint32_t glfw_extensions_count = 0;
            const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extensions_count);
            for (uint32_t i = 0; i < glfw_extensions_count; i++)
                instance_extensions.push_back(glfw_extensions[i]);

            std::vector<std::string> openvr_instance_extensions;
            getOpenvrVulkanInstanceExtensionsRequired(openvr_instance_extensions);
            for (int i = 0; i < (int)openvr_instance_extensions.size(); i++) {
                // openvr extensions may contain entries glfw requires too! only add unique ones
                bool is_duplicate = false;
                for (const char* existing_ext : instance_extensions) {
                    if (std::strcmp(existing_ext, openvr_instance_extensions[i].c_str()) == 0) {
                        is_duplicate = true;
                        break;
                    }
                }

                if (!is_duplicate) {
                    instance_extensions.push_back(openvr_instance_extensions[i].c_str());
                }
            }
            
            return setupVulkan(instance_extensions);
        }

        bool Renderer_Vulkan::initialiseWindow(GLFWwindow* window, int width, int height) {
            ASSERT(window != nullptr, "window must not be null!");

            m_window = window;
            m_width = width;
            m_height = height;

            VkSurfaceKHR surface;
            VkResult err = glfwCreateWindowSurface(m_instance, window, m_allocator, &surface);
            (void) check_vk_result(err);

            m_mainWindowData.Surface = surface;

            // Check for WSI support
            VkBool32 res;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_queueFamily, m_mainWindowData.Surface, &res);
            if (res != VK_TRUE) {
                LOG_ERROR("Error no WSI support on physical device 0");
                return false;
            }

            // Select Surface Format
            const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
            const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            m_mainWindowData.SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(m_physicalDevice, m_mainWindowData.Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

            // Select Present Mode
#ifdef APP_USE_UNLIMITED_FRAME_RATE
            VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
#else
            VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
#endif
            m_mainWindowData.PresentMode = ImGui_ImplVulkanH_SelectPresentMode(m_physicalDevice, m_mainWindowData.Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));
            // LOG_INFO("[vulkan] Selected PresentMode = {}", m_mainWindowData.PresentMode);

            // Create SwapChain, RenderPass, Framebuffer, etc.
            IM_ASSERT(m_minImageCount >= 2);
            ImGui_ImplVulkanH_CreateOrResizeWindow(m_instance, m_physicalDevice, m_device, &m_mainWindowData, m_queueFamily, m_allocator, width, height, m_minImageCount, 0);

            ImGui_ImplVulkan_InitInfo init_info = {};
            //init_info.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
            init_info.Instance = m_instance;
            init_info.PhysicalDevice = m_physicalDevice;
            init_info.Device = m_device;
            init_info.QueueFamily = m_queueFamily;
            init_info.Queue = m_queue;
            init_info.PipelineCache = m_pipelineCache;
            init_info.DescriptorPool = m_descriptorPool;
            init_info.MinImageCount = m_minImageCount;
            init_info.ImageCount = m_mainWindowData.ImageCount;
            init_info.Allocator = m_allocator;
            init_info.PipelineInfoMain.RenderPass = m_mainWindowData.RenderPass;
            init_info.PipelineInfoMain.Subpass = 0;
            init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            init_info.CheckVkResultFn = check_vk_result_callback;
            return ImGui_ImplVulkan_Init(&init_info);
        }

        void Renderer_Vulkan::shutdown() {
            VkResult err = vkDeviceWaitIdle(m_device);
            (void) check_vk_result(err);
            ImGui_ImplVulkan_Shutdown();

            ImGui_ImplVulkanH_DestroyWindow(m_instance, m_device, &m_mainWindowData, m_allocator);

            vkDestroyDescriptorPool(m_device, m_descriptorPool, m_allocator);

#if defined(_DEBUG) || defined(RENDER_USE_VULKAN_DEBUG_REPORT)
            // Remove the debug report callback
            auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT");
            f_vkDestroyDebugReportCallbackEXT(m_instance, m_debugReport, m_allocator);
#endif // APP_USE_VULKAN_DEBUG_REPORT

            vkDestroyDevice(m_device, m_allocator);
            vkDestroyInstance(m_instance, m_allocator);
        }

        void Renderer_Vulkan::newFrame() {
            ImGui_ImplVulkan_NewFrame();
        }

        void Renderer_Vulkan::renderDrawData(ImDrawData* drawData, const ImVec4& clearColor) {
            ASSERT(drawData != nullptr, "drawData must not be null!");
            m_mainWindowData.ClearValue.color.float32[0] = clearColor.x * clearColor.w;
            m_mainWindowData.ClearValue.color.float32[1] = clearColor.y * clearColor.w;
            m_mainWindowData.ClearValue.color.float32[2] = clearColor.z * clearColor.w;
            m_mainWindowData.ClearValue.color.float32[3] = clearColor.w;

            VkSemaphore image_acquired_semaphore = m_mainWindowData.FrameSemaphores[m_mainWindowData.SemaphoreIndex].ImageAcquiredSemaphore;
            VkSemaphore render_complete_semaphore = m_mainWindowData.FrameSemaphores[m_mainWindowData.SemaphoreIndex].RenderCompleteSemaphore;
            VkResult err = vkAcquireNextImageKHR(m_device, m_mainWindowData.Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &m_mainWindowData.FrameIndex);
            if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
                m_swapChainRebuild = true;
            if (err == VK_ERROR_OUT_OF_DATE_KHR)
                return;
            if (err != VK_SUBOPTIMAL_KHR) {
                if (!check_vk_result(err)) {
                    return;
                }
            }

            ImGui_ImplVulkanH_Frame* fd = &m_mainWindowData.Frames[m_mainWindowData.FrameIndex];
            {
                err = vkWaitForFences(m_device, 1, &fd->Fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
                if (!check_vk_result(err)) {
                    return;
                }

                err = vkResetFences(m_device, 1, &fd->Fence);
                if (!check_vk_result(err)) {
                    return;
                }
            }
            {
                err = vkResetCommandPool(m_device, fd->CommandPool, 0);
                (void)check_vk_result(err);
                VkCommandBufferBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
                if (!check_vk_result(err)) {
                    return;
                }
            }
            {
                VkRenderPassBeginInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                info.renderPass = m_mainWindowData.RenderPass;
                info.framebuffer = fd->Framebuffer;
                info.renderArea.extent.width = m_mainWindowData.Width;
                info.renderArea.extent.height = m_mainWindowData.Height;
                info.clearValueCount = 1;
                info.pClearValues = &m_mainWindowData.ClearValue;
                vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
            }

            // Record dear imgui primitives into command buffer
            ImGui_ImplVulkan_RenderDrawData(drawData, fd->CommandBuffer);

            // Submit command buffer
            vkCmdEndRenderPass(fd->CommandBuffer);
            {
                VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                VkSubmitInfo info = {};
                info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                info.waitSemaphoreCount = 1;
                info.pWaitSemaphores = &image_acquired_semaphore;
                info.pWaitDstStageMask = &wait_stage;
                info.commandBufferCount = 1;
                info.pCommandBuffers = &fd->CommandBuffer;
                info.signalSemaphoreCount = 1;
                info.pSignalSemaphores = &render_complete_semaphore;

                err = vkEndCommandBuffer(fd->CommandBuffer);
                if (!check_vk_result(err)) {
                    return;
                }
                err = vkQueueSubmit(m_queue, 1, &info, fd->Fence);
                if (!check_vk_result(err)) {
                    return;
                }
            }
        }

        void Renderer_Vulkan::present(int width, int height) {
            if (m_swapChainRebuild) {
                ImGui_ImplVulkan_SetMinImageCount(m_minImageCount);
                ImGui_ImplVulkanH_CreateOrResizeWindow(m_instance, m_physicalDevice, m_device, &m_mainWindowData, m_queueFamily, m_allocator, width, height, m_minImageCount, 0);
                m_mainWindowData.FrameIndex = 0;
                m_swapChainRebuild = false;
                return;
            }
            VkSemaphore render_complete_semaphore = m_mainWindowData.FrameSemaphores[m_mainWindowData.SemaphoreIndex].RenderCompleteSemaphore;
            VkPresentInfoKHR info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &render_complete_semaphore,
                .swapchainCount = 1,
                .pSwapchains = &m_mainWindowData.Swapchain,
                .pImageIndices = &m_mainWindowData.FrameIndex,
            };
            VkResult err = vkQueuePresentKHR(m_queue, &info);
            if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
                m_swapChainRebuild = true;
            if (err == VK_ERROR_OUT_OF_DATE_KHR)
                return;
            if (err != VK_SUBOPTIMAL_KHR)
                (void) check_vk_result(err);
            m_mainWindowData.SemaphoreIndex = (m_mainWindowData.SemaphoreIndex + 1) % m_mainWindowData.SemaphoreCount; // Now we can use the next set of semaphores
        }

        vr::Texture_t Renderer_Vulkan::getVRTexture() {
            ImGui_ImplVulkanH_Frame* fd = &m_mainWindowData.Frames[m_mainWindowData.FrameIndex];

            m_vrTextureData = {
                .m_nImage               = (uint64_t)fd->Backbuffer,

                .m_pDevice              = m_device,
                .m_pPhysicalDevice      = m_physicalDevice,
                .m_pInstance            = m_instance,
                .m_pQueue               = m_queue,
                .m_nQueueFamilyIndex    = m_queueFamily,

                .m_nWidth               = (uint32_t)m_mainWindowData.Width,
                .m_nHeight              = (uint32_t)m_mainWindowData.Height,
                .m_nFormat              = (uint32_t)m_mainWindowData.SurfaceFormat.format,
                .m_nSampleCount         = VK_SAMPLE_COUNT_1_BIT,
            };

            vr::Texture_t vrTexture = {
                .handle = &m_vrTextureData,
                .eType = vr::ETextureType::TextureType_Vulkan,
                .eColorSpace = vr::EColorSpace::ColorSpace_Auto,
            };
            return vrTexture;
        }

        bool Renderer_Vulkan::setupVulkan(ImVector<const char*> instance_extensions) {
            VkResult err;

            // Create Vulkan Instance
            {
                VkInstanceCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                };

                // Enumerate available extensions
                uint32_t properties_count;
                ImVector<VkExtensionProperties> properties;
                vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
                properties.resize(properties_count);
                err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
                if (!check_vk_result(err)) {
                    return false;
                }

                // Enable required extensions
                if (isExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
                    instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
                if (isExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
                {
                    instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                    create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                }
#endif

                // Enabling validation layers
#if defined(_DEBUG) || defined(APP_USE_VULKAN_DEBUG_REPORT)
                const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = layers;
                instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

                // Create Vulkan Instance
                create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
                create_info.ppEnabledExtensionNames = instance_extensions.Data;
                err = vkCreateInstance(&create_info, m_allocator, &m_instance);
                if (!check_vk_result(err)) {
                    return false;
                }
                volkLoadInstance(m_instance);

                // Setup the debug report callback
#if defined(_DEBUG) || defined(APP_USE_VULKAN_DEBUG_REPORT)
                auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");
                IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
                VkDebugReportCallbackCreateInfoEXT debug_report_ci = {
                    .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
                    .flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT,
                    .pfnCallback = debug_report,
                    .pUserData = nullptr,
                };
                err = f_vkCreateDebugReportCallbackEXT(m_instance, &debug_report_ci, m_allocator, &m_debugReport);
                if (!check_vk_result(err)) {
                    return false;
                }
#endif
            }

            // Select Physical Device (GPU)
            // prefer what openvr wants!
            vr::VRSystem()->GetOutputDevice((uint64_t*) &m_physicalDevice, vr::ETextureType::TextureType_Vulkan, m_instance);
            if (m_physicalDevice == VK_NULL_HANDLE) {
                // fallback to default selection
                m_physicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(m_instance);
            }

            // Select graphics queue family
            m_queueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(m_physicalDevice);
            IM_ASSERT(m_queueFamily != (uint32_t)-1);

            // Create Logical Device (with 1 queue)
            {
                ImVector<const char*> device_extensions;
                device_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

                // Enumerate physical device extension
                uint32_t properties_count;
                ImVector<VkExtensionProperties> properties;
                vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &properties_count, nullptr);
                properties.resize(properties_count);
                vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
                if (isExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
                    device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

                // get openvr extensions
                std::vector<std::string> openvr_device_extensions;
                getOpenvrVulkanDeviceExtensionsRequired(m_physicalDevice, openvr_device_extensions);
                for (int i = 0; i < (int)openvr_device_extensions.size(); i++) {
                    // if (isExtensionAvailable(properties, openvr_device_extensions[i].c_str()))
                    {
                        device_extensions.push_back(openvr_device_extensions[i].c_str());
                    }
                }

                const float queue_priority[] = { 1.0f };
                VkDeviceQueueCreateInfo queue_info[1] = {
                    {
                        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                        .queueFamilyIndex = m_queueFamily,
                        .queueCount = 1,
                        .pQueuePriorities = queue_priority,
                    },
                };
                VkDeviceCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                    .queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]),
                    .pQueueCreateInfos = queue_info,
                    .enabledExtensionCount = (uint32_t)device_extensions.Size,
                    .ppEnabledExtensionNames = device_extensions.Data,
                };
                err = vkCreateDevice(m_physicalDevice, &create_info, m_allocator, &m_device);
                if (!check_vk_result(err)) {
                    return false;
                }
                vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);
            }

            // Create Descriptor Pool
            // If you wish to load e.g. additional textures you may need to alter pools sizes and maxSets.
            {
                VkDescriptorPoolSize pool_sizes[] =
                {
                    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
                };
                VkDescriptorPoolCreateInfo pool_info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                    .maxSets = 0,
                    .poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes),
                    .pPoolSizes = pool_sizes,
                };
                for (VkDescriptorPoolSize& pool_size : pool_sizes)
                    pool_info.maxSets += pool_size.descriptorCount;
                err = vkCreateDescriptorPool(m_device, &pool_info, m_allocator, &m_descriptorPool);
                if (!check_vk_result(err)) {
                    return false;
                }
            }

            return true;
        }
    }
}