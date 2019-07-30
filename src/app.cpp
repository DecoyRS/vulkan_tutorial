#include "debug_utils.hpp"

// shaders
#include <cstdint> // have to include this here to pass 'uint32_t' to shaders
#include "shader_bin/triangle_vert.hpp"
#include "shader_bin/fill_triangle_frag.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
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

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    enum class ERRORS : uint32_t
    {
        _START = 0xdead0000,
        REQUESTED_VALIDATION_LAYERS_ARE_NOT_AVAILABLE,
        FAILED_TO_CREATE_LOGICAL_DEVICE,
        FAILED_TO_CREATE_WINDOW_SURFACE,
        FAILED_TO_CREATE_SWAP_CHAIN,
        FAILED_TO_CREATE_IMAGE_VIEWS,
        FAILED_TO_CREATE_SHADER_MODULE,
        FAILED_TO_CREATE_PIPELINE_LAYOUT,
        FAILED_TO_CREATE_RENDER_PASS,
        FAILED_TO_CREATE_GRAPHICS_PIPELINE
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

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                          const VkAllocationCallbacks* pAllocator,
                                          VkDebugUtilsMessengerEXT* pCallback) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pCallback);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                       const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) func(instance, callback, pAllocator);
    }
}

class HelloTriangleApplication
{
public:

