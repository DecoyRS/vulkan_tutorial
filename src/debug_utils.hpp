#pragma once

#ifndef DEBUG_UTILS_HPP
#define DEBUG_UTILS_HPP
#include <vulkan/vulkan.h>
#include <vulkan/vk_platform.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace DebugUtils
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void * pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << "\n";
        return VK_FALSE;
    }
}

#endif DEBUG_UTILS_HPP