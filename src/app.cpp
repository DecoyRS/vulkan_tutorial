#include "debug_utils.hpp"

// shaders
#include <cstdint> // have to include this here to pass 'uint32_t' to shaders
#include "shader_bin/triangle_vert.hpp"
#include "shader_bin/fill_triangle_frag.hpp"
#include "shader_bin/sp_triangle_vert.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

#include <chrono>
#include <vector>
#include <iostream>
#include <unordered_set>
#include <algorithm>
#include <array>

namespace
{
#ifdef NDEBUG
    constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

    constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_LUNARG_standard_validation"
    };

    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    enum class ERRORS : uint32_t
    {
        // ReSharper disable once CppEnumeratorNeverUsed
        START = 0xdead0000,
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
        FAILED_TO_CREATE_COMMAND_POOL,
        FAILED_TO_ALLOCATE_COMMAND_BUFFERS,
        FAILED_TO_BEGIN_RECORDING_COMMAND_BUFFER,
        FAILED_TO_END_RECORDING_COMMAND_BUFFER,
        FAILED_TO_CREATE_SYNC_OBJECTS,
        FAILED_TO_SUBMIT_DRAW_COMMAND_BUFFER,
        FAILED_TO_ACQUIRE_NEXT_IMAGE,
        FAILED_TO_PRESENT_SWAP_CHAIN,
        FAILED_TO_FIND_SUITABLE_MEMORY_TYPE,
        FAILED_TO_ALLOCATE_VERTEX_BUFFER_MEMORY,
        FAILED_TO_CREATE_BUFFER,
        // ReSharper disable once CppEnumeratorNeverUsed
        END,
        FAILED_TO_CREATE_DESCRIPTOR_SET_LAYOUR,
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

    struct UniformBufferObject {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    struct Vertex
    {
        glm::vec2 position;
        glm::vec3 color;

        static VkVertexInputBindingDescription get_binding_description() {
            VkVertexInputBindingDescription binding_description = {};

            binding_description.binding = 0;
            binding_description.stride = sizeof(Vertex);
            binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            return binding_description;
        }

        static std::array<VkVertexInputAttributeDescription, 2> get_attribute_descriptions() {
            std::array<VkVertexInputAttributeDescription, 2> attribute_descriptions = {};

            attribute_descriptions[0].binding = 0;
            attribute_descriptions[0].location = 0;
            attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attribute_descriptions[0].offset = offsetof(Vertex, position);

            attribute_descriptions[1].binding = 0;
            attribute_descriptions[1].location = 1;
            attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribute_descriptions[1].offset = offsetof(Vertex, color);

            return attribute_descriptions;
        }
    };

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
    };

    const std::vector<uint16_t> indices = {
        0,1,2,2,3,0
    };
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
        
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, [](GLFWwindow* window, int width, int height) {
            const auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
            app->framebuffer_resized = true;
        });
    }

    bool check_validation_layer_support() {
        uint32_t layer_count;
        vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<VkLayerProperties> available_layers(layer_count);
        vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for (const auto layer_name : VALIDATION_LAYERS) {
            bool layer_found = false;

            for (const auto& layer_properties : available_layers) {
                if (strcmp(layer_name, layer_properties.layerName)) {
                    layer_found = true;
                    break;
                }
            }

            if (!layer_found) return false;
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

        create_info.enabledExtensionCount = uint32_t(instance_extensions.size());
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

        int width;
        int height;
        glfwGetFramebufferSize(window_, &width, &height);
        VkExtent2D extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

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
            swap_chain_extent_ = extent_2d;
            return true;
        }

        quit_application(ERRORS::FAILED_TO_CREATE_SWAP_CHAIN);
        return false;
    }

    bool recreate_swap_chain() {
        int width = 0;
        int height = 0;
        while(width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(device_);

        cleanup_swap_chain();

        return
            create_swap_chain() &&
            create_image_views() &&
            create_render_pass() &&
            create_graphics_pipeline() &&
            create_framebuffers() &&
            create_uniform_buffers() &&
            create_command_buffers();
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

        auto binding_description = Vertex::get_binding_description();
        auto attribute_description = Vertex::get_attribute_descriptions();        

        VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
        vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input_state_create_info.vertexBindingDescriptionCount = 1;
        vertex_input_state_create_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_state_create_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_description.size());
        vertex_input_state_create_info.pVertexAttributeDescriptions = attribute_description.data();

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
        input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(swap_chain_extent_.width);
        viewport.height = static_cast<float>(swap_chain_extent_.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swap_chain_extent_;

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
        
        VkSubpassDependency subpass_dependency = {};
        subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        subpass_dependency.dstSubpass = 0;
        subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.srcAccessMask = 0;
        subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        
        VkRenderPassCreateInfo render_pass_create_info = {};
        render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_create_info.attachmentCount = 1;
        render_pass_create_info.pAttachments = &attachment_description;
        render_pass_create_info.subpassCount = 1;
        render_pass_create_info.pSubpasses = &subpass_description;
        render_pass_create_info.dependencyCount = 1;
        render_pass_create_info.pDependencies = &subpass_dependency;

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
            framebuffer_create_info.width = swap_chain_extent_.width;
            framebuffer_create_info.height = swap_chain_extent_.height;
            framebuffer_create_info.layers = 1;

            if(vkCreateFramebuffer(device_, &framebuffer_create_info, nullptr, &swap_chain_framebuffers_[i]) != VK_SUCCESS) {
                quit_application(ERRORS::FAILED_TO_CREATE_FRAMEBUFFERS);
                return false;
            }
        }
        return true;
    }

    bool create_command_pool() {
        const auto indices = find_queue_families(physical_device_);

        VkCommandPoolCreateInfo command_pool_create_info = {};
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.queueFamilyIndex = indices.graphics_and_present_family;
        command_pool_create_info.flags = 0; // Optional

        if(vkCreateCommandPool(device_, &command_pool_create_info, nullptr, &command_pool_)) {
            quit_application(ERRORS::FAILED_TO_CREATE_COMMAND_POOL);
            return false;
        }
        
        return true;
    }

    bool create_command_buffers() {
        command_buffers_.resize(swap_chain_framebuffers_.size());

        VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
        command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.commandPool = command_pool_;
        // The 'level' parameter specifies if the allocated command buffers are primary or secondary command buffers.
        // • VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue for execution, but cannot be called from other command buffers.
        // • VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly, but can be called from primary command buffers
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandBufferCount = uint32_t(command_buffers_.size());

        if(vkAllocateCommandBuffers(device_, &command_buffer_allocate_info, command_buffers_.data()) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_ALLOCATE_COMMAND_BUFFERS);
            return false;
        }

        for (int i = 0; i < command_buffers_.size(); i++) {
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            // The 'flags' parameter specifies how we’re going to use the command buffer. The following values are available:
            // • VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will be rerecorded right after executing it once.
            // • VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary command buffer that will be entirely within a single render pass.
            // • VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can be resubmitted while it is also already pending execution
            begin_info.flags = 0; // Optional
            // The pInheritanceInfo parameter is only relevant for secondary command buffers. It specifies which state to inherit from the calling primary command buffers.
            begin_info.pInheritanceInfo = nullptr; // Optional
            if(vkBeginCommandBuffer(command_buffers_[i], &begin_info) != VK_SUCCESS) {
                quit_application(ERRORS::FAILED_TO_BEGIN_RECORDING_COMMAND_BUFFER);
                return false;
            }

            VkRenderPassBeginInfo render_pass_begin_info = {};
            render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            render_pass_begin_info.renderPass = render_pass_;
            render_pass_begin_info.framebuffer = swap_chain_framebuffers_[i];
            render_pass_begin_info.renderArea.offset = {0, 0};
            render_pass_begin_info.renderArea.extent = swap_chain_extent_;
            // Black with 100% opacity
            VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
            render_pass_begin_info.clearValueCount = 1;
            render_pass_begin_info.pClearValues = &clear_color;

            // • VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the primary command buffer itself and no secondary command buffers will be executed.
            // • VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands will be executed from secondary command buffers.
            vkCmdBeginRenderPass(command_buffers_[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

            VkBuffer vertex_buffers[] = {vertex_buffer_};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(command_buffers_[i], 0, 1, vertex_buffers, offsets);

            vkCmdBindIndexBuffer(command_buffers_[i], index_buffer_, 0, VK_INDEX_TYPE_UINT16);
            
            vkCmdDrawIndexed(command_buffers_[i], uint32_t(indices.size()), 1, 0, 0, 0);
            
            vkCmdEndRenderPass(command_buffers_[i]);

            if(vkEndCommandBuffer(command_buffers_[i]) != VK_SUCCESS) {
                quit_application(ERRORS::FAILED_TO_END_RECORDING_COMMAND_BUFFER);
                return false;
            }
        }
        
        return true;
    }

    bool create_sync_objects() {
        image_available_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        render_finished_semaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        fences_.resize(MAX_FRAMES_IN_FLIGHT);
        images_in_flight_.resize(swap_chain_images_.size(), nullptr);
        
        VkSemaphoreCreateInfo semaphore_create_info = {};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_create_info = {};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++){
            if(vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &image_available_semaphores_[i]) != VK_SUCCESS ||
               vkCreateSemaphore(device_, &semaphore_create_info, nullptr, &render_finished_semaphores_[i]) != VK_SUCCESS ||
               vkCreateFence(device_, &fence_create_info, nullptr, &fences_[i])
               ) {
                quit_application(ERRORS::FAILED_TO_CREATE_SYNC_OBJECTS);
                return false;
            }
        }
        return true;
    }

    bool create_buffer(const VkDeviceSize size, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkBuffer &buffer, VkDeviceMemory& device_memory) {     
        VkBufferCreateInfo buffer_create_info = {};
        buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_create_info.size = size;
        buffer_create_info.usage = usage_flags;
        buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        if(vkCreateBuffer(device_, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_BUFFER);
            return false;
        }

        VkMemoryRequirements memory_requirements;
        vkGetBufferMemoryRequirements(device_, buffer, &memory_requirements);

        VkMemoryAllocateInfo memory_allocate_info = {};
        memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory_allocate_info.allocationSize = memory_requirements.size;
        memory_allocate_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, property_flags);

        if(vkAllocateMemory(device_, &memory_allocate_info, nullptr, &device_memory) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_ALLOCATE_VERTEX_BUFFER_MEMORY);
            return false;
        }

        vkBindBufferMemory(device_, buffer, device_memory, 0);
        return true;
    }

    bool create_vertex_buffer() {
        const VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.size();

        VkBuffer staging_buffer;
        VkDeviceMemory staging_device_memory;
        if(!create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_device_memory))
                return false;
        
        void* data;
        vkMapMemory(device_, staging_device_memory, 0, buffer_size, 0, &data);
        memcpy(data, vertices.data(), buffer_size);
        vkUnmapMemory(device_, staging_device_memory);
        
        if(!create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertex_buffer_, vertex_device_memory_))
                return false;

        copy_buffer(staging_buffer, vertex_buffer_, buffer_size);
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_device_memory, nullptr);
        return true;
    }

    void copy_buffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
        VkCommandBufferAllocateInfo command_buffer_allocate_info = {};
    	command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command_buffer_allocate_info.commandPool = command_pool_;
        command_buffer_allocate_info.commandBufferCount = 1;

        VkCommandBuffer command_buffer;
        vkAllocateCommandBuffers(device_, &command_buffer_allocate_info, &command_buffer);

        VkCommandBufferBeginInfo command_buffer_begin_info = {};
        command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);

        VkBufferCopy copy_region = {};
        copy_region.srcOffset = 0; // Optional
        copy_region.dstOffset = 0; // Optional
        copy_region.size = size;
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

        vkEndCommandBuffer(command_buffer);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vkQueueSubmit(graphics_queue_, 1, &submit_info, nullptr);
        vkQueueWaitIdle(graphics_queue_);

        vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
    }

    uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags property_flags) {
        VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
        vkGetPhysicalDeviceMemoryProperties(physical_device_, &physical_device_memory_properties);

        for(uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++) {
            if( type_filter & (1 << i) &&
                (physical_device_memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags
                ) {
                return i;
            }
        }
        quit_application(ERRORS::FAILED_TO_FIND_SUITABLE_MEMORY_TYPE);
        return 0;
    }

    bool create_index_buffer() {
        VkDeviceSize buffer_size = sizeof(indices[0])*indices.size();

        VkBuffer staging_buffer;
        VkDeviceMemory staging_device_memory;
        if(!create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buffer, staging_device_memory))
                return false;

        void* data;
        vkMapMemory(device_, staging_device_memory, 0, buffer_size, 0, &data);
        memcpy(data, indices.data(), buffer_size);
        vkUnmapMemory(device_, staging_device_memory);

        if(!create_buffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_device_memory_))
                return false;
        copy_buffer(staging_buffer, index_buffer_, buffer_size);
        
        vkDestroyBuffer(device_, staging_buffer, nullptr);
        vkFreeMemory(device_, staging_device_memory, nullptr);
        return true;
    }

    bool create_descriptor_set_layout() {
        VkDescriptorSetLayoutBinding ubo_descriptor_set_layout_binding = {};
        ubo_descriptor_set_layout_binding.binding = 0;
        ubo_descriptor_set_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo_descriptor_set_layout_binding.descriptorCount = 1;
        ubo_descriptor_set_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        ubo_descriptor_set_layout_binding.pImmutableSamplers = nullptr; // Optional

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {};
        descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptor_set_layout_create_info.bindingCount = 1;
        descriptor_set_layout_create_info.pBindings = &ubo_descriptor_set_layout_binding;

        if(vkCreateDescriptorSetLayout(device_, &descriptor_set_layout_create_info, nullptr, &descriptor_set_layout_) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_CREATE_DESCRIPTOR_SET_LAYOUR);
            return false;
        }

        VkPipelineLayoutCreateInfo pipeline_layout_create_info = {};
        pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &descriptor_set_layout_;
        return true;
    }

    bool create_uniform_buffers() {
        const VkDeviceSize buffer_size = sizeof(UniformBufferObject);

        uniform_buffers_.resize(swap_chain_images_.size());
        uniform_device_memory_.resize(swap_chain_images_.size());

        for(size_t i = 0; i < swap_chain_images_.size(); i++) {
            create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniform_buffers_[i],
                uniform_device_memory_[i]);
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
            create_descriptor_set_layout() &&
            create_graphics_pipeline() &&
            create_framebuffers() &&
            create_command_pool() &&
            create_vertex_buffer() &&
            create_index_buffer() &&
            create_uniform_buffers() &&
            create_command_buffers() &&
            create_sync_objects();
    }

    void update_uniform_buffer(uint32_t image_index) {
        static auto start_time = std::chrono::high_resolution_clock::now();

        const auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        UniformBufferObject ubo = {};
        ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
        ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
        ubo.proj = glm::perspective(glm::radians(45.f), swap_chain_extent_.width/float(swap_chain_extent_.height), 0.1f, 10.f);
        ubo.proj[1][1] *= -1;

        void* data;
        vkMapMemory(device_, uniform_device_memory_[current_frame], 0, sizeof(ubo),0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device_, uniform_device_memory_[current_frame]);
        
    }

    void draw_frame() {
        vkWaitForFences(device_, 1, &fences_[current_frame], VK_TRUE, std::numeric_limits<uint64_t>::max());        
        
        uint32_t image_index;
        // The last parameter specifies a variable to output the index of the swap chain image that has become available. The index refers to the VkImage in our swapChainImages array. We’re going to use that index to pick the right command buffer
        VkResult result = vkAcquireNextImageKHR(device_, swapchain_, std::numeric_limits<uint64_t>::max(), image_available_semaphores_[current_frame], nullptr, &image_index);
        if(result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreate_swap_chain();
            return;
        } else if(result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            quit_application(ERRORS::FAILED_TO_ACQUIRE_NEXT_IMAGE);
            return;
        }

        // Check if a previous frame is using this image (i.e. there is its fence to wait on)
        if(images_in_flight_[image_index] != nullptr) {
            vkWaitForFences(device_, 1, &images_in_flight_[image_index], VK_TRUE, std::numeric_limits<uint64_t>::max());
        }

        // Mark the image as now being in use by this frame
        images_in_flight_[image_index] = fences_[current_frame];

        update_uniform_buffer(image_index);

        VkSubmitInfo submit_info = {};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore wait_semaphores[] = {image_available_semaphores_[current_frame]};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffers_[image_index];

        VkSemaphore signal_semaphores[] = {render_finished_semaphores_[current_frame]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        vkResetFences(device_, 1, &fences_[current_frame]);
        
        if(vkQueueSubmit(graphics_queue_, 1, &submit_info, fences_[current_frame]) != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_SUBMIT_DRAW_COMMAND_BUFFER);
        }

        VkPresentInfoKHR present_info = {};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        VkSwapchainKHR swapchains[] = {swapchain_};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        
        present_info.pImageIndices = &image_index;
        present_info.pResults = nullptr; // Optional
        result = vkQueuePresentKHR(graphics_queue_, &present_info);
        if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebuffer_resized) {
            framebuffer_resized = false;
            recreate_swap_chain();
        } else if (result != VK_SUCCESS) {
            quit_application(ERRORS::FAILED_TO_PRESENT_SWAP_CHAIN);
            return;
        }

        current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void main_loop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            draw_frame();
        }
        vkDeviceWaitIdle(device_);
    }


    void cleanup_swap_chain()
    {
        for(size_t i = 0; i < swap_chain_images_.size(); i++) {
            vkDestroyBuffer(device_, uniform_buffers_[i], nullptr);
            vkFreeMemory(device_, uniform_device_memory_[i], nullptr);
        }
        for (auto swap_chain_framebuffer : swap_chain_framebuffers_) {
            vkDestroyFramebuffer(device_, swap_chain_framebuffer, nullptr);
        }

        vkFreeCommandBuffers(device_, command_pool_, static_cast<uint32_t>(command_buffers_.size()), command_buffers_.data());

        vkDestroyPipeline(device_, pipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
        vkDestroyRenderPass(device_, render_pass_, nullptr);

        for (auto swap_chain_image_view : swap_chain_image_views_) {
            vkDestroyImageView(device_, swap_chain_image_view, nullptr);
        }
        
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    }

    void cleanup() {        
        cleanup_swap_chain();

        vkDestroyDescriptorSetLayout(device_, descriptor_set_layout_, nullptr);
        
        vkDestroyBuffer(device_, vertex_buffer_, nullptr);
        vkFreeMemory(device_, vertex_device_memory_, nullptr);

        vkDestroyBuffer(device_, index_buffer_, nullptr);
        vkFreeMemory(device_, index_device_memory_, nullptr);

        for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
            vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
            vkDestroyFence(device_, fences_[i], nullptr);
        }
                    
        vkDestroyCommandPool(device_, command_pool_, nullptr);
        
        vkDestroyDevice(device_, nullptr);

        if constexpr (ENABLE_VALIDATION_LAYERS) DestroyDebugUtilsMessengerEXT(instance_, callback_, nullptr);

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
    VkExtent2D swap_chain_extent_;

    VkRenderPass render_pass_;
    VkDescriptorSetLayout descriptor_set_layout_;
    VkPipelineLayout pipeline_layout_;
    VkPipeline pipeline_;
    VkCommandPool command_pool_;

    std::vector<VkImageView> swap_chain_image_views_;
    std::vector<VkFramebuffer> swap_chain_framebuffers_;
    std::vector<VkCommandBuffer> command_buffers_;

    std::vector<VkSemaphore> image_available_semaphores_;
    std::vector<VkSemaphore> render_finished_semaphores_;
    std::vector<VkFence> fences_;
    std::vector<VkFence> images_in_flight_;
    VkBuffer vertex_buffer_;
    VkDeviceMemory vertex_device_memory_;
    VkBuffer index_buffer_;
    VkDeviceMemory index_device_memory_;
    std::vector<VkBuffer> uniform_buffers_;
    std::vector<VkDeviceMemory> uniform_device_memory_;

    size_t current_frame = 0;

    bool framebuffer_resized = false;
    
    constexpr static int WIDTH = 800;
    constexpr static int HEIGHT = 600;
};

int main() {
    HelloTriangleApplication application;
    application.run();
}
