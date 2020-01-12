#include "debug_utils.hpp"

// shaders
#include <cstdint> // have to include this here to pass 'uint32_t' to shaders
#include "shader_bin/triangle_vert.hpp"
#include "shader_bin/fill_triangle_frag.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>

#include <vector>
#include <iostream>
#include <unordered_set>
#include <algorithm>

namespace
{
#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

    const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    enum class ERRORS : uint32_t
    {
        // ReSharper disable once CppInconsistentNaming
        // ReSharper disable once CppEnumeratorNeverUsed
        _START = 0xdead0000,
        REQUESTED_VALIDATION_LAYERS_ARE_NOT_AVAILABLE,
        FAILED_TO_CREATE_LOGICAL_DEVICE,
        FAILED_TO_CREATE_WINDOW_SURFACE,
        FAILED_TO_CREATE_SWAP_CHAIN,
        FAILED_TO_CREATE_IMAGE_VIEWS,
        FAILED_TO_CREATE_SHADER_MODULE,
        FAILED_TO_CREATE_PIPELINE_LAYOUT,
        FAILED_TO_CREATE_RENDER_PASS,
        FAILED_TO_CREATE_GRAPHICS_PIPELINE,
        FAILED_TO_CREATE_FRAMEBUFFERS,
        // ReSharper disable once CppInconsistentNaming
        // ReSharper disable once CppEnumeratorNeverUsed
        _END
    };

    void quit_application(ERRORS error) {
        exit(static_cast<uint32_t>(error));
    }

