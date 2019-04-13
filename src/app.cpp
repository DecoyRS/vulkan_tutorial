#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <iostream>
#include "debug_utils.hpp"

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

    enum class ERRORS : uint32_t
    {
        _START = 0xdead0000,
        REQUESTED_VALIDATION_LAYERS_ARE_NOT_AVAILABLE
    };

    bool quit_application(ERRORS error)
    {
        exit(static_cast<uint32_t>(error));
    }

    std::vector<const char*> get_required_extensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if(ENABLE_VALIDATION_LAYERS) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }

    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if(func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pCallback);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT callback, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if(func != nullptr) func(instance, callback, pAllocator);
    }
}

class HelloTriangleApplication
{
public:

    void run()
    {
        initWindow();
        if(!initVulkan()) return;
        mainLoop();
        cleanup();
    }
private:
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for(const auto layerName : validationLayers)
        {
            bool layerFound = false;

            for(const auto& layerProperties : availableLayers)
            {
                if(strcmp(layerName, layerProperties.layerName))
                {
                    layerFound = true;
                    break;
                }
            }

            if(!layerFound) return false;
        }

        return true;
    }

    VkInstance instance;
    VkDebugUtilsMessengerEXT callback;

    bool createInstance()
    {
        if(ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport())
        {
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

        if(ENABLE_VALIDATION_LAYERS) {
            create_info.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            create_info.ppEnabledLayerNames = validationLayers.data();
        }
        else {
            create_info.enabledLayerCount = 0;    
        }
        
        VkResult result = vkCreateInstance(&create_info, nullptr, &instance);
        if(result != VK_SUCCESS)
        {
            return false;
        }

        uint32_t extension_count;
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
        std::vector<VkExtensionProperties> extensions(extension_count);
        vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions.data());

        for(auto & extension : extensions)
        {
            std::cout << "\t" << extension.extensionName << "\n";
        }

        return true;
    }

    bool setupDebugCallback() {
        if constexpr (!ENABLE_VALIDATION_LAYERS) return true;

        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugUtils::debugCallback;
        createInfo.pUserData = nullptr;

        if(CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) return false;

        return true;
    }

    bool initVulkan()
    {
        return
        createInstance() &&
        setupDebugCallback();
    }
    void mainLoop()
    {
        while(!glfwWindowShouldClose(window))
        {
            glfwPollEvents();
        }
    }
    void cleanup()
    {
        if constexpr(ENABLE_VALIDATION_LAYERS) DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);
        
        vkDestroyInstance(instance, nullptr);
        
        glfwDestroyWindow(window);

        glfwTerminate();
    }
    GLFWwindow* window = nullptr;
    constexpr static int WIDTH = 800;
    constexpr static int HEIGHT = 600;
};

int main()
{
    HelloTriangleApplication application;
    application.run();
}