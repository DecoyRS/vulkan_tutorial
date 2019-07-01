#include "debug_utils.hpp"

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
        FAILED_TO_CREATE_SWAP_CHAIN
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

    bool initVulkan() {
        return
            createInstance() &&
            setupDebugCallback() &&
            createSurface() &&
            pickPhysicalDevice() &&
            createLogicalDevice() &&
            createSwapChain();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if constexpr (ENABLE_VALIDATION_LAYERS) DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);

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

    constexpr static int WIDTH = 800;
    constexpr static int HEIGHT = 600;
};

int main() {
    HelloTriangleApplication application;
    application.run();
}