    std::vector<const char*> get_required_extensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (ENABLE_VALIDATION_LAYERS) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info,
                                          const VkAllocationCallbacks* allocator,
                                          VkDebugUtilsMessengerEXT* callback) {
        const auto func = PFN_vkCreateDebugUtilsMessengerEXT(vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT"));
        if (func != nullptr) {
            return func(instance, create_info, allocator, callback);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                       const VkAllocationCallbacks* pAllocator) {
        const auto func = PFN_vkDestroyDebugUtilsMessengerEXT(vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func != nullptr) func(instance, callback, pAllocator);
    }
}

class HelloTriangleApplication
{
public:

    void run() {
        init_window();
        if (!init_vulkan()) return;
        main_loop();
        cleanup();
    }

private:
    void init_window() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    }

    bool check_validation_layer_support() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const auto layerName : VALIDATION_LAYERS) {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName)) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) return false;
        }

        return true;
    }

    bool create_instance() {
        if constexpr (ENABLE_VALIDATION_LAYERS){
            if (!check_validation_layer_support()) {
                quit_application(ERRORS::REQUESTED_VALIDATION_LAYERS_ARE_NOT_AVAILABLE);
            }
        }
        VkApplicationInfo application_info = {};
        application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application_info.pApplicationName = "Hello Triangle";
        application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.pEngineName = "No Engine";
        application_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        application_info.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &application_info;

        auto instance_extensions = get_required_extensions();

        create_info.enabledExtensionCount = instance_extensions.size();
        create_info.ppEnabledExtensionNames = instance_extensions.data();

        if constexpr (ENABLE_VALIDATION_LAYERS) {
            create_info.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }
        else {
            create_info.enabledLayerCount = 0;
        }

        const VkResult result = vkCreateInstance(&create_info, nullptr, &instance_);
        if (result != VK_SUCCESS) {
            return false;
        }

        uint32_t extension_count;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        for (auto& extension : extensions) {
            std::cout << "\t" << extension.extensionName << "\n";
        }

        return true;
    }

    bool setup_debug_callback() {
        if constexpr (!ENABLE_VALIDATION_LAYERS) return true;

        VkDebugUtilsMessengerCreateInfoEXT create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        create_info.pfnUserCallback = DebugUtils::debugCallback;
        create_info.pUserData = nullptr;

        if (CreateDebugUtilsMessengerEXT(instance_, &create_info, nullptr, &callback_) != VK_SUCCESS) return false;

        return true;
    }

    bool check_device_extension_support(const VkPhysicalDevice& device) const {
        uint32_t extension_count = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

        std::vector<VkExtensionProperties> available_extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

        std::unordered_set<std::string> required_extensions(begin(DEVICE_EXTENSIONS), end(DEVICE_EXTENSIONS));

        for(const auto& extension : available_extensions) {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    bool is_device_suitable(const VkPhysicalDevice& device) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

        auto indices = find_queue_families(device);

        const auto device_extension_supported = check_device_extension_support(device);
        bool swap_chain_is_adequate = false;
        if(device_extension_supported) {
            const auto swap_chain_support = query_swap_chain_support(device);
            swap_chain_is_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.presentModes.empty();
        }

        return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == properties.deviceType &&
            features.geometryShader && indices.is_complete() && device_extension_supported && swap_chain_is_adequate;
    }

    VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& available_formats) {
        if(available_formats.size() == 1 && available_formats.at(0).format == VK_FORMAT_UNDEFINED) {
            return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }
        for (const auto& format : available_formats) {
            if(format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        // TODO: research what other formats are suitable for my needs. Meanwhile, take any available
        return available_formats.at(0);
    }

    VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR> available_present_modes) {
        auto best_mode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& present_mode : available_present_modes) {
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) return present_mode;
			if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) best_mode = present_mode;
		}
        return best_mode;
    }

    VkExtent2D choose_swap_extent(const VkSurfaceCapabilitiesKHR& capabilities) const {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() && capabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        VkExtent2D extent = {WIDTH, HEIGHT};

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }

    bool create_swap_chain() {
        const SwapChainSupportDetails swap_chain_support_details = query_swap_chain_support(physical_device_);
        const VkSurfaceFormatKHR format_khr = choose_surface_format(swap_chain_support_details.formats);
        const VkPresentModeKHR present_mode_khr = choose_present_mode(swap_chain_support_details.presentModes);
        const VkExtent2D extent_2d = choose_swap_extent(swap_chain_support_details.capabilities);
        uint32_t image_count = swap_chain_support_details.capabilities.minImageCount + 1;
        if(swap_chain_support_details.capabilities.maxImageCount > 0 && image_count > swap_chain_support_details.capabilities.maxImageCount) {
            image_count = swap_chain_support_details.capabilities.maxImageCount;
        }
        VkSwapchainCreateInfoKHR create_info_khr = {};
        create_info_khr.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info_khr.surface = surface_;
        create_info_khr.minImageCount = image_count;
        create_info_khr.imageFormat = format_khr.format;
        create_info_khr.imageColorSpace = format_khr.colorSpace;
        create_info_khr.imageExtent = extent_2d;
        // Always 1 unless you're working on stereoscopic 3d app
        create_info_khr.imageArrayLayers = 1;
        // Specifies what kind of operations we'll use the images in the swap chain for.
        // To render directly to them, we'll use VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        // If you want to render to separate image for post-processing, you'll need VK_IMAGE_USAGE_TRANSFER_DST_BIT
        create_info_khr.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        create_info_khr.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info_khr.queueFamilyIndexCount = 0;
        create_info_khr.pQueueFamilyIndices = nullptr;
        // If you don't want any extra transformations on images, simply use current transform
        create_info_khr.preTransform = swap_chain_support_details.capabilities.currentTransform;
        create_info_khr.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        create_info_khr.presentMode = present_mode_khr;
        create_info_khr.clipped = VK_TRUE;
        // Example of using oldSwapchain - if current swap chain becomes invalid or unoptimized, e.g. resize of main window,
        // you'll need to create new swap chain and pass old one to oldSwapchain
        create_info_khr.oldSwapchain = nullptr;
        
        if(vkCreateSwapchainKHR(device_, &create_info_khr, nullptr, &swapchain_) == VK_SUCCESS) {
            vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr);
            swap_chain_images_.resize(image_count);
            vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, swap_chain_images_.data());
            format_ = format_khr.format;
            extent_2d_ = extent_2d;
            return true;
        }

        quit_application(ERRORS::FAILED_TO_CREATE_SWAP_CHAIN);
        return false;
    }

    bool pick_physical_device() {
        uint32_t device_count = 0;
        vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

        if (device_count == 0) return false;

        std::vector<VkPhysicalDevice> physicalDevices(device_count);
        vkEnumeratePhysicalDevices(instance_, &device_count, physicalDevices.data());

        for (const auto& device : physicalDevices) {
            // TODO: check for multiple GPU's and select appropriate one (page 64)
            if (is_device_suitable(device)) {
                physical_device_ = device;
                return true;
            }
        }

        return false;
    }

    bool create_logical_device() {
        const QueueFamilyIndices indices = find_queue_families(physical_device_);

        // [AP] NB: I'm drifting from tutorial on purpose as I'm using the same queue from graphics and presentation and
        // I don't need to create multiple queues
        VkDeviceQueueCreateInfo queue_create_info = {};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = indices.graphics_and_present_family;
        queue_create_info.queueCount = 1;

        constexpr float queue_priority = 1.0f;
        queue_create_info.pQueuePriorities = &queue_priority;

        VkPhysicalDeviceFeatures device_features = {};

        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pQueueCreateInfos = &queue_create_info;
        create_info.queueCreateInfoCount = 1;
        create_info.pEnabledFeatures = &device_features;

        create_info.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
        create_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

        if constexpr  (ENABLE_VALIDATION_LAYERS) {
            create_info.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            create_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        } else {
            create_info.enabledLayerCount = 0;
        }

        if(vkCreateDevice(physical_device_, &create_info, nullptr, &device_) == VK_SUCCESS) {
            // The parameters are the logical device, queue family, queue index and a pointer to the variable to store the queue handle in.
            // Because we’re only creating a single queue from this family, we’ll simply use index 0.
            vkGetDeviceQueue(device_, indices.graphics_and_present_family, 0, &graphics_queue_);
            return true;
        };

        quit_application(ERRORS::FAILED_TO_CREATE_LOGICAL_DEVICE);
        return false;
    }

    bool create_surface() {
        if(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) == VK_SUCCESS) {
            return true;
        }

        quit_application(ERRORS::FAILED_TO_CREATE_WINDOW_SURFACE);
        return false;
    }

    bool create_image_views() {
        swap_chain_image_views_.resize(swap_chain_images_.size());
        for (int i = 0; i < swap_chain_images_.size(); i++) {
            VkImageViewCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = swap_chain_images_.at(i);

            // viewType - treat images as 1D, 2D, 3D textures or cube maps
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = format_;
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;
            if(vkCreateImageView(device_, &create_info, nullptr, &swap_chain_image_views_[i]) != VK_SUCCESS) {
                quit_application(ERRORS::FAILED_TO_CREATE_IMAGE_VIEWS);
                return false;
            }
        } 
        return true;
    }

    VkShaderModule create_shader_module(const uint32_t * shader_code, uint32_t code_size) const {
        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code_size;
        create_info.pCode = shader_code;
        VkShaderModule shader_module;
        if(vkCreateShaderModule(device_, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            quit_application(ERRORS::FAILED_TO_CREATE_SHADER_MODULE);
        }
        return shader_module;
    }

    bool create_graphics_pipeline() {
        auto vertex_module = create_shader_module(triangle_vert, sizeof(triangle_vert));
        auto frag_module = create_shader_module(fill_triangle_frag, sizeof(fill_triangle_frag));

        VkPipelineShaderStageCreateInfo vertex_stage_create_info = {};
        vertex_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertex_stage_create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertex_stage_create_info.module = vertex_module;
        vertex_stage_create_info.pName = "main";
        //  pSpecializationInfo is used to pass constants to shaders
        // vertex_stage_create_info.pSpecializationInfo = nullptr;

        VkPipelineShaderStageCreateInfo frag_stage_create_info = {};
        frag_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        frag_stage_create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        frag_stage_create_info.module = frag_module;
        frag_stage_create_info.pName = "main";
        //  pSpecializationInfo is used to pass constants to shaders
        // vertex_stage_create_info.pSpecializationInfo = nullptr;
        
        VkPipelineShaderStageCreateInfo shader_stages[] = {vertex_stage_create_info, frag_stage_create_info};

        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
        vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_create_info.vertexBindingDescriptionCount = 0;
        vertex_input_state_create_info.pVertexBindingDescriptions = nullptr;
        vertex_input_state_create_info.vertexAttributeDescriptionCount = 0;
        vertex_input_state_create_info.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(extent_2d_.width);
        viewport.height = static_cast<float>(extent_2d_.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = extent_2d_;

        VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
        viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_state_create_info.viewportCount = 1;
        viewport_state_create_info.pViewports = &viewport;
        viewport_state_create_info.scissorCount = 1;
        viewport_state_create_info.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
        rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization_state_create_info.depthClampEnable = VK_FALSE;
        rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
        rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
        rasterization_state_create_info.lineWidth = 1.f;
        rasterization_state_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
        
        rasterization_state_create_info.depthBiasEnable = VK_FALSE; // Optional
        rasterization_state_create_info.depthBiasConstantFactor = 0.f; // Optional
        rasterization_state_create_info.depthBiasClamp = 0.f; // Optional
        rasterization_state_create_info.depthBiasSlopeFactor = 0.f; // Optional

        VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
        multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample_state_create_info.sampleShadingEnable = VK_FALSE;
        multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisample_state_create_info.minSampleShading = 1.f;
        multisample_state_create_info.pSampleMask = nullptr;
        multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
        multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
        color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        color_blend_attachment_state.blendEnable = VK_FALSE;
        color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
        color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        color_blend_state_create_info.logicOpEnable = VK_FALSE;
        color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY; // Optional
        color_blend_state_create_info.attachmentCount = 1;
        color_blend_state_create_info.pAttachments = &color_blend_attachment_state;
        color_blend_state_create_info.blendConstants[0] = 0.0f; // Optional
        color_blend_state_create_info.blendConstants[1] = 0.0f; // Optional
        color_blend_state_create_info.blendConstants[2] = 0.0f; // Optional
        color_blend_state_create_info.blendConstants[3] = 0.0f; // Optional

        VkDynamicState dynamic_states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };

        VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
        dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic_state_create_info.dynamicStateCount = 0;
        dynamic_state_create_info.pDynamicStates = dynamic_states;

        VkPipelineLayoutCreateInfo layout_create_info = {};
        layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_create_info.setLayoutCount = 0; // Optional
        layout_create_info.pSetLayouts = nullptr; // Optional
        layout_create_info.pushConstantRangeCount = 0; // Optional
        layout_create_info.pPushConstantRanges = nullptr; // Optional

        if(vkCreatePipelineLayout(device_, &layout_create_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_PIPELINE_LAYOUT);
            return false;
        }

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {};
        graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        graphics_pipeline_create_info.stageCount = 2;
        graphics_pipeline_create_info.pStages = shader_stages;
        graphics_pipeline_create_info.pVertexInputState = &vertex_input_state_create_info;
        graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state_create_info;
        graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;
        graphics_pipeline_create_info.pRasterizationState = &rasterization_state_create_info;
        graphics_pipeline_create_info.pMultisampleState = &multisample_state_create_info;
        graphics_pipeline_create_info.pDepthStencilState = nullptr; // Optional
        graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;
        graphics_pipeline_create_info.pDynamicState = nullptr; // Optional

        graphics_pipeline_create_info.layout = pipeline_layout_;
        graphics_pipeline_create_info.renderPass = render_pass_;
        graphics_pipeline_create_info.subpass = 0;

        graphics_pipeline_create_info.basePipelineHandle = nullptr; // Optional
        graphics_pipeline_create_info.basePipelineIndex = -1; // Optional

        if(vkCreateGraphicsPipelines(device_, nullptr, 1, &graphics_pipeline_create_info, nullptr, &pipeline_) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_GRAPHICS_PIPELINE);
            return false;
        }

        vkDestroyShaderModule(device_, vertex_module, nullptr);
        vkDestroyShaderModule(device_, frag_module, nullptr);

        return true;
    }

    bool create_render_pass() {
        VkAttachmentDescription attachment_description = {};
        attachment_description.format = format_;
        attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference attachment_reference = {};
        attachment_reference.attachment = 0;
        attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass_description = {};
        subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_description.colorAttachmentCount = 1;
        subpass_description.pColorAttachments = &attachment_reference;
        // The index of the attachment in this array is directly referenced from the fragment
        // shader with the layout(location = 0)out vec4 outColor directive!
        
        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &attachment_description;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass_description;

        if(vkCreateRenderPass(device_, &render_pass_create_info, nullptr, &render_pass_) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_RENDER_PASS);
            return false;
        }

        return true;
    }

    bool create_framebuffers() {
        swap_chain_framebuffers_.resize(swap_chain_image_views_.size());
        for (int i = 0; i <  swap_chain_image_views_.size(); i++){
            VkImageView attachments[] = {swap_chain_image_views_[i]};
            VkFramebufferCreateInfo framebuffer_create_info = {};
            framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            // Specify which render pass this framebuffer needs to be compatible with
            // Compatible = uses same number and type of attachments
            framebuffer_create_info.renderPass = render_pass_;
            // attachmentCount & pAttachments specify same objects that should be bound to respective attachment descriptions
            // in the render pass pAttachment array
            framebuffer_create_info.attachmentCount = 1;
            framebuffer_create_info.pAttachments = attachments;
            framebuffer_create_info.width = extent_2d_.width;
            framebuffer_create_info.height = extent_2d_.height;
            framebuffer_create_info.layers = 1;

            if(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &swap_chain_framebuffers_[i]) != VK_SUCCESS) {
                quit_application(ERRORS::FAILED_TO_CREATE_FRAMEBUFFERS);
                return false;
            }
        }
        return true;
    }

    bool init_vulkan() {
        return
            create_instance() &&
            setup_debug_callback() &&
            create_surface() &&
            pick_physical_device() &&
            create_logical_device() &&
            create_swap_chain() &&
            create_image_views() &&
            create_render_pass() &&
            create_graphics_pipeline() &&
            create_framebuffers();
    }

    void main_loop() const {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if constexpr (ENABLE_VALIDATION_LAYERS) DestroyDebugUtilsMessengerEXT(instance_, callback_, nullptr);

        for (auto swap_chain_framebuffer : swap_chain_framebuffers_) {
            vkDestroyFramebuffer(device_, swap_chain_framebuffer, nullptr);
        }

        vkDestroyPipeline(device_, pipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        vkDestroyRenderPass(device_, render_pass_, nullptr);

        for (auto swap_chain_image_view : swap_chain_image_views_) {
            vkDestroyImageView(device_, swap_chain_image_view, nullptr);
        } 

        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        vkDestroyDevice(device_, nullptr);

        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);

        glfwDestroyWindow(window_);

        glfwTerminate();
    }

    struct QueueFamilyIndices
    {
        int graphics_and_present_family = -1;

        bool is_complete() const {
            return graphics_and_present_family >= 0;
        }
    };

    QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

        int i = 0;
        for (const auto& queue_family : queue_families) {
            // Note that it’s very likely that these end up being the same queue family after all, but throughout the program we will treat them as if they were separate queues for a uniform approach.
            // Nevertheless, you could add logic to explicitly prefer a physical device that supports drawing and presentation in the same queue for improved performance
            // [AP] I've decided to go with later approach, only using device that is supporting both
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queue_family.queueCount > 0 && queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport) {
                indices.graphics_and_present_family = i;
            }

            if (indices.is_complete()) break;

            ++i;
        }

        return indices;
    }

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device) const {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, nullptr);
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count, details.formats.data());

        uint32_t present_mode_count = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, nullptr);
        details.presentModes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_mode_count, details.presentModes.data());

        return details;
    }    

    VkInstance instance_;
    VkDebugUtilsMessengerEXT callback_;
    VkSurfaceKHR surface_;
    
    VkPhysicalDevice physical_device_ = nullptr;
    VkSwapchainKHR swapchain_;

    GLFWwindow* window_ = nullptr;
    VkDevice device_;
    VkQueue graphics_queue_;

    std::vector<VkImage> swap_chain_images_;
    VkFormat format_;
    VkExtent2D extent_2d_;

    VkRenderPass render_pass_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;

    std::vector<VkImageView> swap_chain_image_views_;
    std::vector<VkFramebuffer> swap_chain_framebuffers_;

    constexpr static int WIDTH = 800;
    constexpr static int HEIGHT = 600;
};

int main() {
    HelloTriangleApplication application;
    application.run();
}
