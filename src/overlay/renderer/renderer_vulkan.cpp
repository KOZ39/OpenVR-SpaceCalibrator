#include "renderer_vulkan.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <volk.h>
#include "log.h"
#include "util.h"
#include <stb_image.h>

namespace spacecal {
    namespace renderer {
        constexpr uint32_t k_VULKAN_API_VERSION = VK_API_VERSION_1_3;

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
                if (strncmp(p.extensionName, extension, VK_MAX_EXTENSION_NAME_SIZE) == 0)
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
                    if (std::strncmp(existing_ext, openvr_instance_extensions[i].c_str(), VK_MAX_EXTENSION_NAME_SIZE) == 0) {
                        is_duplicate = true;
                        break;
                    }
                }

                if (!is_duplicate) {
                    instance_extensions.push_back(openvr_instance_extensions[i].c_str());
                }
            }

            m_loadedTextureData.reserve(64);
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
            init_info.ApiVersion = k_VULKAN_API_VERSION;
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
                VkApplicationInfo app_info = {
                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .pApplicationName = "Space Calibrator",
                    .applicationVersion = 1,
                    .pEngineName = "Space Calibrator Nova CORE",
                    .engineVersion = 2,
                    .apiVersion = k_VULKAN_API_VERSION,
                };

                VkInstanceCreateInfo create_info = {
                    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                    .pApplicationInfo = &app_info,
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
#if defined(_DEBUG) || defined(RENDER_USE_VULKAN_DEBUG_REPORT)
                const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = layers;
                instance_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif // RENDER_USE_VULKAN_DEBUG_REPORT

                // Create Vulkan Instance
                create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
                create_info.ppEnabledExtensionNames = instance_extensions.Data;
                err = vkCreateInstance(&create_info, m_allocator, &m_instance);
                if (!check_vk_result(err)) {
                    return false;
                }
                volkLoadInstance(m_instance);

                // Setup the debug report callback
#if defined(_DEBUG) || defined(RENDER_USE_VULKAN_DEBUG_REPORT)
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
#endif // RENDER_USE_VULKAN_DEBUG_REPORT
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
                    { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
                    { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
                };
                VkDescriptorPoolCreateInfo pool_info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                    .maxSets = 0,
                    .poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes),
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

        uint32_t Renderer_Vulkan::findMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties mem_properties;
            vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &mem_properties);

            for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
                if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
                    return i;

            return 0xFFFFFFFF; // Unable to find memoryType
        }

        TextureData_t Renderer_Vulkan::loadTexture(const std::string& szFilePath) {
            int width, height, nrChannels;
            uintptr_t dwInternalDataIdx = (uintptr_t) -1;
            VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

            constexpr int k_TEXTURE_CHANNELS = STBI_rgb_alpha;
            unsigned char* textureData = stbi_load(szFilePath.c_str(), &width, &height, &nrChannels, k_TEXTURE_CHANNELS);
            TextureData_t data = {};

            if (textureData) {
                size_t image_size = width * height * k_TEXTURE_CHANNELS;
                VkResult err;

                VkTextureData_t tex_data = {};

                // Create the Vulkan image.
                {
                    VkImageCreateInfo info = {};
                    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                    info.imageType = VK_IMAGE_TYPE_2D;
                    info.format = VK_FORMAT_R8G8B8A8_UNORM;
                    info.extent.width = width;
                    info.extent.height = height;
                    info.extent.depth = 1;
                    info.mipLevels = 1;
                    info.arrayLayers = 1;
                    info.samples = VK_SAMPLE_COUNT_1_BIT;
                    info.tiling = VK_IMAGE_TILING_OPTIMAL;
                    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
                    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    err = vkCreateImage(m_device, &info, m_allocator, &tex_data.Image);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    VkMemoryRequirements req;
                    vkGetImageMemoryRequirements(m_device, tex_data.Image, &req);
                    VkMemoryAllocateInfo alloc_info = {};
                    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    alloc_info.allocationSize = req.size;
                    alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
                    err = vkAllocateMemory(m_device, &alloc_info, m_allocator, &tex_data.ImageMemory);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    err = vkBindImageMemory(m_device, tex_data.Image, tex_data.ImageMemory, 0);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                }

                // Create the Image View
                {
                    VkImageViewCreateInfo info = {};
                    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                    info.image = tex_data.Image;
                    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
                    info.format = VK_FORMAT_R8G8B8A8_UNORM;
                    info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    info.subresourceRange.levelCount = 1;
                    info.subresourceRange.layerCount = 1;
                    err = vkCreateImageView(m_device, &info, m_allocator, &tex_data.ImageView);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                }

                // Create Image View Descriptor Set 
                // (note: before 1.92.8 this also took a Sampler. See Wiki history)
                descriptorSet = ImGui_ImplVulkan_AddTexture(tex_data.ImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                // Create Upload Buffer
                {
                    VkBufferCreateInfo buffer_info = {};
                    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    buffer_info.size = image_size;
                    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
                    err = vkCreateBuffer(m_device, &buffer_info, m_allocator, &tex_data.UploadBuffer);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    VkMemoryRequirements req;
                    vkGetBufferMemoryRequirements(m_device, tex_data.UploadBuffer, &req);
                    VkMemoryAllocateInfo alloc_info = {};
                    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
                    alloc_info.allocationSize = req.size;
                    alloc_info.memoryTypeIndex = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
                    err = vkAllocateMemory(m_device, &alloc_info, m_allocator, &tex_data.UploadBufferMemory);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    err = vkBindBufferMemory(m_device, tex_data.UploadBuffer, tex_data.UploadBufferMemory, 0);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                }

                // Upload to Buffer:
                {
                    void* map = NULL;
                    err = vkMapMemory(m_device, tex_data.UploadBufferMemory, 0, image_size, 0, &map);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    memcpy(map, textureData, image_size);
                    VkMappedMemoryRange range[1] = {};
                    range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                    range[0].memory = tex_data.UploadBufferMemory;
                    range[0].size = image_size;
                    err = vkFlushMappedMemoryRanges(m_device, 1, range);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    vkUnmapMemory(m_device, tex_data.UploadBufferMemory);
                }

                // Release image memory using stb
                stbi_image_free(textureData);
                textureData = nullptr;

                // Create a command buffer that will perform following steps when hit in the command queue.
                // TODO: this works in the example, but may need input if this is an acceptable way to access the pool/create the command buffer.
                VkCommandPool command_pool = m_mainWindowData.Frames[m_mainWindowData.FrameIndex].CommandPool;
                VkCommandBuffer command_buffer;
                {
                    VkCommandBufferAllocateInfo alloc_info{};
                    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                    alloc_info.commandPool = command_pool;
                    alloc_info.commandBufferCount = 1;

                    err = vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }

                    VkCommandBufferBeginInfo begin_info = {};
                    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    err = vkBeginCommandBuffer(command_buffer, &begin_info);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                }

                // Copy to Image
                {
                    VkImageMemoryBarrier copy_barrier[1] = {};
                    copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    copy_barrier[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    copy_barrier[0].image = tex_data.Image;
                    copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    copy_barrier[0].subresourceRange.levelCount = 1;
                    copy_barrier[0].subresourceRange.layerCount = 1;
                    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, copy_barrier);

                    VkBufferImageCopy region = {};
                    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    region.imageSubresource.layerCount = 1;
                    region.imageExtent.width = width;
                    region.imageExtent.height = height;
                    region.imageExtent.depth = 1;
                    vkCmdCopyBufferToImage(command_buffer, tex_data.UploadBuffer, tex_data.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

                    VkImageMemoryBarrier use_barrier[1] = {};
                    use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                    use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    use_barrier[0].image = tex_data.Image;
                    use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    use_barrier[0].subresourceRange.levelCount = 1;
                    use_barrier[0].subresourceRange.layerCount = 1;
                    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, use_barrier);
                }

                // End command buffer
                {
                    VkSubmitInfo end_info = {};
                    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                    end_info.commandBufferCount = 1;
                    end_info.pCommandBuffers = &command_buffer;
                    err = vkEndCommandBuffer(command_buffer);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    err = vkQueueSubmit(m_queue, 1, &end_info, VK_NULL_HANDLE);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                    err = vkDeviceWaitIdle(m_device);
                    if (!check_vk_result(err)) {
                        goto vk_failure;
                    }
                }

                dwInternalDataIdx = m_loadedTextureData.size();
                m_loadedTextureData.push_back(tex_data);
            } else {
                LOG_WARN("Failed to load texture from {}...", szFilePath);
            }

            data = {
                .hTexture = descriptorSet == VK_NULL_HANDLE ? k_INVALID_TEXTURE_HANDLE : (TextureHandle_t) descriptorSet,
                .dwWidth = (uint32_t)width,
                .dwHeight = (uint32_t)height,
                .hInternalData = dwInternalDataIdx,
            };
            return data;

        vk_failure:
            if (textureData) {
                stbi_image_free(textureData);
                textureData = nullptr;
            }

            TextureData_t invalidData = {
                .hTexture = k_INVALID_TEXTURE_HANDLE,
                .dwWidth = (uint32_t)-1,
                .dwHeight = (uint32_t)-1,
                .hInternalData = (uintptr_t)-1,
            };

            return invalidData;
        }

        void Renderer_Vulkan::destroyTexture(TextureData_t hTexture) {
            if (hTexture.hInternalData != (uintptr_t) -1) {
                VkTextureData_t& tex_data = m_loadedTextureData[hTexture.hInternalData];

                vkFreeMemory(m_device, tex_data.UploadBufferMemory, nullptr);
                vkDestroyBuffer(m_device, tex_data.UploadBuffer, nullptr);
                vkDestroyImageView(m_device, tex_data.ImageView, nullptr);
                vkDestroyImage(m_device, tex_data.Image, nullptr);
                vkFreeMemory(m_device, tex_data.ImageMemory, nullptr);
            }
            if (hTexture.hTexture != k_INVALID_TEXTURE_HANDLE) {
                VkDescriptorSet descriptorSet = (VkDescriptorSet)hTexture.hTexture;
                ImGui_ImplVulkan_RemoveTexture(descriptorSet);
            }
        }
    }
}