    void run() {
        initWindow();
        if (!initVulkan()) return;
        mainLoop();
        cleanup();
    }

private:
    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const auto layerName : validationLayers) {
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

    bool createInstance() {
        if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
            quit_application(ERRORS::REQUESTED_VALIDATION_LAYERS_ARE_NOT_AVAILABLE);
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

        if (ENABLE_VALIDATION_LAYERS) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            create_info.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            create_info.enabledLayerCount = 0;
        }

        VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
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

    bool setupDebugCallback() {
        if constexpr (!ENABLE_VALIDATION_LAYERS) return true;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugUtils::debugCallback;
        createInfo.pUserData = nullptr;

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) return false;

        return true;
    }

    bool checkDeviceExtensionSupport(const VkPhysicalDevice& device) const {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::unordered_set<std::string> requiredExtensions(begin(deviceExtensions), end(deviceExtensions));

        for(const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    bool isDeviceSuitable(const VkPhysicalDevice& device) {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);

        auto indices = findQueueFamilies(device);

        auto deviceExtensionSupported = checkDeviceExtensionSupport(device);
        bool swapChainIsAdequate = false;
        if(deviceExtensionSupported) {
            const auto swapChainSupport = querySwapChainSupport(device);
            swapChainIsAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        return VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == properties.deviceType &&
            features.geometryShader && indices.isComplete() && deviceExtensionSupported && swapChainIsAdequate;
    }

    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        if(availableFormats.size() == 1 && availableFormats.at(0).format == VK_FORMAT_UNDEFINED) {
            return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }
        for (const auto& format : availableFormats) {
            if(format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return format;
            }
        }

        // TODO: research what other formats are suitable for my needs. Meanwhile, take any available
        return availableFormats.at(0);
    }

    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR> availablePresentModes) {
        auto best_mode = VK_PRESENT_MODE_FIFO_KHR;
		for (const auto& present_mode : availablePresentModes) {
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) return present_mode;
			if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR) best_mode = present_mode;
		}
        return best_mode;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max() && capabilities.currentExtent.height != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        VkExtent2D extent = {WIDTH, HEIGHT};

        extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return extent;
    }

    bool createSwapChain() {
        SwapChainSupportDetails swap_chain_support_details = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR format_khr = chooseSurfaceFormat(swap_chain_support_details.formats);
        VkPresentModeKHR present_mode_khr = choosePresentMode(swap_chain_support_details.presentModes);
        VkExtent2D extent_2d = chooseSwapExtent(swap_chain_support_details.capabilities);
        uint32_t imageCount = swap_chain_support_details.capabilities.minImageCount + 1;
        if(swap_chain_support_details.capabilities.maxImageCount > 0 && imageCount > swap_chain_support_details.capabilities.maxImageCount) {
            imageCount = swap_chain_support_details.capabilities.maxImageCount;
        }
        VkSwapchainCreateInfoKHR create_info_khr = {};
        create_info_khr.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info_khr.surface = surface;
        create_info_khr.minImageCount = imageCount;
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
        create_info_khr.oldSwapchain = VK_NULL_HANDLE;
        
        if(vkCreateSwapchainKHR(device, &create_info_khr, nullptr, &swapchain) == VK_SUCCESS) {
            vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapChainImages.data());
            format_ = format_khr.format;
            extent_2d_ = extent_2d;
            return true;
        }

        quit_application(ERRORS::FAILED_TO_CREATE_SWAP_CHAIN);
        return false;
    }

    bool pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0) return false;

        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        for (const auto& device : physicalDevices) {
            // TODO: check for multiple GPU's and select appropriate one (page 64)
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                return true;
            }
        }

        return false;
    }

    bool createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // [AP] NB: I'm drifting from tutorial on purpose as I'm using the same queue from graphics and presentation and
        // I don't need to create multiple queues
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsAndPresentFamily;
        queueCreateInfo.queueCount = 1;

        constexpr float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) == VK_SUCCESS) {
            // The parameters are the logical device, queue family, queue index and a pointer to the variable to store the queue handle in.
            // Because we’re only creating a single queue from this family, we’ll simply use index 0.
            vkGetDeviceQueue(device, indices.graphicsAndPresentFamily, 0, &graphicsQueue);
            return true;
        };

        quit_application(ERRORS::FAILED_TO_CREATE_LOGICAL_DEVICE);
        return false;
    }

    bool createSurface() {
        if(glfwCreateWindowSurface(instance, window, nullptr, &surface) == VK_SUCCESS) {
            return true;
        }

        quit_application(ERRORS::FAILED_TO_CREATE_WINDOW_SURFACE);
        return false;
    }

    bool createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        for (int i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = swapChainImages.at(i);

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
            if(vkCreateImageView(device, &create_info, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                quit_application(ERRORS::FAILED_TO_CREATE_IMAGE_VIEWS);
                return false;
            }
        } 
        return true;
    }

    VkShaderModule createShaderModule(const uint32_t * shader_code, uint32_t code_size)
    {
        VkShaderModuleCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = code_size;
        create_info.pCode = shader_code;
        VkShaderModule shader_module;
        if(vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS)
        {
            quit_application(ERRORS::FAILED_TO_CREATE_SHADER_MODULE);
        }
        return shader_module;
    }

    bool createGraphicsPipeline() {
        auto vertex_module = createShaderModule(triangle_vert, sizeof(triangle_vert));
        auto frag_module = createShaderModule(fill_triangle_frag, sizeof(fill_triangle_frag));

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
        viewport.width = (float) extent_2d_.width;
        viewport.height = (float) extent_2d_.height;
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

        if(vkCreatePipelineLayout(device, &layout_create_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
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

        graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE; // Optional
        graphics_pipeline_create_info.basePipelineIndex = -1; // Optional

        if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &graphics_pipeline_create_info, nullptr, &pipeline_) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_GRAPHICS_PIPELINE);
            return false;
        }

        vkDestroyShaderModule(device, vertex_module, nullptr);
        vkDestroyShaderModule(device, frag_module, nullptr);

        return true;
    }

    bool createRenderPass() {
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

        if(vkCreateRenderPass(device, &render_pass_create_info, nullptr, &render_pass_) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_RENDER_PASS);
            return false;
        }

        return true;
    }

    bool initVulkan() {
        return
            createInstance() &&
            setupDebugCallback() &&
            createSurface() &&
            pickPhysicalDevice() &&
            createLogicalDevice() &&
            createSwapChain() &&
            createImageViews() &&
            createRenderPass() &&
            createGraphicsPipeline();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if constexpr (ENABLE_VALIDATION_LAYERS) DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);

        vkDestroyPipeline(device, pipeline_, nullptr);
        vkDestroyPipelineLayout(device, pipeline_layout_, nullptr);
        vkDestroyRenderPass(device, render_pass_, nullptr);

        for (auto swap_chain_image_view : swapChainImageViews) {
            vkDestroyImageView(device, swap_chain_image_view, nullptr);
        } 

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyDevice(device, nullptr);

        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);

        glfwTerminate();
    }

    struct QueueFamilyIndices
    {
        int graphicsAndPresentFamily = -1;

        bool isComplete() const {
            return graphicsAndPresentFamily >= 0;
        }
    };

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // Note that it’s very likely that these end up being the same queue family after all, but throughout the program we will treat them as if they were separate queues for a uniform approach.
            // Nevertheless, you could add logic to explicitly prefer a physical device that supports drawing and presentation in the same queue for improved performance
            // [AP] I've decided to go with later approach, only using device that is supporting both
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport) {
                indices.graphicsAndPresentFamily = i;
            }

            if (indices.isComplete()) break;

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

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());

        return details;
    }    

    VkInstance instance;
    VkDebugUtilsMessengerEXT callback;
    VkSurfaceKHR surface;
    
    VkPhysicalDevice physicalDevice = nullptr;
    VkSwapchainKHR swapchain;

    GLFWwindow* window = nullptr;
    VkDevice device;
    VkQueue graphicsQueue;

    std::vector<VkImage> swapChainImages;
    VkFormat format_;
    VkExtent2D extent_2d_;

    VkRenderPass render_pass_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;

    std::vector<VkImageView> swapChainImageViews;

    constexpr static int WIDTH = 800;
    constexpr static int HEIGHT = 600;
};

int main() {
    HelloTriangleApplication application;
    application.run();
}
